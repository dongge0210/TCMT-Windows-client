using System;
using System.IO;
using System.IO.Pipes;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Serilog;

namespace AvaloniaUI.Services.IPC;

/// <summary>
/// Pipe 客户端：连接 C++ 端的 Named Pipe(Windows) / Unix Domain Socket(macOS/Linux)
/// 接收 Schema 消息并触发回调。
/// </summary>
public class IPCPipeClient : IAsyncDisposable
{
    private CancellationTokenSource? _cts;
    private bool _disposed;
    private bool _schemaReceived;

    /// <summary>
    /// 收到 Schema 时的回调。返回 true 表示接受，false 表示拒绝（版本不匹配等）
    /// </summary>
    public Func<SchemaMessage, bool>? OnSchemaReceived { get; set; }

    /// <summary>
    /// 连接状态变化回调
    /// </summary>
    public Action<bool, string>? OnConnectionChanged { get; set; }

    public bool IsConnected { get; private set; }
    public string LastError { get; private set; } = string.Empty;

    public async Task StartAsync(CancellationToken ct = default)
    {
        if (_disposed) return;
        _cts = CancellationTokenSource.CreateLinkedTokenSource(ct);

        while (!_cts.Token.IsCancellationRequested)
        {
            if (_schemaReceived) break;

            try
            {
                if (OperatingSystem.IsWindows())
                    await ConnectWindowsAsync(_cts.Token);
                else
                    await ConnectUnixAsync(_cts.Token);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                LastError = ex.Message;
                Log.Warning(ex, "Pipe connection lost, reconnecting in 1s...");
                IsConnected = false;
                OnConnectionChanged?.Invoke(false, LastError);
                await Task.Delay(1000, _cts.Token);
            }
        }
    }

    // --- Windows: Named Pipe ---
    private async Task ConnectWindowsAsync(CancellationToken ct)
    {
        var pipeName = IPCConstants.PipeName;
        Log.Debug("IPC: Connecting to Windows pipe: {Name}", pipeName);

        using var client = new NamedPipeClientStream(
            ".", pipeName,
            PipeDirection.InOut,
            PipeOptions.Asynchronous);

        await client.ConnectAsync(5000, ct);

        IsConnected = true;
        OnConnectionChanged?.Invoke(true, "已连接");
        Log.Information("IPC: Connected to Windows pipe: {Name}", pipeName);

        await ReceiveLoopAsync(client, ct);
    }

    // --- macOS/Linux: Unix Domain Socket ---
    private async Task ConnectUnixAsync(CancellationToken ct)
    {
        var socketPath = IPCConstants.UnixSocketPath;
        Log.Debug("IPC: Connecting to Unix socket: {Path}", socketPath);

        while (!ct.IsCancellationRequested)
        {
            try
            {
                using var client = new Socket(AddressFamily.Unix, SocketType.Stream, ProtocolType.Unspecified);
                var ep = new UnixDomainSocketEndPoint(socketPath);
                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(TimeSpan.FromSeconds(5));

                await client.ConnectAsync(ep, cts.Token);

                IsConnected = true;
                OnConnectionChanged?.Invoke(true, "已连接");
                Log.Information("IPC: Connected to Unix socket: {Path}", socketPath);

                await using var ns = new NetworkStream(client, ownsSocket: false);
                await ReceiveLoopAsync(ns, ct);
            }
            catch (OperationCanceledException) when (!ct.IsCancellationRequested)
            {
                await Task.Delay(500, ct);
            }
            catch (Exception ex)
            {
                Log.Debug("Unix socket connect failed: {Msg}, retrying...", ex.Message);
                IsConnected = false;
                OnConnectionChanged?.Invoke(false, ex.Message);
                await Task.Delay(1000, ct);
            }
        }
    }

    private async Task ReceiveLoopAsync(Stream stream, CancellationToken ct)
    {
        int headerSize = IPCConstants.SchemaHeaderSize;
        int maxFields = IPCConstants.MaxFields;
        int fieldDefSize = IPCConstants.FieldDefSize;

        var headerBuf = new byte[headerSize];
        var fieldBuf = new byte[maxFields * fieldDefSize];

        while (!ct.IsCancellationRequested)
        {
            try
            {
                int n = await ReadFullAsync(stream, headerBuf, headerSize, ct);
                if (n == 0) break;

                ushort fieldCount = BitConverter.ToUInt16(headerBuf, 6);
                uint totalSize = BitConverter.ToUInt32(headerBuf, 8);

                int fieldsSize = Math.Min(fieldCount, maxFields) * fieldDefSize;
                if (fieldsSize > 0)
                {
                    await ReadFullAsync(stream, fieldBuf, fieldsSize, ct);
                }

                var raw = new byte[headerSize + fieldsSize];
                Buffer.BlockCopy(headerBuf, 0, raw, 0, headerSize);
                if (fieldsSize > 0)
                    Buffer.BlockCopy(fieldBuf, 0, raw, headerSize, fieldsSize);

                var schema = SchemaMessage.Parse(raw);

                if (schema.Header.Magic != IPCConstants.Magic)
                {
                    Log.Warning("IPC: Invalid magic: 0x{Actual:X}, expected 0x{Expected:X}", schema.Header.Magic, IPCConstants.Magic);
                    continue;
                }

                if (schema.Header.Version != IPCConstants.CurrentVersion)
                {
                    Log.Warning("IPC: Schema version mismatch: {Actual} vs {Expected}", schema.Header.Version, IPCConstants.CurrentVersion);
                    OnConnectionChanged?.Invoke(false, $"Schema 版本不匹配: {schema.Header.Version} vs {IPCConstants.CurrentVersion}");
                    continue;
                }

                Log.Information("IPC: Schema received: {Count} fields, totalSize={Size}", schema.Header.FieldCount, schema.Header.TotalSize);
                foreach (var f in schema.Fields)
                    Log.Debug("  - [{Id}] {Name}: {Type}@{Offset}[{Size}] {Units}", f.Id, f.Name, f.Type, f.Offset, f.Size, f.Units);

                bool accepted = OnSchemaReceived?.Invoke(schema) ?? true;
                if (!accepted)
                {
                    Log.Warning("IPC: Schema rejected by handler");
                    OnConnectionChanged?.Invoke(false, "Schema 被拒绝");
                }
                else
                {
                    var ack = new byte[] { 4, 0, 0, 1, schema.Header.Version, 0, 0, 0 };
                    await stream.WriteAsync(ack, ct);
                    await stream.FlushAsync(ct);
                }
                // Schema received — done with pipe. Data flows via shared memory.
                _schemaReceived = true;
                break;
            }
            catch (IOException) { break; }
            catch (OperationCanceledException) { break; }
        }

        // Pipe disconnection after schema handshake is expected.
        Log.Debug("IPC: Pipe disconnected, schema handshake complete — data now via shared memory");
    }

    private static async Task<int> ReadFullAsync(Stream stream, byte[] buf, int len, CancellationToken ct)
    {
        int offset = 0;
        while (offset < len)
        {
            int n = await stream.ReadAsync(buf.AsMemory(offset, len - offset), ct);
            if (n == 0) return offset;
            offset += n;
        }
        return offset;
    }

    public async ValueTask DisposeAsync()
    {
        if (_disposed) return;
        _disposed = true;
        _cts?.Cancel();
        _cts?.Dispose();
    }
}

// IPCMemoryReader.cs - 基于 Schema 动态读取共享内存
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Text;
using Serilog;

namespace AvaloniaUI.Services.IPC;

public class IPCMemoryReader : IDisposable
{
    // macOS: file-based
    private FileStream? _shmStream;
    private Memory<byte> _shmView;

    // Windows: MemoryMappedFile
    private MemoryMappedFile? _mmf;
    private MemoryMappedViewAccessor? _accessor;

    private SchemaMessage? _schema;
    private bool _disposed;
    private readonly object _lock = new();

    public bool IsOpen => OperatingSystem.IsWindows() ? _accessor != null : _shmStream != null;
    public uint TotalSize => _schema?.Header.TotalSize ?? 0;

    public bool Open(SchemaMessage schema)
    {
        lock (_lock)
        {
            try
            {
                Close();
                _schema = schema;

                if (_schema.Header.TotalSize == 0)
                {
                    Log.Error("IPC Memory: Schema totalSize is 0");
                    return false;
                }

                if (OperatingSystem.IsWindows())
                    return OpenWindows();

                return OpenMacOS();
            }
            catch (Exception ex)
            {
                Log.Error(ex, "IPC Memory: Failed to open shared memory");
                return false;
            }
        }
    }

    private bool OpenWindows()
    {
        string[] names =
        {
            "Global\\SystemMonitorSharedMemory",
            "Local\\SystemMonitorSharedMemory",
            "SystemMonitorSharedMemory"
        };

        foreach (string name in names)
        {
            try
            {
#pragma warning disable CA1416 // validated by caller (OpenWindows called only on Windows)
                _mmf = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
#pragma warning restore CA1416
                _accessor = _mmf.CreateViewAccessor(0, _schema!.Header.TotalSize, MemoryMappedFileAccess.Read);
                Log.Information("IPC Memory: Connected to Windows shared memory: {Name}", name);
                return true;
            }
            catch (FileNotFoundException)
            {
                Log.Debug("IPC Memory: Shared memory not found: {Name}", name);
                continue;
            }
            catch (Exception ex)
            {
                Log.Warning("IPC Memory: Failed to open {Name}: {Message}", name, ex.Message);
                continue;
            }
        }

        Log.Error("IPC Memory: Cannot find any Windows shared memory segment");
        return false;
    }

    private bool OpenMacOS()
    {
        string path = IPCConstants.SharedMemoryPath;
        if (!File.Exists(path))
        {
            Log.Warning("IPC Memory: Shared memory file not found: {Path}", path);
            return false;
        }

        _shmStream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        if (_shmStream.Length < _schema!.Header.TotalSize)
        {
            Log.Warning("IPC Memory: File too small: {Len} < {Expected}",
                _shmStream.Length, _schema.Header.TotalSize);
        }

        _shmView = new byte[Math.Min(_shmStream.Length, (long)_schema.Header.TotalSize)];
        return true;
    }

    /// <summary>
    /// 刷新读取（每次 UI 刷新前调用）
    /// On Windows MemoryMappedViewAccessor always reflects the latest data (no-op).
    /// </summary>
    public void Refresh()
    {
        if (_schema == null) return;
        lock (_lock)
        {
            try
            {
                if (OperatingSystem.IsWindows())
                {
                    // MemoryMappedViewAccessor always reflects latest content
                    return;
                }

                // macOS: re-read from backing file
                if (_shmStream == null) return;
                _shmStream.Seek(0, SeekOrigin.Begin);
                var buf = _shmView.Span;
                int totalRead = 0;
                while (totalRead < buf.Length)
                {
                    int n = _shmStream.Read(buf.Slice(totalRead));
                    if (n == 0) break;
                    totalRead += n;
                }
            }
            catch (Exception ex)
            {
                Log.Debug("IPC Memory: Refresh failed: {Msg}", ex.Message);
            }
        }
    }

    public bool HasField(string name)
    {
        if (_schema == null) return false;
        foreach (var f in _schema.Fields)
        {
            if (f.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                return true;
        }
        return false;
    }

    // --- 基础类型读取 ---

    public byte? ReadUInt8(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadByte((int)field.Offset);
        return _shmView.Span[(int)field.Offset];
    }

    public ushort? ReadUInt16(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt16((int)field.Offset);
        // little-endian
        return (ushort)(_shmView.Span[(int)field.Offset]
                      | (_shmView.Span[(int)field.Offset + 1] << 8));
    }

    public uint? ReadUInt32(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt32((int)field.Offset);
        // little-endian
        var s = _shmView.Span.Slice((int)field.Offset);
        return s[0] | ((uint)s[1] << 8) | ((uint)s[2] << 16) | ((uint)s[3] << 24);
    }

    public ulong? ReadUInt64(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt64((int)field.Offset);
        // little-endian
        var s = _shmView.Span.Slice((int)field.Offset);
        return s[0] | ((ulong)s[1] << 8) | ((ulong)s[2] << 16) | ((ulong)s[3] << 24)
             | ((ulong)s[4] << 32) | ((ulong)s[5] << 40) | ((ulong)s[6] << 48) | ((ulong)s[7] << 56);
    }

    public sbyte? ReadInt8(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadSByte((int)field.Offset);
        return (sbyte)_shmView.Span[(int)field.Offset];
    }

    public short? ReadInt16(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        return (short?)ReadUInt16(fieldName);
    }

    public int? ReadInt32(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        return (int?)ReadUInt32(fieldName);
    }

    public long? ReadInt64(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        return (long?)ReadUInt64(fieldName);
    }

    public float? ReadFloat32(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        uint bits = ReadUInt32(fieldName) ?? 0;
        return BitConverter.Int32BitsToSingle((int)bits);
    }

    public double? ReadFloat64(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        ulong bits = ReadUInt64(fieldName) ?? 0;
        return BitConverter.Int64BitsToDouble((long)bits);
    }

    public bool? ReadBool(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadByte((int)field.Offset) != 0;
        return _shmView.Span[(int)field.Offset] != 0;
    }

    public string? ReadWString(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;

        if (OperatingSystem.IsWindows() && _accessor != null)
        {
            int maxBytes = (int)Math.Min(field.Size, _schema!.Header.TotalSize - field.Offset);
            if (maxBytes <= 0) return null;
            var buf = new byte[maxBytes];
            _accessor.ReadArray((int)field.Offset, buf, 0, maxBytes);
            int len = 0;
            while (len + 1 < maxBytes && (buf[len] != 0 || buf[len + 1] != 0)) len += 2;
            if (len == 0) return string.Empty;
            return Encoding.Unicode.GetString(buf, 0, len);
        }

        // macOS: WString not used (IPCDataBlock uses char[], not WCHAR)
        // Fall back to ASCII read for safety
        return ReadString(fieldName);
    }

    public string? ReadString(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;

        if (OperatingSystem.IsWindows() && _accessor != null)
        {
            int maxLen = (int)Math.Min(field.Size, _schema!.Header.TotalSize - field.Offset);
            if (maxLen <= 0) return null;
            var buf = new byte[maxLen];
            _accessor.ReadArray((int)field.Offset, buf, 0, maxLen);
            int len = 0;
            while (len < maxLen && buf[len] != 0) len++;
            if (len == 0) return string.Empty;
            return Encoding.ASCII.GetString(buf, 0, len);
        }

        int mLen = (int)Math.Min(field.Size, _shmView.Length - field.Offset);
        if (mLen <= 0) return null;

        int l = 0;
        while (l < mLen && _shmView.Span[(int)field.Offset + l] != 0) l++;
        if (l == 0) return string.Empty;

        return Encoding.ASCII.GetString(
            _shmView.Span.Slice((int)field.Offset, l));
    }

    // --- 按 FieldDef 读取 ---
    public object? ReadField(FieldDef field)
    {
        return (FieldType)field.Type switch
        {
            FieldType.UInt8   => ReadUInt8(field.Name),
            FieldType.Int8    => ReadInt8(field.Name),
            FieldType.UInt16  => ReadUInt16(field.Name),
            FieldType.Int16   => ReadInt16(field.Name),
            FieldType.UInt32  => ReadUInt32(field.Name),
            FieldType.Int32   => ReadInt32(field.Name),
            FieldType.UInt64  => ReadUInt64(field.Name),
            FieldType.Int64   => ReadInt64(field.Name),
            FieldType.Float32 => ReadFloat32(field.Name),
            FieldType.Float64 => ReadFloat64(field.Name),
            FieldType.Bool    => ReadBool(field.Name),
            FieldType.String  => ReadString(field.Name),
            FieldType.WString => ReadWString(field.Name),
            _ => null
        };
    }

    public Dictionary<string, object?> ReadAllFields()
    {
        var result = new Dictionary<string, object?>();
        if (_schema == null) return result;
        foreach (var f in _schema.Fields)
            result[f.Name] = ReadField(f);
        return result;
    }

    private FieldDef? FindField(string name)
    {
        if (_schema == null) return null;
        foreach (var f in _schema.Fields)
            if (f.Name.Equals(name, StringComparison.OrdinalIgnoreCase))
                return f;
        return null;
    }

    public void Close()
    {
        lock (_lock)
        {
            // Windows cleanup
            _accessor?.Dispose();
            _mmf?.Dispose();
            _accessor = null;
            _mmf = null;

            // macOS cleanup
            _shmStream?.Dispose();
            _shmStream = null;
            _shmView = default;
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        Close();
    }
}

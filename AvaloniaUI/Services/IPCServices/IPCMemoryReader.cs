// IPCMemoryReader.cs - 基于 Schema 动态读取共享内存
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using Serilog;

namespace AvaloniaUI.Services.IPC;

public class IPCMemoryReader : IDisposable
{
    private FileStream? _shmStream;
    private Memory<byte> _shmView;
    private SchemaMessage? _schema;
    private bool _disposed;
    private readonly object _lock = new();

    public bool IsOpen => _shmStream != null;
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

                string path = IPCConstants.SharedMemoryPath;
                if (!File.Exists(path))
                {
                    // 文件还没创建，等 C++ 端
                    Log.Warning("IPC Memory: Shared memory file not found: {Path}", path);
                    return false;
                }

                _shmStream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                if (_shmStream.Length < _schema.Header.TotalSize)
                {
                    Log.Warning("IPC Memory: File too small: {Len} < {Expected}",
                        _shmStream.Length, _schema.Header.TotalSize);
                }

                _shmView = new byte[Math.Min(_shmStream.Length, (long)_schema.Header.TotalSize)];
                return true;
            }
            catch (Exception ex)
            {
                Log.Error(ex, "IPC Memory: Failed to open shared memory");
                return false;
            }
        }
    }

    /// <summary>
    /// 刷新读取（每次 UI 刷新前调用）
    /// </summary>
    public void Refresh()
    {
        if (_shmStream == null || _schema == null) return;
        lock (_lock)
        {
            try
            {
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
        return _shmView.Span[(int)field.Offset];
    }

    public ushort? ReadUInt16(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        return (ushort)((_shmView.Span[(int)field.Offset] << 8)
                      | _shmView.Span[(int)field.Offset + 1]);
    }

    public uint? ReadUInt32(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        var s = _shmView.Span.Slice((int)field.Offset);
        return ((uint)s[0] << 24) | ((uint)s[1] << 16) | ((uint)s[2] << 8) | s[3];
    }

    public ulong? ReadUInt64(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        var s = _shmView.Span.Slice((int)field.Offset);
        return ((ulong)s[0] << 56) | ((ulong)s[1] << 48) | ((ulong)s[2] << 40)
             | ((ulong)s[3] << 32) | ((ulong)s[4] << 24) | ((ulong)s[5] << 16)
             | ((ulong)s[6] << 8)  | s[7];
    }

    public sbyte? ReadInt8(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
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
        return BitConverter.SingleToUInt32Bits(bits);
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
        return _shmView.Span[(int)field.Offset] != 0;
    }

    public string? ReadString(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        int maxLen = (int)Math.Min(field.Size, _shmView.Length - field.Offset);
        if (maxLen <= 0) return null;

        int len = 0;
        while (len < maxLen && _shmView.Span[(int)field.Offset + len] != 0) len++;
        if (len == 0) return string.Empty;

        return Encoding.ASCII.GetString(
            _shmView.Span.Slice((int)field.Offset, len));
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

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
    // macOS: POSIX shared memory via shm_open/mmap
    private IntPtr _shmPtr = IntPtr.Zero;
    private int _shmFd = -1;
    private int _shmSize;

    // Windows: MemoryMappedFile
    private MemoryMappedFile? _mmf;
    private MemoryMappedViewAccessor? _accessor;

    private SchemaMessage? _schema;
    private bool _disposed;
    private readonly object _lock = new();

    // POSIX P/Invoke
    private const int O_RDONLY = 0;
    private const int PROT_READ = 1;
    private const int MAP_SHARED = 1;
    private static readonly IntPtr MAP_FAILED = new IntPtr(-1);

    [DllImport("libc", EntryPoint = "shm_open", SetLastError = true)]
    private static extern int shm_open(string name, int oflag, int mode);

    [DllImport("libc", EntryPoint = "mmap", SetLastError = true)]
    private static extern IntPtr mmap(IntPtr addr, IntPtr length, int prot, int flags, int fd, IntPtr offset);

    [DllImport("libc", EntryPoint = "munmap", SetLastError = true)]
    private static extern int munmap(IntPtr addr, IntPtr length);

    [DllImport("libc", EntryPoint = "close", SetLastError = true)]
    private static extern int close(int fd);

    public bool IsOpen => OperatingSystem.IsWindows() ? _accessor != null : _shmPtr != IntPtr.Zero;
    public uint TotalSize => _schema?.Header.TotalSize ?? 0;

    public bool Open(SchemaMessage schema)
    {
        if (_disposed) return false;
        lock (_lock)
        {
            try
            {
                _schema = schema;
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
#pragma warning disable CA1416
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
        string shmName = "/tcmt_ipc";
        _shmFd = shm_open(shmName, O_RDONLY, 0);
        if (_shmFd == -1)
        {
            int err = Marshal.GetLastWin32Error();
            Log.Error("IPC Memory: shm_open({Name}) failed, errno={Errno}", shmName, err);
            return false;
        }

        int totalSize = (int)_schema!.Header.TotalSize;
        if (totalSize <= 0) totalSize = 4096;

        _shmPtr = mmap(IntPtr.Zero, (IntPtr)totalSize, PROT_READ, MAP_SHARED, _shmFd, IntPtr.Zero);
        if (_shmPtr == MAP_FAILED)
        {
            int err = Marshal.GetLastWin32Error();
            Log.Error("IPC Memory: mmap failed, errno={Errno}", err);
            close(_shmFd);
            _shmFd = -1;
            return false;
        }

        _shmSize = totalSize;
        Log.Information("IPC Memory: Connected to macOS shared memory: {Name}, size={Size}", shmName, totalSize);
        return true;
    }

    public void Refresh() { } // mmap/MMF always reflect latest data

    // --- Typed readers ---

    public byte? ReadUInt8(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadByte((int)field.Offset);
        if (_shmPtr == IntPtr.Zero || (int)field.Offset >= _shmSize) return null;
        return Marshal.ReadByte(_shmPtr, (int)field.Offset);
    }

    public ushort? ReadUInt16(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (field.Size < 2) return null; // too small for UInt16
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt16((int)field.Offset);
        if (_shmPtr == IntPtr.Zero || (int)field.Offset + 2 > _shmSize) return null;
        byte b0 = Marshal.ReadByte(_shmPtr, (int)field.Offset);
        byte b1 = Marshal.ReadByte(_shmPtr, (int)field.Offset + 1);
        return (ushort)(b0 | (b1 << 8));
    }

    public uint? ReadUInt32(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (field.Size < 4) return null; // too small for UInt32
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt32((int)field.Offset);
        if (_shmPtr == IntPtr.Zero || (int)field.Offset + 4 > _shmSize) return null;
        byte b0 = Marshal.ReadByte(_shmPtr, (int)field.Offset);
        byte b1 = Marshal.ReadByte(_shmPtr, (int)field.Offset + 1);
        byte b2 = Marshal.ReadByte(_shmPtr, (int)field.Offset + 2);
        byte b3 = Marshal.ReadByte(_shmPtr, (int)field.Offset + 3);
        return (uint)(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    }

    public ulong? ReadUInt64(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (field.Size < 8) return null; // too small for UInt64
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadUInt64((int)field.Offset);
        if (_shmPtr == IntPtr.Zero || (int)field.Offset + 8 > _shmSize) return null;
        uint lo = ReadUInt32Le((int)field.Offset);
        uint hi = ReadUInt32Le((int)field.Offset + 4);
        return lo | ((ulong)hi << 32);
    }

    public sbyte? ReadInt8(string fieldName)
    {
        var field = FindField(fieldName); if (field == null) return null;
        if (OperatingSystem.IsWindows() && _accessor != null)
            return _accessor.ReadSByte((int)field.Offset);
        if (_shmPtr == IntPtr.Zero || (int)field.Offset >= _shmSize) return null;
        return (sbyte)Marshal.ReadByte(_shmPtr, (int)field.Offset);
    }

    public short? ReadInt16(string fieldName) => (short?)ReadUInt16(fieldName);
    public int? ReadInt32(string fieldName) => (int?)ReadUInt32(fieldName);
    public long? ReadInt64(string fieldName) => (long?)ReadUInt64(fieldName);

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
        if (_shmPtr == IntPtr.Zero || (int)field.Offset >= _shmSize) return null;
        return Marshal.ReadByte(_shmPtr, (int)field.Offset) != 0;
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

        if (_shmPtr == IntPtr.Zero) return null;
        int mLen = (int)Math.Min(field.Size, _shmSize - field.Offset);
        if (mLen <= 0) return null;
        int l = 0;
        while (l < mLen && Marshal.ReadByte(_shmPtr, (int)field.Offset + l) != 0) l++;
        if (l == 0) return string.Empty;
        var bytes = new byte[l];
        Marshal.Copy(_shmPtr + (int)field.Offset, bytes, 0, l);
        return Encoding.ASCII.GetString(bytes);
    }

    public bool HasField(string name)
    {
        if (_schema == null) return false;
        foreach (var f in _schema.Fields)
            if (f.Name == name) return true;
        return false;
    }

    public Dictionary<string, object?> ReadAllFields()
    {
        var result = new Dictionary<string, object?>();
        if (_schema == null) return result;
        foreach (var f in _schema.Fields)
            result[f.Name] = ReadField(f);
        return result;
    }

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

    private uint ReadUInt32Le(int offset)
    {
        byte b0 = Marshal.ReadByte(_shmPtr, offset);
        byte b1 = Marshal.ReadByte(_shmPtr, offset + 1);
        byte b2 = Marshal.ReadByte(_shmPtr, offset + 2);
        byte b3 = Marshal.ReadByte(_shmPtr, offset + 3);
        return (uint)(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    }

    private FieldDef? FindField(string fieldName)
    {
        if (_schema == null) return null;
        foreach (var f in _schema.Fields)
            if (f.Name == fieldName) return f;
        return null;
    }

    public void Dispose()
    {
        lock (_lock)
        {
            if (_disposed) return;
            _disposed = true;
            _accessor?.Dispose();
            _mmf?.Dispose();
            _accessor = null;
            _mmf = null;
            if (_shmPtr != IntPtr.Zero) { munmap(_shmPtr, _shmSize); _shmPtr = IntPtr.Zero; }
            if (_shmFd != -1) { close(_shmFd); _shmFd = -1; }
            _shmSize = 0;
        }
    }
}

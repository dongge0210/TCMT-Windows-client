// IPC Schema 定义 — 与 C++ src/core/IPC/IPCSchema.h 严格一致
namespace AvaloniaUI.Services.IPC;

public enum FieldType : byte
{
    UInt8   = 1,
    Int8    = 2,
    UInt16  = 3,
    Int16   = 4,
    UInt32  = 5,
    Int32   = 6,
    UInt64  = 7,
    Int64   = 8,
    Float32 = 9,
    Float64 = 10,
    Bool    = 11,
    String  = 12,
    WString = 13,
}

public static class IPCConstants
{
    public const uint   Magic             = 0x54434D54; // "TCMT"
    public const byte   CurrentVersion    = 1;
    public const int    MaxFields         = 64;
    public const int    FieldNameLen      = 32;
    public const int    FieldUnitsLen     = 16;
    public const int    SchemaHeaderSize  = 16;  // sizeof(SchemaHeader)
    public const int    FieldDefSize      = 80;  // sizeof(FieldDef)
    public const string PipeName          = "TCMT_IPC_Pipe";       // Windows
    public const string UnixSocketPath    = "/tmp/tcmt_ipc.sock";  // macOS/Linux
    public const string SharedMemoryPath  = "/tmp/tcmt_shm.dat";   // POSIX shm
}

public class SchemaHeader
{
    public uint Magic;              // 0x54434D54 "TCMT"
    public byte Version;            // 1
    public byte Flags;
    public ushort FieldCount;
    public uint TotalSize;          // 共享内存总大小
    public uint StringBlockSize;    // 字符串块大小

    public static SchemaHeader Parse(byte[] data, int offset)
    {
        return new SchemaHeader
        {
            Magic           = ReadUInt32LE(data, offset + 0),
            Version         = data[offset + 4],
            Flags           = data[offset + 5],
            FieldCount      = (ushort)(data[offset + 6] | (data[offset + 7] << 8)),
            TotalSize       = ReadUInt32LE(data, offset + 8),
            StringBlockSize = ReadUInt32LE(data, offset + 12)
        };
    }

    private static uint ReadUInt32LE(byte[] data, int offset)
    {
        return data[offset]
             | ((uint)data[offset + 1] << 8)
             | ((uint)data[offset + 2] << 16)
             | ((uint)data[offset + 3] << 24);
    }
}

public class FieldDef
{
    public uint   Id;       // 字段 ID
    public byte   Type;     // FieldType enum
    public byte   Reserved1;
    public ushort Size;     // 字节大小（string/wstring 时为 maxLen）
    public uint   Offset;   // 共享内存中的偏移量
    public uint   Count;    // 数组元素个数
    public uint   StrOffset;// 字符串实际偏移
    public uint   Flags;
    public float  MinVal;
    public float  MaxVal;
    public string Name = "";  // 字段名
    public string Units = ""; // 单位

    public static FieldDef FromBytes(byte[] data, int baseOffset)
    {
        var def = new FieldDef
        {
            Id         = ReadUInt32LE(data, baseOffset + 0),
            Type       = data[baseOffset + 4],
            Reserved1  = data[baseOffset + 5],
            Size       = (ushort)(data[baseOffset + 6] | (data[baseOffset + 7] << 8)),
            Offset     = ReadUInt32LE(data, baseOffset + 8),
            Count      = ReadUInt32LE(data, baseOffset + 12),
            StrOffset  = ReadUInt32LE(data, baseOffset + 16),
            Flags      = ReadUInt32LE(data, baseOffset + 20),
            MinVal     = ReadFloat(data, baseOffset + 24),
            MaxVal     = ReadFloat(data, baseOffset + 28),
            Name       = ReadString(data, baseOffset + 32, IPCConstants.FieldNameLen),
            Units      = ReadString(data, baseOffset + 64, IPCConstants.FieldUnitsLen)
        };
        return def;
    }

    private static uint ReadUInt32LE(byte[] data, int offset)
    {
        return data[offset]
             | ((uint)data[offset + 1] << 8)
             | ((uint)data[offset + 2] << 16)
             | ((uint)data[offset + 3] << 24);
    }

    private static float ReadFloat(byte[] data, int offset)
    {
        uint bits = ReadUInt32LE(data, offset);
        return BitConverter.UInt32BitsToSingle(bits);
    }

    private static string ReadString(byte[] data, int offset, int maxLen)
    {
        int len = 0;
        while (len < maxLen && offset + len < data.Length && data[offset + len] != 0) len++;
        return System.Text.Encoding.ASCII.GetString(data, offset, len);
    }
}

public class SchemaMessage
{
    public SchemaHeader Header = new();
    public List<FieldDef> Fields { get; } = new();

    public bool IsValid => Header.Magic == IPCConstants.Magic
                        && Header.Version == IPCConstants.CurrentVersion;

    public static SchemaMessage Parse(byte[] raw)
    {
        var msg = new SchemaMessage
        {
            Header = SchemaHeader.Parse(raw, 0)
        };

        int fieldCount = Math.Min((int)msg.Header.FieldCount, IPCConstants.MaxFields);
        for (int i = 0; i < fieldCount; i++)
        {
            int fieldOffset = IPCConstants.SchemaHeaderSize + i * IPCConstants.FieldDefSize;
            if (fieldOffset + IPCConstants.FieldDefSize > raw.Length) break;
            msg.Fields.Add(FieldDef.FromBytes(raw, fieldOffset));
        }
        return msg;
    }
}

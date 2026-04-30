// IpcCommandSender.cs - Frontend-to-backend command mailbox
// The C++ backend polls this struct from shared memory each main loop iteration.
// For now only the struct definition is provided; actual sending will be wired later.
using System.Runtime.InteropServices;

namespace AvaloniaUI.Services.IPC;

// Must match the C++ SharedMemoryBlock::IpcCommand layout (packed)
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct IpcCommand
{
    public uint Magic;       // must be 0x54434D54 for valid command
    public uint Id;          // monotonically increasing command ID
    public uint Type;        // IpcCommandType value
    public uint Param1;      // generic parameter
    public uint Param2;
    public uint Param3;
    public uint Param4;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
    public string Message;   // text parameter
    public uint Status;      // 0=pending, 1=done, 2=error
    public uint Result;      // result code
}

public enum IpcCommandType : uint
{
    None = 0,
    SetSampleInterval = 1,  // Param1 = ms
    TriggerSmartScan = 2,
    ToggleSensor = 3,       // Param1 = sensor ID
    Shutdown = 4,
    SetLogLevel = 5,        // Param1 = log level
}

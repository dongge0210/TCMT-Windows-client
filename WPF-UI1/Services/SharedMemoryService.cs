using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Text;
using WPF_UI1.Models;
using Serilog;

namespace WPF_UI1.Services
{
 // 新协议共享内存读取服务：仅解析 Models.SharedMemoryBlock (精简结构)
 public class SharedMemoryService : IDisposable
 {
 private MemoryMappedFile? _mmf;
 private MemoryMappedViewAccessor? _accessor;
 private readonly object _lock = new();
 private bool _disposed;

 private const string GLOBAL_NAME = "Global\\SystemMonitorSharedMemory";
 private const string LOCAL_NAME = "Local\\SystemMonitorSharedMemory";
 private const string FALLBACK_NAME = "SystemMonitorSharedMemory";

 // 根据 C++ pack(1)结构精确计算后的尺寸 (包含USB设备信息)
 public const int ExpectedSize =3212;

 // 字段偏移（与 C++ DataStruct.h 保持同步）
 private const int OFF_abiVersion =0;
 private const int OFF_writeSequence =4;
 private const int OFF_snapshotVersion =8;
 private const int OFF_reservedHeader =12;
 private const int OFF_cpuLogicalCores =16;
 private const int OFF_cpuUsagePercent_x10 =18;
 private const int OFF_memoryTotalMB =20;
 private const int OFF_memoryUsedMB =28;
 private const int OFF_tempSensors =36; // TemperatureSensor[32] 起始
 private const int TEMP_SENSOR_SIZE =35; //32(name)+2(value)+1(flags)
 private const int OFF_tempSensorCount =1156; //36 +32*35 =1156
 private const int OFF_smartDisks =1158; // SmartDiskScore[16] 起始
 private const int SMART_DISK_SIZE =49; //32+2+4+2+2+2+2+2+1
 private const int OFF_smartDiskCount =1942;
 private const int OFF_baseboardManufacturer =1943; //128
 private const int OFF_baseboardProduct =2071; //+128
 private const int OFF_baseboardVersion =2135; //+64
 private const int OFF_baseboardSerial =2199; //+64
 private const int OFF_biosVendor =2263; //+64
 private const int OFF_biosVersion =2327; //+64
 private const int OFF_biosDate =2391; //+64
 private const int OFF_secureBootEnabled =2423; //+32
 private const int OFF_tpmPresent =2424; //+1
 private const int OFF_memorySlotsTotal =2425; //+1
 private const int OFF_memorySlotsUsed =2427; //+2
 private const int OFF_futureReserved =2429; //64
 private const int OFF_sharedmemHash =2493; //+64
 private const int OFF_usbDevices =2525; //+32
 private const int OFF_usbDeviceCount =3093; //+8*71
 private const int OFF_extensionPad =3094; //+1

 public bool IsInitialized { get; private set; }
 public string LastError { get; private set; } = string.Empty;

 public bool Initialize()
 {
 lock (_lock)
 {
 if (IsInitialized) return true;
 string[] names = { GLOBAL_NAME, LOCAL_NAME, FALLBACK_NAME };
 foreach (var name in names)
 {
 try
 {
 _mmf = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
 _accessor = _mmf.CreateViewAccessor(0, ExpectedSize, MemoryMappedFileAccess.Read);
 IsInitialized = true;
 Log.Information($"连接共享内存成功 name={name} size(view)={ExpectedSize}");
 return true;
 }
 catch (FileNotFoundException) { continue; }
 catch (Exception ex)
 {
 Log.Debug($"打开 {name}失败: {ex.Message}");
 }
 }
 LastError = "未找到共享内存 (可能 C++ 未运行/名称不同)";
 Log.Error(LastError);
 return false;
 }
 }

 public SharedMemoryDiagnostics? ValidateLayout()
 {
 lock (_lock)
 {
 if (!IsInitialized && !Initialize()) return null;
 if (_accessor == null) return null;
 try
 {
 var raw = new byte[Math.Min(ExpectedSize, _accessor.Capacity)];
 _accessor.ReadArray(0, raw,0, raw.Length);
 uint abi = BitConverter.ToUInt32(raw, OFF_abiVersion);
 uint seq = BitConverter.ToUInt32(raw, OFF_writeSequence);
 uint snap = BitConverter.ToUInt32(raw, OFF_snapshotVersion);
 ushort logical = BitConverter.ToUInt16(raw, OFF_cpuLogicalCores);
 short cpuUsageX10 = BitConverter.ToInt16(raw, OFF_cpuUsagePercent_x10);
 ulong memTotal = BitConverter.ToUInt64(raw, OFF_memoryTotalMB);
 ulong memUsed = BitConverter.ToUInt64(raw, OFF_memoryUsedMB);
 byte smartCount = raw[OFF_smartDiskCount];
 ushort tempCount = BitConverter.ToUInt16(raw, OFF_tempSensorCount);
 byte sb = raw[OFF_secureBootEnabled];
 byte tpm = raw[OFF_tpmPresent];
 ushort slotsTotal = BitConverter.ToUInt16(raw, OFF_memorySlotsTotal);
 ushort slotsUsed = BitConverter.ToUInt16(raw, OFF_memorySlotsUsed);
 byte usbCount = raw[OFF_usbDeviceCount];
 var diag = new SharedMemoryDiagnostics
 {
 AbiVersion = abi,
 WriteSequence = seq,
 SnapshotVersion = snap,
 LogicalCores = logical,
 CpuUsagePercentX10 = cpuUsageX10,
 MemoryTotalMB = memTotal,
 MemoryUsedMB = memUsed,
 TempSensorCount = tempCount,
 SmartDiskCount = smartCount,
 SecureBootEnabled = sb !=0,
 TpmPresent = tpm !=0,
 MemorySlotsTotal = slotsTotal,
 MemorySlotsUsed = slotsUsed,
 UsbDeviceCount = usbCount,
 IsSequenceStable = (seq %2) ==0,
 ViewSize = raw.Length,
 ExpectedSize = ExpectedSize,
 SizeMatches = raw.Length == ExpectedSize
 };
 diag.SharedMemHashHex = BytesToHex(raw, OFF_sharedmemHash,32);
 diag.FutureReservedFlags = BytesToHex(raw, OFF_futureReserved,1);
 diag.OffsetSummary = BuildOffsetSummary(raw);
 return diag;
 }
 catch (Exception ex)
 {
 Log.Error(ex, "Shared memory validation failed");
 return null;
 }
 }
 }

 public SystemInfo? ReadSystemInfo()
 {
 lock (_lock)
 {
 if (!IsInitialized && !Initialize()) return null;
 if (_accessor == null) return null;
 try
 {
 var raw = new byte[Math.Min(ExpectedSize, _accessor.Capacity)];
 _accessor.ReadArray(0, raw,0, raw.Length);
 uint seq = BitConverter.ToUInt32(raw, OFF_writeSequence);
 if ((seq &1) ==1)
 {
 System.Threading.Thread.Sleep(5);
 _accessor.ReadArray(0, raw,0, raw.Length);
 seq = BitConverter.ToUInt32(raw, OFF_writeSequence);
 }
 return ParseSystemInfo(raw);
 }
 catch (Exception ex)
 {
 LastError = "读取失败: " + ex.Message;
 Log.Error(ex, LastError);
 return null;
 }
 }
 }

 private SystemInfo ParseSystemInfo(byte[] raw)
 {
 var si = new SystemInfo();
 try
 {
 si.PhysicalCores = BitConverter.ToUInt16(raw, OFF_cpuLogicalCores);
 si.LogicalCores = si.PhysicalCores;
 short cpuX10 = BitConverter.ToInt16(raw, OFF_cpuUsagePercent_x10);
 si.CpuUsage = cpuX10 >=0 ? cpuX10 /10.0 :0;
 si.TotalMemory = BitConverter.ToUInt64(raw, OFF_memoryTotalMB) *1024UL *1024UL;
 si.UsedMemory = BitConverter.ToUInt64(raw, OFF_memoryUsedMB) *1024UL *1024UL;
 si.AvailableMemory = si.TotalMemory > si.UsedMemory ? si.TotalMemory - si.UsedMemory :0;
 si.CpuName = $"abi=0x{BitConverter.ToUInt32(raw, OFF_abiVersion):X8}";

 // 温度传感器
 ushort tempCount = BitConverter.ToUInt16(raw, OFF_tempSensorCount);
 si.Temperatures.Clear();
 int maxTempCount = Math.Min(tempCount, (ushort)32);
 for (int i =0; i < maxTempCount; i++)
 {
 int baseOff = OFF_tempSensors + i * TEMP_SENSOR_SIZE;
 string name = DecodeUtf8(raw, baseOff,32) ?? $"Temp{i}";
 short val = BitConverter.ToInt16(raw, baseOff +32);
 double temp = val >=0 ? val /10.0 : double.NaN;
 si.Temperatures.Add(new TemperatureData { SensorName = name, Temperature = temp });
 if (name.ToLowerInvariant().Contains("cpu") && double.IsNaN(si.CpuTemperature)) si.CpuTemperature = temp;
 if ((name.ToLowerInvariant().Contains("gpu") || name.ToLowerInvariant().Contains("graphics")) && double.IsNaN(si.GpuTemperature)) si.GpuTemperature = temp;
 }

 si.TpmStatus = (raw[OFF_tpmPresent] !=0) ? "TPM Present" : "TPM Absent";
 si.HasTpm = raw[OFF_tpmPresent] !=0;
 si.LastUpdate = DateTime.Now;
 }
 catch (Exception ex)
 {
 Log.Warning(ex, "解析共享内存出现异常，返回部分信息");
 }
 return si;
 }

 private string? DecodeUtf8(byte[] raw, int offset, int max)
 {
 int len =0; while (len < max && raw[offset + len] !=0) len++;
 if (len ==0) return null;
 try { return Encoding.UTF8.GetString(raw, offset, len); } catch { return null; }
 }
 private string BytesToHex(byte[] raw, int offset, int count)
 {
 var sb = new StringBuilder(count *2);
 for (int i =0; i < count; i++) sb.Append(raw[offset + i].ToString("X2"));
 return sb.ToString();
 }

 private string BuildOffsetSummary(byte[] raw)
 {
 var sb = new StringBuilder();
 void add(string name, int off, int len)
 {
 sb.Append(name).Append(" @").Append(off).Append(" len=").Append(len).Append(" -> ");
 int show = Math.Min(len,16);
 for (int i =0; i < show && off + i < raw.Length; i++) sb.Append(raw[off + i].ToString("X2"));
 if (len > show) sb.Append("...");
 sb.AppendLine();
 }
 add("abiVersion", OFF_abiVersion,4);
 add("writeSequence", OFF_writeSequence,4);
 add("snapshotVersion", OFF_snapshotVersion,4);
 add("cpuLogicalCores", OFF_cpuLogicalCores,2);
 add("cpuUsagePercent_x10", OFF_cpuUsagePercent_x10,2);
 add("memoryTotalMB", OFF_memoryTotalMB,8);
 add("memoryUsedMB", OFF_memoryUsedMB,8);
 add("tempSensors[0]", OFF_tempSensors, TEMP_SENSOR_SIZE);
 add("tempSensorCount", OFF_tempSensorCount,2);
 add("smartDisks[0]", OFF_smartDisks, SMART_DISK_SIZE);
 add("smartDiskCount", OFF_smartDiskCount,1);
 add("baseboardManufacturer", OFF_baseboardManufacturer,128);
 add("baseboardProduct", OFF_baseboardProduct,64);
 add("biosVendor", OFF_biosVendor,64);
 add("biosVersion", OFF_biosVersion,64);
 add("biosDate", OFF_biosDate,32);
 add("secureBootEnabled", OFF_secureBootEnabled,1);
 add("tpmPresent", OFF_tpmPresent,1);
 add("memorySlotsTotal", OFF_memorySlotsTotal,2);
 add("memorySlotsUsed", OFF_memorySlotsUsed,2);
 add("futureReserved", OFF_futureReserved,64);
 add("sharedmemHash", OFF_sharedmemHash,32);
 add("usbDevices[0]", OFF_usbDevices,71);
 add("usbDeviceCount", OFF_usbDeviceCount,1);
 add("extensionPad", OFF_extensionPad,118);
 sb.Append("TotalSize=").Append(raw.Length).Append(" Expected=").Append(ExpectedSize);
 return sb.ToString();
 }

 public void Dispose()
 {
 if (_disposed) return;
 lock (_lock)
 {
 _accessor?.Dispose();
 _mmf?.Dispose();
 _accessor = null;
 _mmf = null;
 IsInitialized = false;
 _disposed = true;
 }
 GC.SuppressFinalize(this);
 }
 ~SharedMemoryService() => Dispose();
 }

 public class SharedMemoryDiagnostics
 {
 public uint AbiVersion { get; set; }
 public uint WriteSequence { get; set; }
 public uint SnapshotVersion { get; set; }
 public bool IsSequenceStable { get; set; }
 public ushort LogicalCores { get; set; }
 public short CpuUsagePercentX10 { get; set; }
 public ulong MemoryTotalMB { get; set; }
 public ulong MemoryUsedMB { get; set; }
 public ushort TempSensorCount { get; set; }
 public byte SmartDiskCount { get; set; }
 public bool SecureBootEnabled { get; set; }
 public bool TpmPresent { get; set; }
 public ushort MemorySlotsTotal { get; set; }
 public ushort MemorySlotsUsed { get; set; }
 public byte UsbDeviceCount { get; set; }
 public int ViewSize { get; set; }
 public int ExpectedSize { get; set; }
 public bool SizeMatches { get; set; }
 public string SharedMemHashHex { get; set; } = string.Empty;
 public string FutureReservedFlags { get; set; } = string.Empty;
 public string OffsetSummary { get; set; } = string.Empty;
 }
}

// 合并的共享内存结构定义（原 Models\SharedMemoryBlock.cs）
namespace WPF_UI1.Models
{
 using System.Runtime.InteropServices;

 [StructLayout(LayoutKind.Sequential, Pack =1)]
 public struct SYSTEMTIME
 {
 public ushort wYear;
 public ushort wMonth;
 public ushort wDayOfWeek;
 public ushort wDay;
 public ushort wHour;
 public ushort wMinute;
 public ushort wSecond;
 public ushort wMilliseconds;
 }

 [StructLayout(LayoutKind.Sequential, Pack =1)]
 public struct TemperatureSensor
 {
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)]
 public byte[] name;
 public short valueC_x10;
 public byte flags;
 public static TemperatureSensor CreateDefault() => new TemperatureSensor { name = new byte[32], valueC_x10 = -1, flags =0 };
 }

 [StructLayout(LayoutKind.Sequential, Pack =1)]
 public struct SmartDiskScore
 {
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)] public byte[] diskId;
 public short score;
 public int hoursOn;
 public short wearPercent;
 public ushort reallocated;
 public ushort pending;
 public ushort uncorrectable;
 public short temperatureC;
 public byte recentGrowthFlags;
 public static SmartDiskScore CreateDefault() => new SmartDiskScore { diskId = new byte[32], score = -1, hoursOn =0, wearPercent = -1, reallocated =0, pending =0, uncorrectable =0, temperatureC = -1, recentGrowthFlags =0 };
 }

 [StructLayout(LayoutKind.Sequential, Pack =1)]
 public struct USBDeviceData
 {
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =4)] public byte[] drivePath;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)] public byte[] volumeLabel;
 public ulong totalSize;
 public ulong freeSpace;
 public byte isUpdateReady;
 public byte state;
 public byte reserved;
 public SYSTEMTIME lastUpdate; //16字节时间戳
 public static USBDeviceData CreateDefault() => new USBDeviceData { drivePath = new byte[4], volumeLabel = new byte[32], totalSize =0, freeSpace =0, isUpdateReady =0, state =0, reserved =0, lastUpdate = new SYSTEMTIME() };
 }

 [StructLayout(LayoutKind.Sequential, Pack =1)]
 public struct SharedMemoryBlock
 {
 public uint abiVersion;
 public uint writeSequence;
 public uint snapshotVersion;
 public uint reservedHeader;
 public ushort cpuLogicalCores;
 public short cpuUsagePercent_x10;
 public ulong memoryTotalMB;
 public ulong memoryUsedMB;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)] public TemperatureSensor[] tempSensors;
 public ushort tempSensorCount;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =16)] public SmartDiskScore[] smartDisks;
 public byte smartDiskCount;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =128)] public byte[] baseboardManufacturer;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] baseboardProduct;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] baseboardVersion;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] baseboardSerial;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] biosVendor;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] biosVersion;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)] public byte[] biosDate;
 public byte secureBootEnabled;
 public byte tpmPresent;
 public ushort memorySlotsTotal;
 public ushort memorySlotsUsed;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =64)] public byte[] futureReserved;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =32)] public byte[] sharedmemHash;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =8)] public USBDeviceData[] usbDevices;
 public byte usbDeviceCount;
 [MarshalAs(UnmanagedType.ByValArray, SizeConst =118)] public byte[] extensionPad;
 public static SharedMemoryBlock CreateDefault()
 {
 var block = new SharedMemoryBlock
 {
 abiVersion =0x00010014,
 writeSequence =0,
 snapshotVersion =0,
 reservedHeader =0,
 cpuLogicalCores =0,
 cpuUsagePercent_x10 = -1,
 memoryTotalMB =0,
 memoryUsedMB =0,
 tempSensors = new TemperatureSensor[32],
 tempSensorCount =0,
 smartDisks = new SmartDiskScore[16],
 smartDiskCount =0,
 baseboardManufacturer = new byte[128],
 baseboardProduct = new byte[64],
 baseboardVersion = new byte[64],
 baseboardSerial = new byte[64],
 biosVendor = new byte[64],
 biosVersion = new byte[64],
 biosDate = new byte[32],
 secureBootEnabled =0,
 tpmPresent =0,
 memorySlotsTotal =0,
 memorySlotsUsed =0,
 futureReserved = new byte[64],
 sharedmemHash = new byte[32],
 usbDevices = new USBDeviceData[8],
 usbDeviceCount =0,
 extensionPad = new byte[118]
 };
 for (int i =0; i <32; i++) block.tempSensors[i] = TemperatureSensor.CreateDefault();
 for (int i =0; i <16; i++) block.smartDisks[i] = SmartDiskScore.CreateDefault();
 for (int i =0; i <8; i++) block.usbDevices[i] = USBDeviceData.CreateDefault();
 return block;
 }
 }
}

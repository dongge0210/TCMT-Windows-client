# TCMT macOS适配 - 进度跟踪

## 当前状态：✅ 阶段1-5完成，功能可运行

```
✅ 阶段1：CMake基础结构
✅ 阶段2：平台抽象层（Platform/SharedMemoryManager跨平台）
✅ 阶段3：核心硬件模块（CPU/GPU/Memory/Disk/Network/OS）
✅ 阶段4：温度监控（SMC + powermetrics后台线程）
✅ 阶段5：主程序适配（main_mac.cpp完整）
```

## 已完成模块

| 模块 | 状态 | 实现方式 |
|------|------|---------|
| CpuInfo | ✅ | mach/host_processor_info + sysctl |
| GpuInfo | ✅ | IOKit IOAccelerator（无Metal） |
| MemoryInfo | ✅ | host_statistics64 + sysctl |
| DiskInfo | ✅ | getfsstat + IOKit；过滤系统快照 |
| NetworkAdapter | ✅ | getifaddrs + if_msghdr |
| OSInfo | ✅ | sysctl kern.osproductversion/hw.model |
| TemperatureWrapper | ✅ | SMC直接读取 + powermetrics缓存线程 |
| SharedMemoryManager | ✅ | POSIX shm_open/mmap |
| Platform_macOS | ✅ | SystemTime/CriticalSection/InterprocessMutex |
| Logger | ✅ | ANSI颜色 + strftime |
| WinUtils stubs | ✅ | 跨平台UTF-8转换 |
| TimeUtils | ✅ | sysctl KERN_BOOTTIME |
| ComInitializationHelper | ✅ | no-op on macOS |

## 待完善项（优先级排序）

### P1 — 功能增强
1. [ ] `SystemInfo.gpuName` string字段：main_mac未填（用vector替代了）
2. [ ] 网络速度：macOS上ifi_baudrate可能为0，需其他方案估算
3. [ ] CPU核心频率：mach/host_info获取的cpufrequency可能为0

### P2 — 代码质量
4. [ ] Producer.cpp空符号警告（libTCMTCore.a(Producer.cpp.o) has no symbols）
5. [ ] Producer.cpp在macOS上stub化
6. [ ] DiskInfo label使用volume实际label而非mountpoint最后一段

### P3 — 温度精度
7. [ ] SMC读取：Apple Silicon在非root情况下keys不可读
8. [ ] powermetrics缓存线程：运行时验证（需要root环境测试）
9. [ ] 考虑：使用coretemp/程控Thermal来实现无root的温度读取

## 技术债务记录

- `integer_t` vs `natural_t`：ARM64兼容性，host_processor_info第三参数必须用natural_t
- `cpuid.h`：x86 only，virtualization detection改用sysctlbyname("hw.hypervisor")
- `kIOMainPortDefault`：macOS 12+才引入，需fallback到kIOMasterPortDefault
- `ftruncate`不能shrink POSIX shm：macOS安全限制，需每次shm_unlink重建
- `shm_open`名字最长31字符（含前导`/`），InterprocessMutex的name+_mutex截断到20
- `Metal ObjC桥接`：不能用ObjC方法调用，需纯C IOKit方案

## 当前分支
- 分支：`mac`（基于`dev`分支）
- 已推送至 origin/mac

## 构建命令
```bash
cd TCMT-Windows-client
mkdir -p cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
./src/TCMTClient
```

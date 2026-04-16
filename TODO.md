# TCMT跨平台迁移 - 未完成部分

## 当前状态
**✅ 已完成**
- CMake基础结构搭建（阶段1）
- 平台抽象层实现（阶段2）
  - Platform.h抽象接口
  - Platform_Windows.cpp实现
  - Platform_macOS.cpp实现
  - DataStruct.h使用平台类型（PlatformCriticalSection, PlatformSystemTime）
  - SharedMemoryManager重构为跨平台
    - SharedMemoryManager.cpp作为分发器
    - SharedMemoryManager_Windows.cpp保持现有实现
    - SharedMemoryManager_macOS.cpp实现POSIX版本
  - Logger.h跨平台适配（HANDLE条件编译）

## 未完成部分

### 阶段3：硬件监控模块重构（5-7天）
1. **CPU信息模块** (`CpuInfo`)
   - [ ] 完成CpuInfo.h重构：移除Windows依赖（DWORD, PDH_HQUERY等），使用平台无关类型
   - [ ] 创建CpuInfo.cpp作为条件编译分发器
   - [ ] 创建CpuInfo_macOS.cpp：使用sysctl、mach API实现
   - [ ] 保持CpuInfo_Windows.cpp现有PDH实现

2. **GPU信息模块** (`GpuInfo`)
   - [ ] 重构GpuInfo.h为跨平台接口
   - [ ] 创建GpuInfo_macOS.cpp：使用IOKit + Metal API
   - [ ] 保持GpuInfo_Windows.cpp现有WMI + NVML实现

3. **内存信息模块** (`MemoryInfo`)
   - [ ] 重构MemoryInfo.h为跨平台接口
   - [ ] 创建MemoryInfo_macOS.cpp：使用host_statistics64, vm_statistics64
   - [ ] 保持MemoryInfo_Windows.cpp现有GlobalMemoryStatusEx实现

4. **磁盘信息模块** (`DiskInfo`)
   - [ ] 重构DiskInfo.h为跨平台接口
   - [ ] 创建DiskInfo_macOS.cpp：使用statfs、diskutil、IOKit存储服务
   - [ ] 保持DiskInfo_Windows.cpp现有WMI + SMART实现

5. **网络适配器模块** (`NetworkAdapter`)
   - [ ] 重构NetworkAdapter.h为跨平台接口
   - [ ] 创建NetworkAdapter_macOS.cpp：使用getifaddrs、sysctl
   - [ ] 保持NetworkAdapter_Windows.cpp现有WMI实现

6. **操作系统信息模块** (`OSInfo`)
   - [ ] 重构OSInfo.h为跨平台接口
   - [ ] 创建OSInfo_macOS.cpp：使用sysctl、NSProcessInfo
   - [ ] 保持OSInfo_Windows.cpp现有Windows API实现

### 阶段4：温度监控重构（3-4天）
1. **温度监控接口设计**
   - [ ] 创建TemperatureMonitor统一接口
   - [ ] 支持多平台：Windows、macOS、未来Linux

2. **Windows实现保持**
   - [ ] 保持现有C++/CLI代码
   - [ ] 条件编译隔离：仅在Windows平台编译

3. **macOS实现开发**
   - [ ] 实现SMC读取器类：`SMCReader`
   - [ ] 支持温度传感器枚举：CPU、GPU、主板等

### 阶段5：主程序适配与集成（2-3天）
1. **重构main.cpp**
   - [ ] 移除Windows特有头文件包含
   - [ ] 使用平台抽象函数替换Windows API调用
   - [ ] 条件编译控制台初始化和清理代码

2. **平台特定初始化**
   - [ ] Windows：COM初始化、WMI连接、PDH查询初始化
   - [ ] macOS：SMC连接、IOKit服务初始化

3. **共享内存系统集成**
   - [ ] 测试Windows和macOS共享内存兼容性
   - [ ] 验证进程间通信功能

### 阶段6：测试与验证（2-3天）
1. **构建测试**
   - [ ] Windows CMake构建（Visual Studio 2022）
   - [ ] macOS CMake构建（Xcode 15+）
   - [ ] 验证与原有构建系统的兼容性

2. **功能测试**
   - [ ] Windows功能回归测试
   - [ ] macOS功能完整测试
   - [ ] 跨平台数据一致性验证

3. **性能测试**
   - [ ] 监控准确性测试
   - [ ] 系统资源占用测试
   - [ ] 实时性验证（更新频率）

## 近期优先级

### 高优先级（接下来应该完成）
1. **CpuInfo模块重构** - 这是最依赖Windows API的核心模块，需要优先处理
2. **GPU和内存模块** - 主要监控指标
3. **温度监控macOS实现** - 替换C++/CLI依赖的关键部分

### 中优先级
1. 磁盘和网络模块
2. 操作系统信息模块
3. 主程序适配

### 低优先级
1. 全面测试和性能优化
2. Linux支持（未来扩展）

## 已知技术挑战

1. **macOS硬件监控深度**：需要深入macOS系统API（IOKit、SMC、mach、sysctl）
2. **C++/CLI移除**：温度监控需要完全重写为跨平台实现
3. **实时性要求**：macOS API的实时性可能与Windows不同
4. **共享内存同步**：Windows CRITICAL_SECTION vs macOS pthread_mutex跨进程同步

## 当前分支
- 当前分支：`mac`（基于`dev`分支创建）
- 远程仓库：已推送至`origin/mac`
- 最后一次提交：`74f6fba5 feat(platform): 实现平台抽象层（阶段2）- SharedMemoryManager跨平台`

## 下一步建议
1. 立即开始CpuInfo模块重构（Task #6）
2. 逐个模块完成平台抽象
3. 定期测试CMake构建，确保不破坏现有功能
4. 保持与原有Visual Studio构建系统的兼容性
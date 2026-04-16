# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Windows hardware monitoring tool named "TCMT Windows Client". It consists of:
- A C++/CLI core component (`Project1`) that collects hardware data using LibreHardwareMonitor, CUDA, and WMI
- A WPF UI (`WPF-UI1`) that displays the collected data in real-time
- Communication between components via shared memory (`SharedMemoryManager`)

The core component is a mixed C++/CLI project targeting .NET Framework 4.7.2. The WPF UI targets .NET 8.0 Windows.

## Build Commands

### Prerequisites
- Visual Studio 2022 with C++/CLI support
- CUDA Toolkit 12.6+ (for GPU monitoring)
- .NET 8.0 SDK
- Git submodules initialized

### Initialize Submodules
```bash
git submodule update --init --recursive
```

### Build LibreHardwareMonitorLib
First build the LibreHardwareMonitor library (requires .NET Framework 4.7.2 targeting):
```bash
cd src/third_party/LibreHardwareMonitor
dotnet build LibreHardwareMonitorLib/LibreHardwareMonitorLib.csproj -c Release -f net472
```

### Build CPP-parsers
Build the C++ parsers component:
```bash
msbuild src/CPP-parsers/CPP-parsers/CPP-parsers.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /m
```

### Build WPF UI
Build the WPF user interface:
```bash
cd WPF-UI1
dotnet build WPF-UI1.csproj -c Release
```

### Build Main Project
Build the main C++/CLI project:
```bash
msbuild Project1/Project1.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /m
```

### Full Build Sequence
The complete build sequence as used in CI (see `.github/workflows/build.yml`):
1. Restore NuGet packages: `nuget restore Project1/Project1.sln`
2. Build LibreHardwareMonitorLib
3. Build CPP-parsers
4. Build WPF-UI1
5. Build main project

Build artifacts are output to `Project1/x64/Release/`.

## Architecture

### Core Component (`src/core/`)
- **Hardware Modules**: CPU (`CpuInfo`), GPU (`GpuInfo`), Memory (`MemoryInfo`), Disk (`DiskInfo`), Network (`NetworkAdapter`), OS (`OSInfo`) info collectors
- **Utilities**: `WMIManager` for WMI queries, `WinUtils` for Windows APIs, `Logger` for logging
- **LibreHardwareMonitorBridge**: C++/CLI bridge to the .NET LibreHardwareMonitor library for hardware sensors
- **SharedMemoryManager**: Manages shared memory block (`SharedMemoryBlock`) for inter-process communication

### Data Structures (`src/core/DataStruct/`)
- `DataStruct.h`: Primary data structures including `SystemInfo`, `SharedMemoryBlock`, hardware-specific structs
- `SharedMemoryManager.h/cpp`: Windows shared memory implementation for IPC between C++ core and WPF UI
- Structures are packed (`#pragma pack(push, 1)`) for consistent memory layout

### WPF UI (`WPF-UI1/`)
- MVVM architecture with ViewModels, Models, and Services
- Uses MaterialDesignThemes for UI styling
- LiveCharts for real-time charts
- Serilog for logging
- Dependency injection via Microsoft.Extensions.Hosting

### Communication
1. C++ core collects hardware data via LibreHardwareMonitor, WMI, and direct APIs
2. Data is written to shared memory (`SharedMemoryBlock`)
3. WPF UI reads from shared memory to display real-time metrics
4. Shared memory includes synchronization via `CRITICAL_SECTION`

### Dependencies
- **CUDA 12.6**: NVIDIA Management Library (NVML) for GPU monitoring
- **LibreHardwareMonitor**: .NET hardware monitoring library
- **WPF UI Packages**: LiveCharts, MaterialDesign, Serilog, CommunityToolkit.Mvvm

## Development Notes

### Code Style
- C++ code uses modern C++20 with C++/CLI extensions where needed
- Header files use `#pragma once`
- WPF UI follows MVVM pattern with dependency injection
- Shared memory structures use fixed-size arrays for IPC compatibility

### Important Paths
- C++ Core: `src/core/` and `Project1/`
- WPF UI: `WPF-UI1/`
- Third-party submodules: `src/third_party/` and `src/CPP-parsers/`
- Build output: `Project1/x64/{Configuration}/`

### Testing
Unit tests are planned but not yet implemented (see `docs/architecture_and_protocol_plan_v0.14_full.md`).

### Platform Support
Windows only (x64). Requires Windows 10/11 with appropriate drivers for hardware monitoring.

## Common Issues

### Submodule Dependencies
Ensure all submodules are initialized before building. The build will fail if LibreHardwareMonitor or CPP-parsers are missing.

### CUDA Installation
CUDA Toolkit must be installed and accessible in `%CUDA_PATH%`. The build expects CUDA 12.6.

### Mixed-Mode Assembly
The C++/CLI project references .NET Framework 4.7.2 assemblies. Ensure appropriate targeting packs are installed.

## References
- `.github/workflows/build.yml`: CI build configuration
- `README-WF.md`: License and submodule information
- `docs/architecture_and_protocol_plan_v0.14_full.md`: Architecture notes
# AGENTS.md

## Build Commands

### Windows (msbuild + CUDA)
```bash
# Init submodules first (required!)
git submodule update --init --recursive

# Build order matters
dotnet build src/third_party/LibreHardwareMonitor/LibreHardwareMonitorLib/LibreHardwareMonitorLib.csproj -c Release -f net472
msbuild src/CPP-parsers/CPP-parsers/CPP-parsers.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /m
cd AvaloniaUI && dotnet build AvaloniaUI.csproj -c Release
msbuild TCMT.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /m
```

### macOS (CMake + Clang)
```bash
brew install ncurses
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

## Architecture

- **Core**: C++ (cross-platform) → .NET Framework 4.7.2 (Windows)
- **UI**: AvaloniaUI (.NET 10.0 cross-platform) or TUI (ncurses)
- **IPC**: Shared memory via `SharedMemoryBlock`
- **Third-party**: Git submodules in `src/third_party/`

## Critical Requirements

1. **Git submodules** — Always init before building: `git submodule update --init --recursive`
2. **CUDA Toolkit** — Required for GPU monitoring (Windows build)
3. **Build order** — LibreHardwareMonitorLib → CPP-parsers → AvaloniaUI → main
4. **Output path** — `Project1/x64/Release/` (Windows) or `build/` (macOS)

## Directory Structure

### `src/core/` — Hardware Monitoring Core
| Path | Purpose |
|------|---------|
| `DataStruct/` | Data structures (SharedMemoryBlock, SystemInfo) |
| `Platform/` | Cross-platform abstraction (Windows/macOS) |
| `cpu/` | CPU info (usage, cores, frequency) |
| `gpu/` | GPU info (NVIDIA/AMD/Intel via NVML) |
| `memory/` | Memory info (physical/virtual) |
| `disk/` | Disk info (logical/physical, SMART) |
| `network/` | Network adapters |
| `temperature/` | Temperature sensors |
| `os/` | OS info |
| `Utils/` | Logger, WMI (Windows), WinUtils |

### `src/tui/` — Terminal UI (macOS/Linux)
- `TuiApp.h/cpp` — ncurses-based TUI application
- `LogBuffer.h` — Thread-safe log buffer (500 lines)

### `AvaloniaUI/` — Cross-platform UI
- `Views/MainWindow.axaml` — Main UI
- `ViewModels/MainWindowViewModel.cs` — Business logic
- `Services/SharedMemoryService.cs` — IPC via shared memory
- `Models/SystemInfo.cs` — Data models

### `src/third_party/` — Git Submodules (9 total)
```
LibreHardwareMonitor/  curl/  PDCurses/  TC/
USBMonitor-cpp/       websocketpp/  tpm2-tss/  FFmpeg/
```
All submodules must be initialized before building.

## Platform-Specific Tech Stack

| Platform | Hardware Monitoring |
|----------|-------------------|
| Windows | WMI, PDH, LibreHardwareMonitor, NVIDIA NVML |
| macOS | IOKit, Metal, SMC |

## Testing

No unit tests implemented yet.

## Platform Support

- **Windows x64** — Full features (LibreHardwareMonitor, CUDA, WMI, AvaloniaUI)
- **macOS ARM64** — TUI + basic hardware monitoring

# Installation Guide

This guide provides detailed instructions for building and installing the project on different platforms.

## Table of Contents

- [System Requirements](#system-requirements)
- [Quick Installation](#quick-installation)
- [Platform-Specific Instructions](#platform-specific-instructions)
  - [Windows](#windows)
  - [macOS](#macos)
  - [Linux](#linux)
- [Dependencies](#dependencies)
- [Build Configuration](#build-configuration)
- [Troubleshooting](#troubleshooting)

## System Requirements

### Minimum Requirements
- **C++23** compatible compiler
- **CMake 3.16** or higher
- **4GB RAM** for compilation
- **2GB disk space** for source and build

### Recommended Requirements
- **C++23** compatible compiler for best experience
- **CMake 3.20+**
- **8GB RAM** for faster compilation
- **SSD storage** for improved build times

### Supported Compilers
- **GCC 9.0+** (Linux, Windows with MinGW)
- **Clang 10.0+** (Linux, macOS, Windows with Clang)
- **MSVC 2019 16.8+** (Windows)
- **Apple Clang 12.0+** (macOS)

### Supported Platforms
- **Windows 10/11** (x64, arm64)
- **macOS 11.0+** (x64, Apple Silicon)
- **Linux** (Ubuntu 20.04+, CentOS 8+, Fedora 32+, Arch Linux)

## Quick Installation

### Automatic Installation (Recommended)

The easiest way to build the project is using the provided build scripts:

#### Linux/macOS
```bash
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
./build.sh --auto
```

#### Windows
```powershell
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
.\build.ps1 --auto
```

### Manual Installation

If you prefer manual control over the build process:

```bash
# Clone with submodules
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template

# Create build directory
mkdir build && cd build

# Configure
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

# Build
ninja

# Install (optional)
ninja install
```

## Platform-Specific Instructions

### Windows

#### Prerequisites

1. **Visual Studio 2019 or later** with C++ development tools
   - Download from [Visual Studio](https://visualstudio.microsoft.com/)
   - Ensure "Desktop development with C++" workload is installed

2. **Git for Windows**
   - Download from [Git for Windows](https://gitforwindows.org/)

3. **CMake** (3.16 or later)
   - Download from [CMake](https://cmake.org/download/)
   - Add to PATH during installation

4. **Ninja** (recommended)
   - Download from [Ninja releases](https://github.com/ninja-build/ninja/releases)
   - Add to PATH

#### Build Instructions

##### Option 1: Using build.cmd (Recommended)
```cmd
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
build.cmd
```

##### Option 2: Using Developer Command Prompt
```cmd
# Open "Developer Command Prompt for VS 2022"
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
mkdir build && cd build

# Using Ninja (faster)
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja

# Or using MSBuild
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

##### Option 3: Using PowerShell
```powershell
# Ensure Visual Studio tools are available
& "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
.\build.ps1
```

#### Windows-Specific Options

| Option | Description | Default |
|--------|-------------|---------|
| `WIN32_EXECUTABLE` | Build Windows executable | `ON` |
| `USE_STATIC_RUNTIME` | Use static C++ runtime | `ON` |

### macOS

#### Prerequisites

1. **Xcode Command Line Tools**
   ```bash
   xcode-select --install
   ```

2. **Homebrew** (recommended package manager)
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **Dependencies via Homebrew**
   ```bash
   brew install cmake ninja git
   ```

#### Build Instructions

##### Option 1: Using build.sh (Recommended)
```bash
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
./build.sh --auto
```

##### Option 2: Manual Build
```bash
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
mkdir build && cd build

# Configure for release
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja

# Or using Xcode (for IDE development)
cmake -G Xcode ..
open project_template.xcodeproj
```

#### macOS-Specific Considerations

- **Apple Silicon (M1/M2)**: Builds natively on ARM64
- **Universal Binaries**: Use `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
- **macOS Deployment Target**: Set with `-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`

### Linux

#### Ubuntu/Debian

##### Prerequisites
```bash
# Essential build tools
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config

# Additional development libraries (if needed)
sudo apt-get install -y \
    libx11-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libfontconfig1-dev
```

##### Build Instructions
```bash
git clone --recursive https://github.com/yourusername/project_template.git
cd project_template
./build.sh --auto
```

#### CentOS/RHEL/Fedora

##### Prerequisites
```bash
# CentOS 8+ / RHEL 8+
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake ninja-build git

# Fedora
sudo dnf install gcc-c++ cmake ninja-build git

# Additional libraries (if needed)
sudo dnf install libX11-devel libXcursor-devel libXi-devel mesa-libGL-devel fontconfig-devel
```

#### Arch Linux

##### Prerequisites
```bash
sudo pacman -S base-devel cmake ninja git

# Additional libraries (if needed)
sudo pacman -S libx11 libxcursor mesa-libgl fontconfig
```

## Dependencies

### Required Dependencies

The project uses the following core dependencies:

- **CMake** (build system)
- **Standard C++ Library** (C++23 features)

### Optional Dependencies

- **Google Test** (for testing, auto-downloaded)
- **Benchmark** (for performance testing, auto-downloaded)
- **Format** library (for string formatting, auto-downloaded)

### Managing Dependencies

Dependencies are handled automatically via:
1. **Git submodules** for source dependencies
2. **CMake FetchContent** for header-only libraries
3. **find_package** for system libraries

To update all dependencies:
```bash
git submodule update --remote --recursive
```

## Build Configuration

### Build Types

| Build Type | Description | Use Case |
|------------|-------------|----------|
| `Debug` | Full debug info, no optimization | Development, debugging |
| `Release` | Optimized, no debug info | Production builds |
| `RelWithDebInfo` | Optimized with debug info | Performance testing |
| `MinSizeRel` | Size-optimized | Embedded systems |

### Common Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `CMAKE_BUILD_TYPE` | Build configuration | `RelWithDebInfo` |
| `BUILD_TESTING` | Enable unit tests | `ON` |
| `BUILD_SHARED_LIBS` | Build shared libraries | `OFF` |
| `CMAKE_INSTALL_PREFIX` | Installation directory | `/usr/local` |

### Advanced Configuration

```bash
# Custom installation prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/project_template ..

# Enable all warnings
cmake -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic" ..

# Cross-compilation (example for ARM)
cmake -DCMAKE_TOOLCHAIN_FILE=arm-toolchain.cmake ..

# Static analysis
cmake -DENABLE_CLANG_TIDY=ON ..

# Sanitizers (Debug builds)
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
```

### Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `CC` | C compiler | `gcc`, `clang` |
| `CXX` | C++ compiler | `g++`, `clang++` |
| `CMAKE_GENERATOR` | Build system generator | `Ninja`, `Unix Makefiles` |
| `CMAKE_BUILD_PARALLEL_LEVEL` | Parallel build jobs | `4` |

## Installation

### System Installation

```bash
# After successful build
cd build
sudo ninja install

# Or specify prefix during install
DESTDIR=/tmp/install ninja install
```

### Package Installation

```bash
# Create packages
cpack

# Create specific package types
cpack -G DEB    # Debian package
cpack -G RPM    # RPM package  
cpack -G ZIP    # ZIP archive
```

## Troubleshooting

### Common Issues

#### Build Fails with "Compiler not found"

**Solution:**
```bash
# Specify compiler explicitly
export CC=gcc
export CXX=g++
cmake ..

# Or during cmake configuration
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
```

#### CMake version too old

**Problem:** `CMake 3.16 or higher is required`

**Solution:**
```bash
# Ubuntu: Install newer CMake
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt update && sudo apt install cmake

# macOS: Update via Homebrew
brew upgrade cmake

# Windows: Download from cmake.org
```

#### Git submodule issues

**Problem:** Missing submodule content

**Solution:**
```bash
git submodule update --init --recursive
```

#### Out of memory during compilation

**Problem:** Compiler runs out of memory

**Solution:**
```bash
# Reduce parallel jobs
cmake --build . -- -j2

# Or set environment variable
export CMAKE_BUILD_PARALLEL_LEVEL=2
```

#### Permission denied on Windows

**Problem:** Access denied when building

**Solution:**
- Run as Administrator
- Check antivirus software
- Use Developer Command Prompt

### Platform-Specific Issues

#### Windows: MSVC not found

**Solution:**
```cmd
# Find VS installation
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest

# Setup environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

#### macOS: Xcode license

**Problem:** Xcode license not accepted

**Solution:**
```bash
sudo xcodebuild -license accept
```

#### Linux: Missing development headers

**Problem:** Cannot find system libraries

**Solution:**
```bash
# Install development packages
sudo apt-get install build-essential
sudo apt-get install libx11-dev  # for X11
sudo apt-get install libgl1-mesa-dev  # for OpenGL
```

### Getting Help

If you encounter issues not covered here:

1. **Search existing issues** on GitHub
2. **Check system requirements** above
3. **Try a clean build** (`rm -rf build && mkdir build`)
4. **Create an issue** with:
   - Your operating system and version
   - Compiler version (`gcc --version`, `clang --version`)
   - CMake version (`cmake --version`)
   - Full error message
   - Build command used

### Performance Tips

- **Use Ninja** instead of Make for faster builds
- **Enable ccache** for incremental builds: `cmake -DENABLE_CCACHE=ON ..`
- **Use multiple CPU cores**: `cmake --build . -- -j$(nproc)`
- **Use SSD storage** for source and build directories
- **Close unnecessary applications** during compilation

---

**Need more help?** Check our [Contributing Guide](CONTRIBUTING.md) or create an issue on GitHub.

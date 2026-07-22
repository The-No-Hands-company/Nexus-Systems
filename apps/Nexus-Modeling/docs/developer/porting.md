# Nexus Modeling — Porting Guide

*Platform-specific considerations for bringing Nexus Modeling to new targets.*

---

## Supported Platforms

| Platform | Status | Backend |
|----------|--------|---------|
| Linux (x86_64) | ✅ Production | Vulkan / Null |
| Windows 10/11 (x64) | ✅ Production | Vulkan |
| macOS (Apple Silicon) | 🚧 Beta | MoltenVK |
| Android (ARM64) | 🚧 Experimental | Vulkan |
| Web (WASM) | 📋 Planned | WebGPU |

---

## Linux (x86_64)

### Dependencies
```bash
# Ubuntu/Debian
sudo apt install cmake ninja-build g++-13 libvulkan-dev libglfw3-dev libglu1-mesa-dev

# Fedora/RHEL
sudo dnf install cmake ninja-build gcc-c++ vulkan-devel glfw-devel mesa-libGLU-devel

# Arch
sudo pacman -S cmake ninja gcc vulkan-headers vulkan-icd-loader glfw glu
```

### Build
```bash
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON
cmake --build build -j$(nproc)
```

### Headless CI
```bash
xvfb-run -a ctest --test-dir build --output-on-failure
# or
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json ./build/src/kernel/nexus_modeling --shot out.png --place box
```

---

## Windows (MSVC)

### Prerequisites
- Visual Studio 2022 (17.8+) with C++ workload
- Vulkan SDK 1.3.280+
- CMake 3.28+, Ninja
- Git for Windows

### Build (Developer Command Prompt)
```cmd
cmake -S . -B build ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DNEXUS_BACKEND_VULKAN=ON ^
  -DCMAKE_CXX_COMPILER=cl.exe

cmake --build build --config Release
```

### Visual Studio IDE
1. Open `CMakeLists.txt` via **File → Open → CMake**
2. Add CMake Settings: `Release`, `Debug`
3. CMake settings: `NEXUS_BACKEND_VULKAN=ON`
4. Build → Build All

### Vulkan SDK Path
```cmd
set VULKAN_SDK=C:\VulkanSDK\1.3.290.0
set PATH=%VULKAN_SDK%\Bin;%PATH%
```

---

## macOS (Apple Silicon)

### Prerequisites
```bash
brew install cmake ninja vulkan-sdk glfw
# MoltenVK is included in Vulkan SDK
```

### Build
```bash
cmake -S . -B build \
  -DNEXUS_BACKEND_VULKAN=ON \
  -DNEXUS_BACKEND_NULL=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### MoltenVK
```bash
export VK_ICD_FILENAMES=/opt/homebrew/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=/opt/homebrew/share/vulkan/explicit_layer.d
```

### Known Issues
| Issue | Workaround |
|-------|------------|
| MoltenVK descriptor limits | Reduce descriptor pool sizes |
| Metal validation errors | Disable validation layers (`ValidationLevel::Off`) |
| Swapchain recreation | Handle `VK_ERROR_OUT_OF_DATE_KHR` in resize |

---

## Android (ARM64)

### Toolchain
```bash
cmake -S . -B build_android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_shared \
  -DNEXUS_BACKEND_VULKAN=ON
```

### Requirements
- Android 10+ (API 29+)
- Vulkan 1.1+ device
- `android.hardware.vulkan.version` ≥ 1.1 in manifest

---

## Web (WASM) — Planned

### Emscripten
```bash
emcmake cmake -S . -B build_wasm \
  -DCMAKE_BUILD_TYPE=Release \
  -DNEXUS_BACKEND_WEBGPU=ON \
  -DCMAKE_CXX_FLAGS="-s USE_WEBGPU=1 -s WASM=1"
emmake cmake --build build_wasm
```

### Requirements
- Emscripten 3.1.50+
- WebGPU enabled browser (Chrome 113+, Firefox 120+)
- `webgpu` feature in emscripten

---

## Cross-Compilation Matrix

| Host → Target | Toolchain | Status |
|---------------|-----------|--------|
| Linux x64 → Linux x64 | Native | ✅ |
| Linux x64 → Windows x64 | mingw-w64 | ✅ |
| Linux x64 → Linux ARM64 | aarch64-linux-gnu | ✅ |
| Linux x64 → Android ARM64 | NDK r26+ | 🚧 |
| Linux x64 → WASM | Emscripten | 📋 |
| macOS ARM64 → macOS ARM64 | Native | ✅ |
| Windows x64 → Windows x64 | MSVC | ✅ |

---

## Platform-Specific Code

### Platform Abstraction Layer
```cpp
// src/kernel/src/platform/Platform.h
#if defined(_WIN32)
  #include "PlatformWindows.cpp"
#elif defined(__linux__)
  #include "PlatformLinux.cpp"
#elif defined(__APPLE__)
  #include "PlatformMacOS.cpp"
#elif defined(__ANDROID__)
  #include "PlatformAndroid.cpp"
#elif defined(__EMSCRIPTEN__)
  #include "PlatformWasm.cpp"
#endif
```

### Window Handle Abstraction
```cpp
struct NativeWindowHandle {
#if defined(_WIN32)
    HWND hwnd;
#elif defined(__linux__)
    ::Window xwindow;  // X11
    // or wl_surface* for Wayland
#elif defined(__APPLE__)
    NSView* nsview;  // or CAMetalLayer*
#elif defined(__ANDROID__)
    ANativeWindow* window;
#elif defined(__EMSCRIPTEN__)
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
#endif
};
```

### Platform-Specific Entry Points
```cpp
// Platform abstraction
namespace nexus::platform {
    std::unique_ptr<IWindow> createWindow(const WindowDesc&);
    void pollEvents();
    void* getVulkanSurface(VkInstance, const IWindow&);
}
```

---

## Platform-Specific Builds

### Linux Package (AppImage)
```bash
# linuxdeploy + appimagetool
linuxdeploy --appdir AppDir --output appimage
```

### macOS Bundle
```bash
# CMake generates .app bundle
cmake -DCMAKE_BUNDLE=ON
# Creates Nexus Modeling.app/
```

### Windows Installer (NSIS)
```nsis
!include MUI2.nsh
Name "Nexus Modeling"
OutFile "Nexus-Modeling-Setup.exe"
InstallDir "$PROGRAMFILES\Nexus Modeling"
Page directory
Page instfiles
Section
  SetOutPath $INSTDIR
  File /r "build\Release\*.exe"
  File /r "build\Release\*.dll"
SectionEnd
```

---

## CI/CD Matrix

```yaml
# .github/workflows/ci.yml
strategy:
  matrix:
    include:
      - os: ubuntu-latest
        target: linux-x64
        backend: vulkan
      - os: windows-latest
        target: windows-x64
        backend: vulkan
      - os: macos-latest
        target: macos-arm64
        backend: vulkan  # MoltenVK
```

---

*Porting guide v0.1.0-dev | Updated 2026-07-14*
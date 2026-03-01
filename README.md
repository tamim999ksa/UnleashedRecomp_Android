<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

<h1 align="center">Sonic Unleashed Recompiled: Multi-Platform</h1>

<p align="center">
  <strong>The definitive native port of the Xbox 360 classic, optimized for Android, Linux, and Windows.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android_|_Linux_|_Windows-blue?style=for-the-badge&logo=android" alt="Platforms"/>
  <img src="https://img.shields.io/badge/Vulkan-1.2+-red?style=for-the-badge&logo=vulkan" alt="Vulkan"/>
  <img src="https://img.shields.io/badge/Status-Experimental_WIP-orange?style=for-the-badge" alt="Status"/>
</p>

---

## 🌟 The Recompilation Project

This project is a high-performance, static recompilation of the Xbox 360 version of **Sonic Unleashed**. Unlike traditional emulation, this port translates the original PowerPC binaries into native ARM64 or x86-64 machine code. The result is near-native execution speed, allowing for high-fidelity gameplay on modern hardware from flagship smartphones to high-end PCs.

> [!IMPORTANT]
> **Game assets are NOT included.** You must provide your own legally acquired Xbox 360 copy of *Sonic Unleashed*.

---

## ✨ Cutting-Edge Enhancements

This fork pushes the boundaries of the recompilation engine with advanced optimizations and platform-native features:

### 🚀 Performance & Parallelism
- **Intel TBB Acceleration:** Utilizes C++17 parallel execution policies and Intel Threading Building Blocks to parallelize heavy workloads like asset hashing, STFS parsing, and GPU pipeline compilation.
- **Asynchronous Pipeline Pre-compilation:** Stutter-free gameplay via background pre-compilation of graphics pipelines (including MSAA, Gaussian Blur, and Motion Blur effects).
- **O(1) Engine Lookups:** Replaced iterative search patterns with hash-based lookups for achievements and mod assets, eliminating CPU bottlenecks during gameplay.
- **Zero-Allocation I/O:** Optimized **STFS/SVOD** parsing logic that eliminates redundant string allocations and leverages memory-mapped I/O.
- **Thread-Local Caching:** Highly efficient `ModLoader` utilizing thread-local lookup caches to prevent file system contention.

### 🎮 The Mobile Experience (Android)
- **Sustained Performance Mode:** Native Android API integration to lock CPU/GPU clocks, ensuring consistent frame rates during long play sessions.
- **Low-Latency Audio Stack:** Dedicated **AAudio** and **Oboe** backends for sub-millisecond audio response times.
- **Native Touch Interface:** Fully customized on-screen touch overlay with multi-touch support.
- **Universal HID Support:** Support for Xbox, PlayStation, and generic Bluetooth controllers with dynamic, switchable button prompts.

### 🛠️ Desktop & Modern UX
- **Desktop Excellence:** Full support for **Linux** (including Steam Deck) and **Windows** with native Vulkan 1.2 rendering.
- **Visual Fidelity:** Native support for **Resolution Scaling**, **Shadow Resolution** overrides (up to 8K), and **Ultrawide Aspect Ratio** patches.
- **Aesthetic Effects:** Recreated visual effects like TV Static, faders, and high-fidelity overlays.

---

## 📋 System Requirements

| Requirement | Android | Windows / Linux |
| :--- | :--- | :--- |
| **Architecture** | ARM64 (arm64-v8a) | x86-64 (Amd64) |
| **OS Version** | Android 8.0 (API 26) | Windows 10/11 / Ubuntu 22.04+ |
| **Graphics API** | Vulkan 1.2+ | Vulkan 1.2+ |
| **RAM** | 4 GB (Full Guest Allocation) | 8 GB+ |
| **Storage** | 10-15 GB (Internal) | 10-15 GB |

---

## 🚀 Installation & Build Guide

### 🌐 The Easy Path (GitHub Actions)
Build for any platform without installing a local compiler:
1.  **Fork** this repository.
2.  Navigate to the **Actions** tab and select the **Release** workflow.
3.  Click **Run workflow** and choose your target (**Android, Windows, or Linux**).
4.  **Automated Asset Prep:** Provide URLs for your `game_url`, `update_url`, and `dlc_url`. The CI will automatically extract and prepare **ZIP, ISO, or XEX** files.
5.  Download your artifact from the build summary.

### 💻 The Developer Path (Manual Build)

#### 1. Prerequisites
- **Common:** `cmake` (3.20+), `git`, `wget`, `unzip`, `libtbb-dev`.
- **Android:** Android SDK, **NDK 25.2.9519653**, and **Java 17**.
- **Windows:** **Visual Studio 2022** (C++ Desktop & Game workloads).
- **Linux:** `gcc-13` / `g++-13` and Vulkan SDK.

#### 2. Local Compilation

##### **Windows**
```powershell
git clone --recursive https://github.com/yourusername/UnleashedRecomp-Android.git
cd UnleashedRecomp-Android
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="../thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release --parallel
```

##### **Linux (including Steam Deck)**
```bash
git clone --recursive https://github.com/yourusername/UnleashedRecomp-Android.git
cd UnleashedRecomp-Android
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="../thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
make -j$(nproc)
```

##### **Android**
```bash
# Setup NDK Path
export ANDROID_NDK_HOME=/path/to/ndk-25.2.9519653
# Build host tools (Linux/WSL)
chmod +x ./build_tools.sh && ./build_tools.sh
# Compile APK
chmod +x ./build_android.sh && ./build_android.sh
```

---

## ⚖️ Disclaimer
*Sonic Unleashed Recompiled* is an unofficial, fan-led project. It is not affiliated with, authorized, or endorsed by SEGA® or Sonic Team™. This software is for educational purposes and requires legally owned game assets to function.

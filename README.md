<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

<h1 align="center">Unleashed Recompiled: Android Edition</h1>

<p align="center">
  <strong>High-performance static recompilation of Sonic Unleashed (Xbox 360) running natively on Android, Windows, and Linux.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android_|_Windows_|_Linux-blue?style=flat-square" alt="Platforms"/>
  <img src="https://img.shields.io/badge/Architecture-ARM64_|_x86--64-blue?style=flat-square" alt="Arch"/>
  <img src="https://img.shields.io/badge/Graphics-Vulkan_1.2-red?style=flat-square&logo=vulkan" alt="Vulkan"/>
  <img src="https://img.shields.io/badge/Status-Work_In_Progress-orange?style=flat-square" alt="Status"/>
</p>

---

This project is an **experimental, high-performance port** of [Sonic Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp). It utilizes static recompilation to translate Xbox 360 PowerPC instructions into native code, enabling the game to run with modern enhancements on Android (ARM64) as well as Windows and Linux (x86-64).

> [!IMPORTANT]
> **This repository does NOT contain any game assets.** You must provide your own legally acquired copy of *Sonic Unleashed* for the Xbox 360.

---

## ✨ Key Features & Enhancements

This fork introduces significant architectural improvements, mobile-specific optimizations, and robust multi-platform support:

### 🚀 Android & Mobile Performance
- **Sustained Performance Mode:** Integrated Android-specific performance hints to lock CPU/GPU frequencies and prevent thermal throttling.
- **Optimized Android I/O & Drivers:** Native implementation of Android-specific logic for file system resolution, logging, and thread priority management.
- **Low-Latency Audio:** Fully integrated **AAudio** and **Oboe** backends for perfectly synced sound effects and music.
- **Touch & Controller Support:** Native on-screen touch overlay and plug-and-play support for Xbox/PlayStation controllers with dynamic icon switching.

### 🛠️ Core & Kernel Optimizations
- **Modular Kernel Architecture:** Monolithic imports have been refactored into modular units (`threading`, `synchronization`, `I/O`, `memory`) for superior stability.
- **Parallel Execution Engine:** Leverages C++17 parallel algorithms and Intel TBB for heavy tasks like asset hashing and GPU pipeline pre-compilation.
- **O(1) Achievement System:** Optimized hash-based lookups for achievements, eliminating redundant iteration overhead.
- **Advanced File System:** Native support for Xbox 360 **STFS** and **SVOD** parsing with zero-allocation string handling and memory-mapped I/O.
- **Modding Support:** Thread-local mod lookup cache in `ModLoader` to minimize file system contention.

### 🎮 Multi-Platform & Modern UX
- **Windows & Linux:** Full x86-64 support with native Vulkan 1.2 rendering and high-refresh-rate display compatibility.
- **Visual Enhancements:** Native support for **Resolution Scaling**, **Aspect Ratio Patches** (including Ultrawide), and high-fidelity shadow resolution.
- **Universal Save Redirection:** Seamlessly manages save data across all platforms and mod profiles.

---

## 📱 Device Requirements (Android)

| Requirement | Minimum Specification | Recommended |
| :--- | :--- | :--- |
| **Architecture** | ARMv8-A 64-bit (ARM64) | Latest flagship SoC (Snapdragon 8 Gen 2+) |
| **OS Version** | Android 8.0 (API 26) | Android 12+ |
| **Graphics API** | Vulkan 1.2 | Vulkan 1.3 |
| **RAM** | 4 GB (Strict Guest Allocation) | 8 GB+ |
| **Storage** | 10 GB (High-speed internal) | 15 GB+ (UFS 3.1+) |

---

## 🚀 Getting Started

### 1. Prepare Your Assets
Create a `private/` directory in the project root and place: `default.xex`, `default.xexp` (optional), `shader.ar` (optional), and your Game data containers (STFS/SVOD).

### 2. Build via GitHub Actions (Recommended)
Our CI pipeline supports all target OSs:
1. **Fork** this repository.
2. Go to the **Actions** tab -> **Release** workflow -> **Run workflow**.
3. Select your target OS (**Android, Windows, or Linux**).
4. **Dynamic Data Input:** Provide URLs for your assets. The workflow supports **ZIP, ISO, and XEX** formats and will automatically prepare them.
5. Download the final artifact once the build completes.

### 3. Manual Build (Step-by-Step)

#### 📦 Dependencies (Universal)
- `gcc 13+`, `g++ 13+`, `cmake` (3.20+), `git`, `libtbb-dev`, **Java 17**.

#### 📦 Platform-Specific Requirements
- **Android:** Android SDK and **NDK 25.2.9519653**.
- **Windows:** **Visual Studio 2022** (with C++ Desktop/Game development workloads).
- **Linux:** Standard build-essential tools and development headers for Vulkan.

#### 🛠️ Build Steps
```bash
# 1. Clone with submodules
git clone --recursive https://github.com/yourusername/UnleashedRecomp-Android.git
cd UnleashedRecomp-Android

# 2. Build for Windows (PowerShell)
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="../thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release --parallel

# 3. Build for Linux (Bash)
mkdir build; cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="../thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake"
make -j$(nproc)

# 4. Build for Android (Linux)
# Set your NDK path
export ANDROID_NDK_HOME=/path/to/android-sdk/ndk/25.2.9519653
# Build host tools first
chmod +x ./build_tools.sh
./build_tools.sh
# Build the APK
chmod +x ./build_android.sh
./build_android.sh
```

---

## ⚖️ Disclaimer
*Unleashed Recompiled: Android Edition* is an unofficial fan-made project. It is not affiliated with, authorized, or endorsed by SEGA® or Sonic Team™. All trademarks belong to their respective owners.

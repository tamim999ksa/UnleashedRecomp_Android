<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

<h1 align="center">Sonic Unleashed Recompiled</h1>

<p align="center">
  <strong>Fork from https://github.com/hedge-dev/UnleashedRecomp </strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android_|_Windows_|_Linux-blue?style=for-the-badge&logo=android" alt="Platforms"/>
  <img src="https://img.shields.io/badge/Vulkan-1.3-red?style=for-the-badge&logo=vulkan" alt="Vulkan"/>
  <img src="https://img.shields.io/badge/Audio-Oboe_/_AAudio-green?style=for-the-badge" alt="Audio"/>
  <img src="https://img.shields.io/badge/Status-Experimental_WIP-orange?style=for-the-badge" alt="Status"/>
</p>

---

## 🌟 Sonic Unleashed Android Recompilation

This repository is a recompilation project of the Xbox 360 version of **Sonic Unleashed**. By translating original PowerPC binaries into native machine code (**ARM64** / **x86-64**). Fork includes Android support with **Vulkan 1.3**, **Oboe High-Performance Audio**, and **NDK 29** optimizations.

> [!IMPORTANT]
> **Game assets are NOT included.** You must provide your own legally acquired Xbox 360 copy of *Sonic Unleashed* and its updates.

---

## 🚀 Technical Stack

This fork has been optimized and modernized

### 🏎️ Engine & Performance Optimizations
- **Advanced Architecture:** Native optimization for **ARMv8.2-A** (including Crypto, CRC, DotProd, and FP16 instructions) on Android.
- **Ultra-Aggressive Compilation:** Utilizes **Full LTO (Link Time Optimization)**, **O3** optimization levels, and **Fast Math**.
- **Parallel Execution Engine:** Powered by **Intel TBB** and C++20 parallel algorithms to accelerate asset loading, hashing, and GPU pipeline pre-compilation.
- **Background Pipeline Pre-compilation:** Background compilation of graphics pipelines (MSAA, Blur, etc.) .
- **Zero-Allocation I/O:** High-performance **STFS** and **SVOD** parsing with zero-allocation string handling and memory-mapped I/O.

### 🎮 The Android Experience
- **Vulkan 1.3:** Utilizes the latest Vulkan standards for rendering efficiency and modern GPU features.
- **Oboe High-Performance Audio:** Integrated Google's **Oboe** library for the lowest possible audio latency using **AAudio**.
- **Sustained Performance Mode:** Native Android API integration to lock CPU/GPU clocks and prevent thermal throttling.
- **Enhanced HID & Input:** Refined controller handling with dynamic button prompt switching (Xbox/PS) and optimized multi-touch overlay.

### 🖥️ Desktop & Modern UX
- **Cross-Platform Mastery:** Full native support for **Windows** and **Linux (including Steam Deck)**.
- **Visual Fidelity:** Native support for **Resolution Scaling**, **Ultrawide Aspect Ratio** patches, and high-fidelity shadow resolution (up to 8K).
- **Modding Support:** Seamless integration with the **Hedge Mod Manager** ecosystem.

---

## 📋 System Requirements

| Requirement | Android | Windows / Linux |
| :--- | :--- | :--- |
| **Architecture** | ARMv8.2-A 64-bit REQUIRED | x86-64 (Amd64) |
| **OS Version** | Android 8.0+ (Targeting Android 15) | Win 10/11 / Ubuntu 24.04+ |
| **Graphics API** | Vulkan 1.3 REQUIRED | Vulkan 1.2+ |
| **RAM** | 4 GB (Strict Guest Allocation) | 8 GB+ Recommended |
| **Storage** | 10-15 GB (High-speed internal) | 10-15 GB |

---

## 🚀 Installation & Build Guide

### 🌐 GitHub Actions (Recommended)
Build for any platform without local setup:
1.  **Fork** this repository.
2.  Go to the **Actions** tab -> **Release** workflow -> **Run workflow**.
3.  Select your target OS (**Android, Windows, or Linux**).
4.  **Dynamic Asset Ingestion:** Provide URLs for your assets (**ZIP, RAR, 7Z, ISO, or XEX**) — the CI extracts and prepares them automatically!

### 💻 Manual Build (Developer Path)

#### 📦 Prerequisites (Common)
- `cmake` (3.22+), `git`, `ninja-build`, `libtbb-dev`.
- **Android:** Android SDK, **NDK 29.0.14206865**, **Java 17**.
- **Windows:** **Visual Studio 2022** with **Clang-cl** and **LLVM 18+**.
- **Linux:** `gcc-13` / `g++-13` and Vulkan SDK.

#### 🛠️ Build Steps

##### **Windows**
```powershell
cmake --preset x64-Clang-Release
cmake --build out/build/x64-Clang-Release --config Release --parallel
```

##### **Linux**
```bash
cmake --preset linux-release
cmake --build out/build/linux-release --config Release --parallel

# Optional Flatpak
flatpak-builder --user --install --force-clean build flatpak/io.github.hedge_dev.unleashedrecomp.json
```

##### **Android**
```bash
# 1. Build host-side tools first
chmod +x ./build_tools.sh && ./build_tools.sh

# 2. Set NDK path (REQUIRED: 29.0.14206865)
export ANDROID_NDK_HOME=/path/to/android-sdk/ndk/29.0.14206865

# 3. Compile the APK
chmod +x ./build_android.sh && ./build_android.sh
```

---

## ⚖️ Disclaimer
*Sonic Unleashed Recompiled* is an unofficial fan-made project. It is not affiliated with, authorized, or endorsed by SEGA® or Sonic Team™. This project is intended for educational purposes and requires legally owned game assets.

<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

<h1 align="center">Unleashed Recompiled: Android Edition</h1>

<p align="center">
  <strong>Static recompilation of Sonic Unleashed (Xbox 360) running natively on Android.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Android-green?style=flat-square&logo=android" alt="Platform"/>
  <img src="https://img.shields.io/badge/Architecture-ARM64-blue?style=flat-square" alt="Arch"/>
  <img src="https://img.shields.io/badge/Graphics-Vulkan_1.2-red?style=flat-square&logo=vulkan" alt="Vulkan"/>
  <img src="https://img.shields.io/badge/Status-Work_In_Progress-orange?style=flat-square" alt="Status"/>
</p>

---

This project is an **experimental, work-in-progress port** of [Sonic Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) to the Android platform. It leverages static recompilation to translate Xbox 360 PowerPC instructions into native ARM64 code, delivering high-performance gameplay on modern mobile devices.

> [!IMPORTANT]
> **This repository does NOT contain any game assets.** You must provide your own legally acquired copy of *Sonic Unleashed* for the Xbox 360.

---

## ✨ Key Features & Enhancements

This fork introduces significant architectural improvements and mobile-specific optimizations:

### 🛠️ Core & Kernel
- **Modular Architecture:** Refactored the monolithic kernel into clean, modular components (`threading`, `synchronization`, `I/O`) for improved stability and maintainability.
- **Advanced File System:** Native support for Xbox 360 **STFS** and **SVOD** (XContent) parsing with memory-mapped I/O for lightning-fast asset loading.
- **Modding Support:** Integrated `ModLoader` with a thread-local lookup cache to minimize file system overhead during modded gameplay.
- **Universal Save Redirection:** Automatically manages and redirects save data to ensure persistence across updates and mod configurations.
- **Achievement System:** Includes a native, built-in **Achievement Overlay** and manager, faithfully recreating the original Xbox 360 experience.

### 🎮 Mobile Experience
- **Vulkan-First Rendering:** Optimized pipeline specifically for **Vulkan 1.2**, ensuring the best performance on modern mobile GPUs.
- **Native Touch Controls:** Fully customizable on-screen overlay designed for mobile-first gameplay.
- **Controller Support:** Plug-and-play support for Xbox, PlayStation, and generic Bluetooth/HID controllers with dynamic icon switching.
- **Low-Latency Audio:** Utilizes **AAudio** and **Oboe** backends for perfectly synced sound effects and music.

---

## 📱 Device Requirements

| Requirement | Minimum Specification | Recommended |
| :--- | :--- | :--- |
| **Architecture** | ARMv8-A 64-bit (ARM64) | Latest flagship SoC (Snapdragon 8 Gen 1+) |
| **OS Version** | Android 8.0 (API 26) | Android 11+ |
| **Graphics API** | Vulkan 1.2 | Vulkan 1.3 |
| **RAM** | 4 GB (Strict Guest Allocation) | 8 GB+ |
| **Storage** | 10 GB (Internal Storage) | 15 GB+ (UFS 3.0+) |

---

## 🚀 Getting Started

### 1. Prepare Your Assets
Create a `private/` directory in the project root and place the following files:
- `default.xex` — The main game executable.
- `default.xexp` — Title update (optional).
- `shader.ar` — DLC or shader archives (optional).
- Game data containers (STFS/SVOD).

### 2. Build via GitHub Actions (Easy)
The CI pipeline is highly robust and can ingest assets automatically:
1. **Fork** this repository.
2. Navigate to the **Actions** tab.
3. Select the **Release** workflow and click **Run workflow**.
4. **Automated Asset Ingestion:** You can provide URLs for your `game_url`, `update_url`, and `dlc_url`. The workflow supports **ZIP, ISO, and XEX** formats and will automatically extract and prepare them for the build.
5. Download the final APK from the **Artifacts** section.

### 3. Manual Build (Advanced)
For developers building locally on Linux or macOS.

#### 📦 Dependencies
- `gcc`, `g++`, `cmake` (3.20+), `git`, `wget`, `unzip`
- `libtbb-dev` (Intel Threading Building Blocks)
- Android SDK & **NDK 25.2.9519653**
- `patchelf`

#### 🛠️ Build Steps
```bash
# 1. Clone with submodules
git clone --recursive https://github.com/yourusername/UnleashedRecomp-Android.git
cd UnleashedRecomp-Android

# 2. Build host-side tools (Recompiler, asset handlers, etc.)
chmod +x ./build_tools.sh
./build_tools.sh

# 3. Configure environment
export ANDROID_NDK_HOME=/path/to/android-sdk/ndk/25.2.9519653

# 4. Compile the APK
chmod +x ./build_android.sh
./build_android.sh
```
Find your APK at: `android/app/build/outputs/apk/debug/app-debug.apk`

---

## ⚖️ Disclaimer
*Unleashed Recompiled: Android Edition* is an unofficial fan-made project. It is not affiliated with, authorized, or endorsed by SEGA® or Sonic Team™. This project is intended for educational and interoperability purposes. All trademarks belong to their respective owners.

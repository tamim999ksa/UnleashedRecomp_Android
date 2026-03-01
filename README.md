<p align="center">
    <img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512"/>
</p>

---

# Unleashed Recompiled: Android Edition

This is an **experimental, work-in-progress Android port** of [Sonic Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp).
It utilizes static recompilation of the Xbox 360 version of *Sonic Unleashed* to run natively on ARM64 Android devices.

**Status:** 🚧 **Work In Progress (WIP)** 🚧
**End Goal:** A fully playable, high-performance experience of Sonic Unleashed on Android with native Vulkan rendering and modern enhancements.

## Key Differences & Improvements

Compared to the original PC version, this fork introduces several architectural changes and optimizations:

- **Modular Kernel implementation:** The monolithic `imports.cpp` has been refactored into modular components (`threading.cpp`, `synchronization.cpp`, `io/nt.cpp`, etc.) for better maintainability and isolated testing.
- **Advanced File System Support:** Native implementation of Xbox 360 **STFS** and **SVOD** (XContent) parsing, allowing the game to read assets directly from container files with optimized memory-mapped I/O.
- **Optimized Mod Loading:** Implemented a thread-local mod lookup cache (`CachedArchivePath`) in the `ModLoader` to significantly reduce file system overhead during gameplay.
- **Enhanced Security & Stability:** Hardened guest-to-host memory copies in the kernel (e.g., `fillFindData`) to prevent buffer overflows and improve host-side robustness.
- **Improved Build Pipeline:** Optimized the recompilation batch size (50 instructions per file) to reduce build artifacts and improve compilation speed on resource-constrained CI environments.
- **Vulkan-First Architecture:** Tailored rendering logic specifically for Vulkan to ensure maximum compatibility and performance on mobile GPUs.
- **Optimized Asset Integrity:** Uses streaming SHA256 (PicoSHA2) and XXH3 for fast and memory-efficient file verification during installation.

## Android Device Requirements (Based on Code)

- **Architecture:** ARMv8-A 64-bit (ARM64 / `arm64-v8a`) **REQUIRED**.
- **OS Version:** Android 8.0 (Oreo, API 26)+ or higher.
- **GPU:** Vulkan 1.2 support **REQUIRED**.
- **RAM:** 4 GB Minimum (6 GB+ strongly recommended).
- **Storage:** **10-15 GB** of high-speed internal storage for game assets and shader caches.
- **Audio:** Utilizes **AAudio / Oboe** backends for low-latency audio performance on modern Android devices. (Note: Some kernel audio functions are currently stubs).

## How to Build

### 1. Prepare Game Assets
The build system requires original game files to be placed in a `private/` directory in the project root:
- `default.xex` (Main executable)
- `default.xexp` (Title update, optional)
- `shader.ar` (DLC/Shader assets, optional)
- STFS/SVOD containers (Game data)

### 2. Build using GitHub Actions (Recommended)
1. Fork this repository.
2. Go to the **Actions** tab and select the **Release** workflow.
3. Click **Run workflow**. You can provide URLs to your game assets, or the workflow will look for them in your forked repository's `private/` folder.
4. Download the resulting APK from the artifacts once complete.

### 3. Manual Build (Linux/macOS)
Ensure you have the **Android SDK**, **NDK (25.2.9519653)**, and **CMake** installed.

```bash
# Set your NDK path if not auto-detected
export ANDROID_NDK_HOME=/path/to/android-sdk/ndk/25.2.9519653

# Run the build script
./build_android.sh
```
The APK will be generated at: `android/app/build/outputs/apk/debug/app-debug.apk`.

## Disclaimer
This project is an unofficial fan-made port and is not affiliated with SEGA or Sonic Team. No game assets are included in this repository.

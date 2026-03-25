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

This fork has been optimized and modernized, still WIP so expect build to break.

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

### 🌐 GitHub Actions (Recommended — No Local Setup Required)

The **Release** workflow builds the APK (and optionally packages game data) entirely in the cloud.

1. **Fork** this repository.
2. Go to the **Actions** tab → **Release** workflow → **Run workflow**.
3. Choose **Target OS** (Android, Windows, or Linux).
4. Provide your **game data URLs**:
   - **game\_url** *(required)*: URL to your game disc (ISO, XEX, ZIP, 7Z, or RAR).
   - **update\_url** *(optional)*: URL for the title update (default.xexp).
   - **dlc\_url** *(optional)*: URL for shader.ar (shader data).
5. Choose a **delivery mode** via the `embed_game_files` checkbox:

#### Delivery Mode A — Embedded (default ✅)
- Game data is compressed and baked directly into the APK.
- **Result:** One large APK file (~3-6 GB). Install and play — no extra steps on device.

#### Delivery Mode B — Sideloaded (uncheck `embed_game_files`)
- Game data is packaged as a separate downloadable archive.
- **Result:** Two artifacts:
  - `UnleashedRecomp-Android-APK` — small APK (~50 MB)
  - `UnleashedRecomp-GameData` — `game_data.tar.zst` archive (~3-5 GB compressed)
- **Setup on device:**
  1. Install the APK.
  2. Launch the app once (it creates the game directory).
  3. Download the `UnleashedRecomp-GameData` artifact from GitHub Actions and extract the zip.
  4. Copy `game_data.tar.zst` to the game directory on your device:
     ```
     /Android/data/com.hedge_dev.UnleashedRecomp/files/UnleashedRecomp/
     ```
  5. Launch the app — it auto-detects the archive, extracts all game files, then deletes the archive to free space.

> [!TIP]
> On Android, use a file manager that can access `/Android/data/` (e.g., MT Manager, MiXplorer, or `adb push`).

---

### 💻 Manual Build (Local)

#### 📦 Prerequisites

| Tool | Android | Linux | Windows |
| :--- | :--- | :--- | :--- |
| **CMake** | 3.22+ | 3.22+ | 3.22+ |
| **Git** | ✅ | ✅ | ✅ |
| **Ninja** | ✅ | `ninja-build` | via VS |
| **C++ Compiler** | NDK provides | `gcc-13` / `g++-13` | **Visual Studio 2022** + Clang-cl + LLVM 18+ |
| **Java** | JDK 17 | — | — |
| **Android SDK** | ✅ | — | — |
| **Android NDK** | **29.0.14206865** | — | — |
| **p7zip** | `p7zip-full` | `p7zip-full` | — |
| **TBB** | — | `libtbb-dev` | — |
| **libcurl** | — | `libcurl4-openssl-dev` | — |
| **patchelf** | — | `patchelf` | — |
| **Vulkan SDK** | — | ✅ | — |

#### 🛠️ Build Steps

##### **Android**
```bash
# 1. Place your game assets in a `private/` folder at the repo root:
#    private/default.xex   (game executable)
#    private/default.xexp   (title update patch)
#    private/shader.ar      (shader archive)
#    private/<any>.iso      (game disc - extracted automatically)
#    The build script handles all extraction and normalization automatically.

# 2. Install Linux build dependencies:
sudo apt-get install -y cmake build-essential ninja-build p7zip-full \
    libtbb-dev libcurl4-openssl-dev patchelf

# 3. Set NDK path (REQUIRED: version 29.0.14206865)
export ANDROID_NDK_HOME=/path/to/android-sdk/ndk/29.0.14206865

# 4. Run the full build (builds host tools + applies patches + compiles APK)
chmod +x ./build_android.sh && ./build_android.sh

# The APK will be at: android/app/build/outputs/apk/debug/app-debug.apk
```

##### **Linux**
```bash
# 1. Install dependencies
sudo apt-get install -y cmake build-essential ninja-build gcc-13 g++-13 \
    libtbb-dev libcurl4-openssl-dev patchelf p7zip-full

# 2. Place game assets in private/ and build host tools
chmod +x ./build_tools.sh && ./build_tools.sh

# 3. Apply patches
chmod +x ./apply_patches.sh && ./apply_patches.sh

# 4. Build
mkdir -p build_linux && cd build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="../thirdparty/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DHOST_TOOLS_DIR="../build_tools/bin"
cmake --build . --config Release --parallel $(nproc)
```

##### **Windows**
```powershell
# Requires Visual Studio 2022 with Clang-cl and LLVM 18+
cmake --preset x64-Clang-Release
cmake --build out/build/x64-Clang-Release --config Release --parallel
```

---

## 📂 Game Data Structure

On Android, game files are stored at:
```
/Android/data/com.hedge_dev.UnleashedRecomp/files/UnleashedRecomp/
```

The required directory structure under the game path:
```
UnleashedRecomp/
├── game/                    # Base game files
│   ├── default.xex          # Original game executable
│   ├── *.ar.00, *.ar.01...  # Game archives (levels, models, textures)
│   ├── *.arl                # Archive lists
│   └── voices/              # Voice data
├── update/
│   └── default.xexp         # Title update patch
└── patched/
    └── default.xex          # Patched executable (auto-generated at first launch)
```

---

## ⚖️ Disclaimer
*Sonic Unleashed Recompiled* is an unofficial fan-made project. It is not affiliated with, authorized, or endorsed by SEGA® or Sonic Team™. This project is intended for educational purposes and requires legally owned game assets.

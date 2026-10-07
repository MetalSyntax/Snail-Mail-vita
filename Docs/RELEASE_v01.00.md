<!--
  GitHub Release body for v01.00.
  Title: "v01.00 — Initial Release: Snail Mail for PS Vita"
-->

# Snail Mail (PS Vita) — v01.00 Initial Release

First public, fully playable release of **Snail Mail** (originally developed by Sandlot Games) for the PlayStation Vita, powered by the Android ARM dynamic library loader (soloader) and FalsoJNI.

---

## 🚀 Features & Highlights

- **Native Hardware Rendering**:
  - OpenGL ES 1.1 running at the PlayStation Vita's native 960x544 resolution via vendored `vitaGL` built with `softfp` ABI.
  - Smooth, locked 60 FPS performance.
- **Resource Streaming & Fast In-Memory Decompression**:
  - Direct C-level hooking for resources packed inside `asm.mp3` (Sandlot Games DAT resource archive).
  - High-speed in-memory texture decoding (PNG and JPEG via `stb_image.h`) with vertical row flip for TGA format compliance.
  - High-speed in-memory ZIP raw deflation powered by `zlib`.
- **Full Multichannel Audio**:
  - Simultaneous background music (BGM) looping and sound effects (SFX) decoded in real time with Tremor (`libvorbisidec`).
  - Dedicated audio mixer thread on CPU Core 1 (`SceAudioOut`) at 44100 Hz stereo.
- **Save Games & Configuration**:
  - Transparent and persistent game state and options saving directly to `ux0:data/snailmail/saves/`.
- **LiveArea Integration**:
  - Custom 8-bit indexed colormapped icon, background banner, and splash screen conforming to official PS Vita standards.

---

## 🚨 Known Issues / Current Limitations

> [!WARNING]
> **Important**: Please keep the following limitations in mind when playing this initial release:
> 
> 1. **Controls Function in Tilt Mode Only**:
>    - Physical analog controls (Left Analog Stick and D-Pad) **only steer Turbo if "Tilt Mode" is selected in the game options**.
>    - Physical controls emulate Android's native accelerometer (`JNIAccelerometer`).
>    - **Touch Mode is currently a work-in-progress (WIP)** and will be expanded in a future update.
> 2. **Menu Navigation Requires Front Touchscreen**:
>    - **Main menus and level selection screens cannot be navigated using physical buttons or the D-Pad**.
>    - Please use the **front touchscreen** of your PS Vita to tap menu buttons, select levels, and adjust game options.

---

## 📦 Installation Instructions

1. Install `snailmail.vpk` on your PS Vita using **VitaShell**.
2. Ensure you have the required plugins installed in your `ur0:tai/config.txt`:
   ```ini
   *KERNEL
   ur0:tai/kubridge.skprx
   ur0:tai/fd_fix.skprx
   ```
3. Ensure runtime shader compiler `libshacccg.suprx` is installed on your console (extractable with `ShaRKBR33D`).
4. Extract the data files from your legally purchased Android APK and place them inside `ux0:data/snailmail/`:
   - Copy `lib/armeabi-v7a/libsnailmail.so` (or `lib/armeabi/libsnailmail.so`) to `ux0:data/snailmail/libsnailmail.so`.
   - Copy `assets/asm.mp3` to `ux0:data/snailmail/assets/asm.mp3`.
   - Copy all 118 `.ogg` audio files from `assets/` to `ux0:data/snailmail/assets/`.

### Folder Layout Reference

```
ux0:data/snailmail/
├── libsnailmail.so
├── assets/
│   ├── asm.mp3
│   ├── mainmenu.ogg
│   ├── music1.ogg
│   ├── ... (all remaining .ogg audio files)
├── logs/                   <- created automatically (snailmail_NNN.log)
└── saves/                  <- created automatically (savegame and settings)
```

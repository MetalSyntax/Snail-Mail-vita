<!--
  GitHub Release body for v01.01.
  Title: "v01.01 — Physical Controls & Stability Update"
-->

# Snail Mail (PS Vita) — v01.01 Physical Controls & Stability Update

This update makes Snail Mail fully playable with physical buttons (no touchscreen required), adds an in-game controls menu, and hardens the renderer against several GPU-hang causes found on real hardware.

---

## 🎮 Controls

- **Steering in both Tilt and Touch modes**: the Left Analog Stick and D-Pad steer Turbo no matter which mode is selected in the game options. Tilt Mode now receives a stable, centered accelerometer vector, so the screen no longer drifts.
- **Shooting with Cross (X) / R1**: fires from Turbo's current lane (left, center or right) without dragging him to the center of the road. Hold to auto-fire while you keep steering.
- **Pause with START**: opens the game's own pause menu.
- **Menu navigation without touch**: the D-Pad and Left Analog Stick move a virtual cursor (the button under it is highlighted); Cross / R1 tap the button under the cursor. Works in the main menu, options, level select, Continue screens and tutorial cards.
- **Controls menu (START + SELECT)**: remap SHOOT, STEER LEFT, STEER RIGHT and PAUSE, adjust DEADZONE (5–50%) and SENSITIVITY (20–300%), or reset to defaults. Settings are saved automatically to `ux0:data/snailmail/controls.txt` (editable by hand).

## 🛠️ Stability Fixes

- **Crash after completing a lap / level**: the loader no longer feeds steering/cursor input to the engine while the mouse sub-object it frees between levels is missing.
- **Crash on the splash screen**: the virtual menu cursor is no longer sent to the engine before its menus exist.
- **Green/cyan tint over the whole game**: the controls overlay now saves and restores the GL state and resets the color to white, so its color no longer leaks into the game.
- **Controls menu rendering**: fixed mirrored text, and the overlay now uses far less GPU memory. A fixed 2 MB buffer used to overflow and corrupt GPU memory; the buffer is now only reserved while the menu is open.
- **GPU hang hardening**:
  - vitaGL now skips fixed-function draws whose bound texture is not valid (never uploaded or already deleted), instead of handing freed/null memory to the GPU.
  - Every engine draw call is validated against the tracked GPU buffers (index/vertex ranges and finite positions). Invalid draws are skipped and reported in the log, with the engine function that issued them.

---

## 🚨 Known Issues

> [!WARNING]
> - **GPU hang during the tutorial (under observation)**: earlier builds froze at a fixed point of the tutorial (when Turbo becomes invincible). With this release the tutorial has been completed on real hardware without freezing, but the exact trigger is not yet confirmed. If it still happens to you, please share `ux0:data/snailmail/logs/snailmail_NNN.log` and the crash dump from `ux0:data/`.
> - A few `Cannot find Texture X/...` messages appear in the log while loading; they come from the original game data (same on Android) and are harmless.
> - **OpenFeint** (online leaderboards/achievements) is discontinued and not available.

---

## 📦 Installation / Update

- **Updating from v01.00**: just install the new `snailmail.vpk` with **VitaShell**. Your data files and saves in `ux0:data/snailmail/` are kept.
- **Fresh install**: follow the [installation instructions](../README.md#installation-instructions) in the README. You need `kubridge.skprx`, `fd_fix.skprx`, `libshacccg.suprx`, and the data files from your own legally purchased Android APK (`libsnailmail.so`, `assets/asm.mp3`, and the `.ogg` audio files).

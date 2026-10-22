<!--
  GitHub Release body for v01.03.
  Title: "v01.03 — Save Data Persistence & Audio Glitch Fix"
-->

# Snail Mail (PS Vita) — v01.03 Update

This update fixes save game persistence (avoiding having to restart from the tutorial every time) and eliminates audio crackling / popping artifacts, especially noticeable when playing with headphones.

---

## 🛠️ Fixes

- **Save Data Persistence**:
  - **Issue**: Each time the game was launched, progress was lost and the player was forced to start from the tutorial level.
  - **Cause**: The save hooks (`PfmSaveFile` / `JAVASaveFile`) attempted to write save state into `ux0:data/snailmail/saves/`. Because that directory was not created automatically upon startup, `fopen` failed silently with `NULL`, preventing any progress or settings from saving.
  - **Fix**: The loader now ensures `ux0:data/snailmail/saves/` is automatically created on startup via `file_mkpath()`. Progress and options now persist across restarts.

- **Audio Crackling / Pop Elimination**:
  - **Issue**: Periodic crackling or popping noises occurred during gameplay, particularly noticeable when listening through headphones.
  - **Cause**: The linear resampler in `voice_decode` was referencing samples from `stage[stage_pos]` *after* advancing the source frame pointer via `voice_next_src_frame()`. Whenever the staging buffer was refilled, it read stale or misaligned frame samples (or one frame out-of-bounds in stereo), introducing periodic glitches on any sound with a sample rate other than 44.1 kHz. In addition, sample volumes were not strictly clamped to 1.0f, which could introduce clipping.
  - **Fix**: Rewrote the interpolation loop to cleanly track the last two decoded frames (`prev` and `cur`) without reading invalid indices from the stage buffer. Clamped SFX playback volume to 1.0f to prevent int16 clipping.

---

## 🚨 Known Issues

> [!WARNING]
> - **Occasional stutter during races**: short hitches can happen during gameplay. They do not affect progress; investigation is ongoing.
> - A few `Cannot find Texture X/...` messages appear in the log while loading. They come from the original game data (the same happens on Android) and are harmless.
> - **OpenFeint** (online leaderboards/achievements) is discontinued and not available.

---

## 📦 Installation / Update

- **Updating from previous versions (v01.00 / v01.01 / v01.02)**: install the new `snailmail.vpk` with **VitaShell**. Your existing data files and saves in `ux0:data/snailmail/` are preserved.
- **Fresh install**: follow the [installation instructions](../README.md#installation-instructions) in the README.

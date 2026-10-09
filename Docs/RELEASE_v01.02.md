<!--
  GitHub Release body for v01.02.
  Title: "v01.02 — Hotfix: GPU freeze after the third weapon power-up"
-->

# Snail Mail (PS Vita) — v01.02 Hotfix

This hotfix fixes the console freeze in the tutorial that v01.01 did not solve.

---

## 🛠️ Fixes

- **Freeze when firing after the third weapon power-up**: when Turbo collected the third power-up (the white rings) and fired, the game froze and the console reported a GPU crash.
  - **Cause**: the model of that weapon's shot has an index count that does not form whole triangles (8 indices for a triangle list). On Android the OpenGL ES driver ignores the leftover indices. On PS Vita they were passed straight to the GPU, which hung.
  - **Fix**: the loader now trims every draw call to whole primitives, the same way OpenGL ES does. The weapon looks the same as on Android.
  - Verified on real hardware: the full tutorial race was completed, collecting every power-up and firing all weapons.
- **Renderer hardening** (defensive, no visible change):
  - vitaGL's `glPushMatrix`/`glPopMatrix` now always check the stack limits. Before, an unbalanced push/pop could corrupt vitaGL's memory. The projection and texture stacks also have room for 4 matrices instead of 2.
  - Out-of-range texture IDs are bound as "no texture" instead of corrupting vitaGL's texture table.

---

## 🚨 Known Issues

> [!WARNING]
> - **Occasional stutter during races**: short hitches can happen during gameplay. They do not affect progress; investigation is ongoing.
> - A few `Cannot find Texture X/...` messages appear in the log while loading. They come from the original game data (the same happens on Android) and are harmless.
> - **OpenFeint** (online leaderboards/achievements) is discontinued and not available.

---

## 📦 Installation / Update

- **Updating from v01.00 / v01.01**: install the new `snailmail.vpk` with **VitaShell**. Your data files and saves in `ux0:data/snailmail/` are kept.
- **Fresh install**: follow the [installation instructions](../README.md#installation-instructions) in the README.

<!--
  GitHub Release body for v01.00.
  Title: "v01.00 — Initial Release: Snail Mail for PS Vita"
-->

# Snail Mail (PS Vita) — v01.00 Initial Release

Primera versión pública y jugable del port de **Snail Mail** (Sandlot Games) para PlayStation Vita, basada en el cargador dinámico de librerías ARM de Android (soloader) y FalsoJNI.

---

## 🇪🇸 Novedades y Características

- **Motor y Renderizado Nativo**:
  - Renderizado OpenGL ES 1.1 a resolución nativa completa de PS Vita (960x544) mediante `vitaGL` vendorizado con soporte `softfp`.
  - Tasa de refresco fluida a 60 FPS estables.
- **Carga de Recursos y Descompresión**:
  - Interceptación directa en C de archivos empaquetados en `asm.mp3` (archivo de recursos DAT).
  - Decodificación en memoria ultrarrápida de texturas JPEG/PNG (`stb_image.h`) con orientación corregida para formato TGA.
  - Descompresión en memoria con `zlib` (raw deflate).
- **Audio Multicanal Completo**:
  - Reproducción de música en bucle (BGM) y efectos de sonido (SFX) simultáneos decodificados en tiempo real con Tremor (`libvorbisidec`).
  - Hilo de mezcla de audio dedicado en CPU Core 1 (`SceAudioOut`) a 44100 Hz estéreo.
- **Partidas Guardadas y Configuración**:
  - Guardado transparente y persistente en `ux0:data/snailmail/saves/`.
- **Integración LiveArea**:
  - Iconos, fondos e imágenes de arranque adaptados a los estándares de PS Vita.

---

## 🚨 Problemas Conocidos / Issues Actuales

> [!WARNING]
> **Importante**: Por favor, ten en cuenta las siguientes limitaciones en esta versión inicial:
> 
> 1. **El juego solo funciona con los controles en Tilt Mode (Modo Inclinación)**:
>    - Para jugar usando los controles analógicos físicos (Stick Analógico Izquierdo o Cruceta D-Pad), debes asegurarte de tener seleccionado **Tilt Mode** en las opciones del juego.
>    - Los controles físicos emulan el sensor de acelerómetro de Android (`JNIAccelerometer`).
>    - **Se está trabajando actualmente en la implementación para Touch Mode**.
> 2. **Navegación de menús solo por Pantalla Táctil**:
>    - **No se puede navegar por el menú con los controles físicos**.
>    - Utiliza la **pantalla táctil frontal** de tu PS Vita para seleccionar las opciones de menú, cambiar niveles y configurar ajustes.

---

## 🇬🇧 Release Notes (English)

- **Native Hardware Rendering**:
  - OpenGL ES 1.1 running at native 960x544 resolution powered by vendored `vitaGL` (compiled with `softfp`).
  - Smooth 60 FPS performance.
- **Resource Streaming & Fast Decompression**:
  - Direct C-level hooking for resources packed in `asm.mp3` (Sandlot DAT archive).
  - High-speed in-memory texture decoding (PNG/JPEG via `stb_image.h`) with correct TGA orientation.
  - In-memory Zip raw deflation with `zlib`.
- **Full Multichannel Audio**:
  - Simultaneous BGM and sound effects powered by Tremor (`libvorbisidec`).
  - Dedicated Core 1 audio mixer thread at 44100 Hz stereo.
- **Save Games & Settings**:
  - Native savefile persistence to `ux0:data/snailmail/saves/`.
- **LiveArea Assets**:
  - Custom 8-bit colormapped LiveArea banner, icon, and splash screen.

### ⚠️ Known Issues

1. **Controls only work in Tilt Mode**:
   - The game currently only responds to physical controls (Left Analog Stick / D-Pad) if **Tilt Mode** is selected in the game options.
   - Tilt mode maps stick movements to Android's `JNIAccelerometer`.
   - Work is currently in progress for **Touch Mode**.
2. **Menu navigation is touchscreen only**:
   - Menus cannot be navigated using physical buttons or D-Pad.
   - Please use the **front touchscreen** to select items in menus.

---

## Instrucciones de Instalación / Installation Instructions

1. Instala `snailmail.vpk` usando **VitaShell**.
2. Asegúrate de tener los plugins requeridos en `ur0:tai/config.txt`:
   ```ini
   *KERNEL
   ur0:tai/kubridge.skprx
   ur0:tai/fd_fix.skprx
   ```
3. Instala `libshacccg.suprx` en la consola (vía `ShaRKBR33D`).
4. Extrae los archivos de tu copia legítima del APK de Android en `ux0:data/snailmail/`:
   - `ux0:data/snailmail/libsnailmail.so`
   - `ux0:data/snailmail/assets/asm.mp3`
   - Todos los archivos `.ogg` de sonido dentro de `ux0:data/snailmail/assets/`.

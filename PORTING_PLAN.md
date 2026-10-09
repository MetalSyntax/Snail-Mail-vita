# Plan de Port — Snail Mail (PS Vita)

> Actualizado tras ingeniería inversa y compilación completa el 2026-10-07.

## 0. Contexto

- **Juego:** Snail Mail
- **Paquete Java:** com.sandlotgames.snailmail
- **APK original:** `com.sandlotgames.snailmail-1.00-paid-www.apksum.com.apk`
- **TITLEID asignado:** `PSVSM0001`
- **Motor:** Motor propietario C++ de Sandlot Games con capa de abstracción de plataforma `Pfm`.

## 1. Detección de Arquitectura y Gráficos

- **ABI:** armeabi-v7a (ARMv7 hard-float/NEON, nativo para Cortex-A9 de PS Vita).
- **Versión de GLES:** OpenGL ES 1.1 (Fixed-Function Pipeline con vitaGL: `glMatrixMode`, `glPushMatrix`, `glVertexPointer`, `glTexCoordPointer`, `glEnableClientState`, `glFogfv`, etc.).

## 2. Binarios y Datos

- `ux0:data/snailmail/libsnailmail.so` (655 KB): librería dinámica nativa principal.
- `ux0:data/snailmail/assets/asm.mp3` (7.8 MB): archivo DAT comprimido que contiene todos los recursos del juego (modelos 3D, texturas, niveles, fuentes).
- `ux0:data/snailmail/assets/*.ogg`: 118 archivos de audio Vorbis (música y efectos de sonido).

## 3. Exports JNI Confirmados (`libsnailmail.so`)

1. `Java_com_sandlotgames_snailmail_ADRenderer_nativeInit`
2. `Java_com_sandlotgames_snailmail_ADRenderer_nativeReInit`
3. `Java_com_sandlotgames_snailmail_ADRenderer_nativeResize`
4. `Java_com_sandlotgames_snailmail_ADRenderer_nativeRender`
5. `Java_com_sandlotgames_snailmail_ADRenderer_nativeDone`
6. `Java_com_sandlotgames_snailmail_ADRenderer_JNIAudioInit`
7. `Java_com_sandlotgames_snailmail_ADRenderer_JNIOFOSubmitCB`
8. `Java_com_sandlotgames_snailmail_ADRenderer_JNIOFOUnlockCB`
9. `Java_com_sandlotgames_snailmail_ADGLSurfaceView_JNIMouseEvent`
10. `Java_com_sandlotgames_snailmail_ADGLSurfaceView_nativePause`
11. `Java_com_sandlotgames_snailmail_ADGLSurfaceView_JNIKey`
12. `Java_com_sandlotgames_snailmail_SnailMailActivity_JNIDatInit`
13. `Java_com_sandlotgames_snailmail_SnailMailActivity_JNIDatUnInit`
14. `Java_com_sandlotgames_snailmail_SnailMailActivity_JNIDebug`
15. `Java_com_sandlotgames_snailmail_SnailMailActivity_JNIOFOSave`
16. `Java_com_sandlotgames_snailmail_SnailMailActivity_JNIResourceManagerInvalidate`
17. `Java_com_sandlotgames_snailmail_AccelerometerListener_JNIAccelerometer`
18. `Java_com_sandlotgames_snailmail_MyOpenFeintDelegate_JNIOFOInit`

## 4. Checklist

- [x] Repo creado desde soloader-boilerplate, git init, .gitignore anti-DMCA.
- [x] APK decompilado (jadx) y .so decompilado(s) (Ghidra/objdump).
- [x] Análisis del motor real (capa de abstracción `Pfm`, ciclo de vida JNI, archivo de assets DAT `asm.mp3`).
- [x] Bootstrap del loader: `so_file_load`/`so_relocate`/`so_resolve`, compilación limpia del ejecutable.
- [x] Tabla JNI (FalsoJNI): registro de los 26 métodos de `ADRenderer` + ganchos nativos directos en `so_patch()`.
- [x] Gráficos: GLES 1.1 enlazado con `vitaGL`, resolución 960x544, buffer swap en render loop.
- [x] Input: táctil frontal vía `JNIMouseEvent`, stick analógico izquierdo y D-Pad mapeados a `JNIAccelerometer`, botones Cruz/R1 mapeados a disparar/saltar (Space + click), Start a pausa.
- [x] Menú de controles in-game (estilo Carnivores) con START+SELECT: remapeo de SHOOT/STEER/PAUSE, deadzone y sensibilidad, dibujado como overlay vitaGL con fuente 8x8 y guardado en `controls.txt`.
- [x] Acelerómetro estable: `JNIAccelerometer(0,0,1)` cada frame para pantalla centrada en modo Tilt.
- [x] Audio: subsistema completo multicanal con Tremor Vorbis (`libvorbisidec`) y `SceAudioOut` con thread dedicado en CPU Core 1.
- [x] Assets, LiveArea y VPK: LiveArea optimizado (PNG 8-bit indexados), `snailmail.vpk` generado exitosamente.
- [ ] Primer arranque en consola real.
- [ ] Pruebas en hardware real.

## 5. Herramientas y Despliegue

Este port se gestiona con **psvita-port-toolkit** (standalone).
- Despliegue a consola física: `psvita-toolkit deploy`
- Binarios generados:
  - `build/eboot.bin`
  - `build/snailmail.vpk` (instalable en PS Vita)
- Carpeta de datos a transferir a `ux0:data/snailmail/`:
  - `ux0_data/snailmail/libsnailmail.so`
  - `ux0_data/snailmail/assets/` (`asm.mp3` y archivos `.ogg`)
  - `ux0_data/snailmail/logs/` (con `next.idx`)
  - `ux0_data/snailmail/saves/`

## 6. Estándar de logs en consola

Configurado según el estándar canónico:
- Archivos en `<DATA_PATH>logs/snailmail_NNN.log` rotativos de 001 a 999 con `next.idx`.
- Validado con `psvita-toolkit log-standard`.

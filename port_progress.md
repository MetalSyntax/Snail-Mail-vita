# Registro de Progreso — Snail Mail (PS Vita)

## Fase 1: Configuración y Preparación (Completada — 2026-10-05)
- Repositorio inicializado desde soloader-boilerplate con `.gitignore` anti-DMCA.
- APK `com.sandlotgames.snailmail-1.00-paid-www.apksum.com.apk` extraído e inspeccionado.
- ABI detectada: armeabi-v7a (ARMv7 hard-float/NEON, nativa para PS Vita Cortex-A9).
- Limpieza de metadatos basura macOS AppleDouble (`._*`) ejecutada con `psvita-toolkit clean-junk`.

## Fase 2: Decompilación y Análisis de Símbolos (Completada — 2026-10-07)
- Decompilación Java (jadx) analizada en `decompiled/apk_jadx/sources/com/sandlotgames/snailmail/`.
- Clases principales del juego analizadas:
  - `ADRenderer`: ciclo de vida GLES (`nativeInit`, `nativeResize`, `nativeRender`, `nativeDone`) y llamadas callback Java (`JAVALoadSample`, `JAVAPlaySample`, `JAVASaveFile`, etc.).
  - `ADGLSurfaceView`: eventos de pantalla táctil (`JNIMouseEvent`), teclado (`JNIKey`) y pausa (`nativePause`).
  - `SnailMailActivity`: inicialización de datos de assets (`JNIDatInit`).
  - `AccelerometerListener`: control de inclinación para la dirección (`JNIAccelerometer`).
- Análisis de la librería nativa `libsnailmail.so` (655 KB):
  - 18 exports JNI globales identificados con `readelf` y desensamblador ARM.
  - 80 símbolos externos requeridos (todos cubiertos por `source/dynlib.c` y runtime de VitaSDK/vitaGL).
  - Descubierta la capa interna de abstracción de plataforma `Pfm` (`PfmAudio*`, `PfmSaveFile`, `PfmLoadFile`, `PfmFindFile`, `PfmDeleteFile`, `PfmSwapBuffers`, `PfmFinish`, `PfmVibrate`).
  - Descubierto el empaquetado del archivo de recursos `asm.mp3` (archivo DAT de 7.8 MB con 734 recursos indexados).

## Fase 3: Bootstrap del Loader e Integración FalsoJNI (Completada — 2026-10-07)
- Configuración de CMake ajustada para compatibilidad con VitaSDK y CMake 4.x (`CMAKE_POLICY_VERSION_MINIMUM 3.5`).
- Resuelta colisión de símbolos EGL desactivando `reimpl/egl.c` (vitaGL implementa EGL nativamente).
- Agregado conversor UTF-8/UTF-16 en `lib/falso_jni/converter.c`.
- Soporte dual de carga dinámica en `source/utils/init.c` para `libsnailmail.so` y `main.so`.
- Integración de FalsoJNI en `source/java.c` con los 26 métodos requeridos por `ADRenderer` registrados e implementados.
- Implementación de `so_patch()` en `source/patch.c`:
  - `JNIDatInit`: gancho nativo que abre y parsea `ux0:data/snailmail/assets/asm.mp3` directamente mediante `fopen` e inicializa la tabla hash `gDatHash` sin depender de clases JNI ni de `dup()`.
  - `JAVAC_UnPng` y `JAVAC_UnJpg`: decodificación de texturas e imágenes en memoria ultrarrápida usando `stb_image.h` (`stbi_load_from_memory`), evitando overhead de paso de buffers por JNI.
  - `JAVAC_UnZip`: descompresión en memoria usando `zlib` (`inflateInit2` / raw deflate).
  - Funciones `Pfm*`: puente directo para almacenamiento de partidas en `ux0:data/snailmail/saves/` y puente hacia el subsistema de audio.
  - `wprintf`: redirección de trazas y logs del motor C++ hacia el logger de PS Vita.

## Fase 4: Gráficos, Input y Audio (Completada — 2026-10-07)
- **Gráficos**:
  - Pipeline de función fija OpenGL ES 1.1 renderizado con `vitaGL`.
  - Configurado a resolución nativa completa de PS Vita: 960x544.
  - Bucle de renderizado con intercambio de buffers mediante `gl_swap()`.
- **Audio**:
  - Subsistema multicanal implementado en `source/audio.h` y `source/audio.c`.
  - Decodificación Ogg Vorbis integrada mediante Tremor (`libvorbisidec`).
  - Hilo de mezcla de audio dedicado en CPU Core 1 (`SceAudioOut`), tasa 44100 Hz estéreo.
  - Soporte para BGM en bucle (`mainmenu.ogg`, `music1.ogg`, etc.) y 7 voces concurrentes para SFX (`tut*.ogg`, `laser*.ogg`, etc.) con control de volumen independiente.
- **Input**:
  - Pantalla táctil frontal analizada mediante `sceTouchPeek` y convertida a coordenadas normalizadas para `JNIMouseEvent` (ACTION_DOWN, ACTION_MOVE, ACTION_UP).
  - Stick analógico izquierdo y cruceta digital (D-Pad) mapeados al vector 3D de acelerómetro (`JNIAccelerometer`) para control analógico del caracol.
  - Botón Cruz (X) y gatillo R1 mapeados a salto/disparo (evento sintético de Space bar vía `JNIKey` y click táctil).
  - Botón START mapeado a pausar juego (`nativePause`).

## Fase 5: Empaquetado y Verificación (Completada — 2026-10-07)
- Sistema de logs adaptado al estándar canónico: `ux0:data/snailmail/logs/snailmail_NNN.log` con rotación 001..999 y `next.idx`, validado exitosamente con `psvita-toolkit log-standard`.
- LiveArea configurado con imágenes PNG colormap de 8 bits validadas (`icon0.png`, `pic0.png`, `startup.png`, `bg0.png`, `template.xml`).
- Compilación final ejecutada exitosamente:
  - Generado `build/eboot.bin` (685 KB).
  - Generado `build/snailmail.vpk` (767 KB).
- Carpeta de staging preparada en `ux0_data/snailmail/`:
  - `libsnailmail.so`
  - `assets/asm.mp3` y 118 pistas/efectos de sonido `.ogg`
  - `logs/`
  - `saves/`

## Fase 6: Triaje y Corrección de Crash en Hardware Real (Completada — 2026-10-07)
- **Diagnóstico del Crash Inicial (`snailmail_001.log` y core dump `snailmail-psp2core-1791378221-0x0000a132cf-eboot.bin.psp2dmp`)**:
  - Excepción reportada: `0x30003` (Prefetch abort exception, CPU intentando ejecutar instrucción en dirección inválida `PC = 0x200`).
  - `LR = 0x8101b861`: dirección de retorno apuntando a `DeleteLocalRef` (`FalsoJNI.c:216`).
  - Pila del dump: en el stack frame se observaron llamadas a `_Z14RShellLoadFilePcPvPi` procesando `Sprites/sandlotloading.jpg` (recurso 38 del DAT, 512x512 JPG).
- **Causa Raíz Descubierta**:
  1. **Colisión destructiva en `hook_arm` de 8 bytes sobre funciones de 4 bytes**:
     - En `libsnailmail.so`, `_Z18PfmAudioLoadSamplePc` está ubicado en `0x149e4` y mide solamente 4 bytes (una sola instrucción `b _Z14JAVACLoadSamplePc`).
     - Inmediatamente adyacente a `0x149e4 + 4 = 0x149e8` se encuentra `_Z11JAVAC_UnJpgPviS_iii`.
     - `hook_arm` escribe un salto indirecto de 8 bytes (`ldr pc, [pc, #-4]` + dirección de 32 bits).
     - Al enganchar `PfmAudioLoadSample` a `0x149e4`, los 4 bytes de dirección pisaron los primeros 4 bytes de `JAVAC_UnJpg` (`0x149e8`), destruyendo su hook de decompresión JPG.
  2. **Ejecución corrupta de `JAVAC_UnJpg` nativo y llamada JNI**:
     - Debido a la sobreescritura, la llamada de decodificación cayó en la rutina nativa del `.so` que hace llamadas JNI a Android (`CallVoidMethod`, `GetByteArrayRegion`, `DeleteLocalRef`).
     - Al retornar de `DeleteLocalRef`, el valor de altura/anchura de la imagen (`w = 512 = 0x200`) que estaba en la pila fue popeado a `PC`, provocando el salto a `0x200`.
  3. **Logging incondicional**:
     - En `source/utils/logger.h`, los macros de log dependían de `#ifdef DEBUG_SOLOADER`, dejando `snailmail_001.log` vacío en builds normales.
- **Solución Implementada**:
  - **Hooking de alto nivel en `PfmLoadFileDat`**: Implementado `hooked_PfmLoadFileDat` en [patch.c](file:///Volumes/TOSHIBA%20EXT/PSVITA%20Develop/Snail-Mail-vita/source/patch.c) que intercepta directamente `_Z14PfmLoadFileDatPviiiitt` (`0x14c74`). Carga, descomprime (Zip raw inflate vía `zlib`, PNG/JPG vía `stb_image.h`) y construye el header TGA de 18 bytes directamente en C sin tocar ninguna llamada JNI interna.
  - **Inversión vertical de filas para compatibilidad TGA**: `stbi_load_from_memory` entrega píxeles en orientación top-down; se implementó el copiado bottom-up de filas para respetar la convención de origen inferior izquierdo de los TGAs del motor Sandlot.
  - **Resolución de hooks en funciones trampolín de 4 bytes**:
    - `_Z18PfmAudioLoadSamplePc` (4 bytes) ahora se engancha en su función destino real `_Z14JAVACLoadSamplePc` (136 bytes).
    - `_Z11PfmFindFilePc`, `_Z11PfmSaveFilePcPvi` y `_Z13PfmDeleteFilePc` (4 bytes cada una) ahora se enganchan en las funciones subyacentes `JAVACFindFile` (140 bytes), `JAVACSaveFile` (256 bytes) y `JAVACDeleteFile` (128 bytes), eliminando cualquier sobreescritura en memoria.
  - **Logging activo en Release y Debug**: Macros `l_info()`, `l_warn()`, `l_debug()` ahora generan salida directa siempre en `ux0:data/snailmail/logs/snailmail_NNN.log`.
  - Recompilados exitosamente `snailmail` (con símbolos), `eboot.bin` y `snailmail.vpk`.

## Fase 7: Vendorización de vitaGL y Solución de Pantalla Negra (Completada — 2026-10-07)
- **Diagnóstico del Run 002 (`snailmail_002.log`)**:
  - Síntoma: El juego arrancó perfectamente, sin crashes, audio funcionando y procesando lógica, pero con **pantalla negra permanente**.
  - El log confirmó la secuencia completa:
    ```
    [    4208] info    Initializing Audio...
    [    4210] success [Audio] Audio mixer initialized.
    [    4216] info    Calling JNIDatInit...
    [    4262] success DAT archive initialized successfully!
    [    4270] info    Calling nativeInit...
    [    4277] info    [Engine] Register Functions +
    [    4286] info    [Engine] Register Functions -
    [    4293] info    Calling nativeResize (960, 544)...
    [    4301] info    [Engine] !!! ANDROID SCREEN 960 x 544
    ```
- **Causa Raíz Descubierta**:
  - Idéntica a la documentada en **Zenonia 3/4 (Fase 124)**, **Prince of Persia Classic** y **Raging Thunder 2**:
  - La librería binaria `libvitaGL.a` precompilada del SDK (`vitasdk`) fue construida sin `HAVE_SOFTFP_ABI`, mientras que el loader de Snail Mail compila obligatoriamente con `-mfloat-abi=softfp`.
  - Sin `HAVE_SOFTFP_ABI`, vitaGL toma caminos de código y convenciones ABI de coma flotante incompatibles: todas las llamadas de dibujo y subida de texturas se emiten internamente pero la presentación al framebuffer de GXM se corrompe y nunca llega a pantalla -> **pantalla negra con audio perfecto**.
- **Solución Implementada**:
  - **Vendorización de vitaGL**: Importado el árbol completo de `vitaGL` desde `Prince of Persia` a `vendor/vitaGL/` (upstream master con compatibilidad probada con el toolchain actual y parches para evitar falsos positivos de `system_app_mode`).
  - **Compilación con Flags de Compatibilidad**:
    - Flags: `SOFTFP_ABI=1 NO_SPLASHSCREEN=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1`.
    - `SOFTFP_ABI=1` habilita `-DHAVE_SOFTFP_ABI` y alinea vitaGL al 100% con el ABI softfp del loader.
    - `NO_SPLASHSCREEN=1` elimina el splash de vitaGL que puede interferir con la sincronización inicial de buffers.
  - **Configuración en CMakeLists.txt**:
    - Se incorporó `vendor/vitaGL/source` como include prioritario (`include_directories(BEFORE ...)`).
    - Se creó el target `vitaGL_lib` mediante `build_vitagl.sh` y `make -C vendor/vitaGL` con reevaluación por sello de flags (`vitagl.stamp`).
    - Se enlazó explícitamente contra `${VITAGL_VENDOR_DIR}/libvitaGL.a`.
  - **Compilación y Empaquetado**:
    - Recompilado `snailmail` (ELF 9.4 MB con símbolos).
    - Generado `build/eboot.bin` (729 KB).
    - Generado `build/snailmail.vpk` (1.12 MB).

## Fase 8: Pruebas en Consola Real y Verificación (Completada — 2026-10-07)
- [x] Transferir `ux0_data/snailmail/` a `ux0:data/snailmail/` en la consola física.
- [x] Instalar el nuevo `build/snailmail.vpk` (o sustituir `eboot.bin` en `ux0:app/PSVSM0001/`).
- [x] Iniciar el juego y verificar que la pantalla de carga Sandlot (`Sprites/sandlotloading.jpg`), menús, audio y juego funcionen.
- [x] Verificado por el usuario en hardware real: **¡El juego ya funciona!**

## Fase 9: Release Inicial y Documentación (Completada — 2026-10-07)
- Creado `README.md` estandarizado con banner LiveArea, requisitos, árbol de datos y tabla de controles.
- Creado documento de release bilingüe `Docs/RELEASE_v01.00.md`.
- Documentado issue conocido prioritario:
  - El juego solo funciona con los controles en **Tilt Mode** (emulado vía Stick Analógico / D-Pad). Se está trabajando para el **Touch Mode**.
  - No se puede navegar por el menú con los controles físicos (requiere pantalla táctil frontal).
- Preparado primer commit del repositorio.

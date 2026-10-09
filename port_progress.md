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
- Documentados issues conocidos iniciales.
- Preparado primer commit del repositorio.

## Fase 10: Soporte Integral de Controles y Navegación de Menús (Completada — 2026-10-08)
- **Corrección Issue 1: Soporte universal Tilt Mode y Touch Mode**:
  - **Causa Raíz**: En `libsnailmail.so`, la función `cAccelerometer::AI()` verificaba `options->control_mode`. Si era distinto de 0 (Touch Mode), saltaba a `0x4b618`, anulando el cálculo del acelerómetro y forzando la dirección horizontal a `0.0f`. Además, la lógica táctil nativa en Touch Mode estaba incompleta.
  - **Solución**: Se parcharon con `NOP` los dos saltos condicionales en `cAccelerometer::AI()` (+0x1f8 y +0x28c). Ahora el stick analógico y el D-Pad controlan a Turbo en ambos modos sin interrupción. Adicionalmente, se implementó control táctil directo en gameplay (tocar mitad izquierda/derecha de la pantalla gira a Turbo).
- **Corrección Issue 2: Navegación completa de menús con botones físicos**:
  - **Causa Raíz**: La interfaz de Snail Mail fue adaptada para Android asumiendo pulsaciones táctiles directas. `JNIMouseEvent` ignoraba el movimiento en menús y no existía un cursor virtual ni salto entre botones.
  - **Solución**:
    - Se creó el módulo `source/menu_ctrl.c` y `source/menu_ctrl.h`.
    - **D-Pad Smart Snapping**: Inspecciona `cRBorderManager` en tiempo real y salta inteligentemente al botón interactivo más cercano en la dirección pulsada (Arriba, Abajo, Izquierda, Derecha con wrap-around). Al apuntar, se actualiza `RShellSetMouse` para que el botón se ilumine visualmente.
    - **Cursor Virtual Analógico**: El stick analógico izquierdo mueve un puntero suave por la pantalla cuando no se está en partida activa.
    - **Botón Cruz (X)**: Pulsa y activa el botón enfocado (o toca la pantalla en pantallas de transición/continuar).
    - **Botón Círculo (O) / Triángulo**: Retrocede o cancela automáticamente buscando botones "Back", "OK", "Cancel", "Resume", etc.
    - **Renderizado de Cursor**: Se dibuja un cursor estilo sci-fi con `vitaGL` en menús para retroalimentación visual clara.
    - **Parche JNIMouseEvent**: Se modificó `JNIMouseEvent + 0x84` para propagar el movimiento del cursor en menús.
- **Compilación verificada**: Recompilados `snailmail`, `eboot.bin` y `snailmail.vpk` de forma limpia.
## Fase 11: Corrección de Filtro Verde y Crash al Completar Vuelta/Nivel (Completada — 2026-10-08)
- **Corrección Bug Filtro Verde/Cian en Pantalla**:
  - **Diagnóstico**: `menu_ctrl_draw()` dibujaba el cursor de menú con `glColor4f(0.2f, 0.9f, 1.0f, 1.0f)`. En el pipeline FFP de vitaGL esto altera el uniforme de color de vértice de la GPU. `libsnailmail.so` almacena en caché el último color en `GLColour` (`0x0037caf0`); como la llamada al cursor no actualizaba `GLColour`, la función `G0SetColour()` omitía llamar a `glColor4f` para poner blanco `(1,1,1,1)`, dejando todo el juego teñido de cian/verde.
  - **Solución**: Aislamiento estricto de estado en `menu_ctrl_draw()` con `glPushAttrib(GL_ALL_ATTRIB_BITS)` y `glPopAttrib()`, guardado/restauración de color con `glGetFloatv(GL_CURRENT_COLOR)` / `glColor4fv`, y forzado de invalidación de caché de color (`*s_pGLColour = 0` y llamada a `G0ResetColour()`).
- **Triage y Corrección Crash al Completar Vuelta (`snailmail-psp2core-1791478787-0x0004be2b63-eboot.bin.psp2dmp`)**:
  - **Diagnóstico con `so-crash-triage` + `vita-parse-core`**:
    - **Excepción**: Data abort exception en `PC: 0x98020d10`, `LR: 0x81001625` (`menu_ctrl_update`).
    - **Función Nativa**: `_Z8MouseSetiii` (offset `+0x64`).
    - **Instrucción causante**: `vstr s14, [r3, #88]`, donde `r3` fue cargado desde `*(ip + 0x224)` (`Game + 0x224`).
    - **Causa Raíz**: Al terminar una vuelta / completar un nivel (`cRSubGame::Complete()`), el motor destruye o desvincula los sub-objetos de carrera y pantallas asociadas, poniendo el puntero en `Game + 0x224` en `NULL`. Cuando el bucle de actualización del loader llamaba a `RShellSetMouse` / `MouseSet`, o cuando la UI de `cRBorder::MouseTest` consultaba las coordenadas del ratón, el motor desreferenciaba el puntero `NULL` (`0x0 + 0x58`) provocando el crash inmediato.
  - **Solución Implementada**:
    - **Protección en `menu_ctrl.c`**:
      - Se agregó validación con `gGameValid` (`*(uint8_t *)gGameValid`).
      - Se implementó `update_engine_mouse_pos()`, que verifica que `gGame` y `*(void **)(gGame + 0x224)` existan antes de llamar a `RShellSetMouse`.
    - **Blindaje Binario en `source/patch.c`**:
      - **Parche en `MouseSet` (`_Z8MouseSetiii + 0x64`)**: Se reemplazó el acceso directo con `cmp r3, #0` (`0xe3530000`) y `beq 0x20d18` (`0x0affffff`), saltando de forma segura si el puntero de sub-objeto es nulo.
      - **Parche en `cRBorder::MouseTest` (`_ZN8cRBorder9MouseTestEv + 0x40`)**: Se agregaron guardas para verificar si el puntero cargado desde `0x224` es nulo, retornando `0` de forma segura en lugar de intentar leer el struct desreferenciado.
- **Compilación verificada**: Recompilados `snailmail`, `eboot.bin` y `snailmail.vpk` con todas las verificaciones exitosas.

## Fase 12: Disparo con CROSS, Tilt Centrado y Menú de Controles START+SELECT (Completada — 2026-10-08)
- **CROSS para disparar (robusto, doble vía)**:
  - En gameplay, mientras se mantiene `btn_shoot` (CROSS/R1 por defecto) se llama a `RShellInputRegisterMouseClickOnly()` cada frame (autofuego) y en el flanco de pulsación se invoca además `JNIKey(62)` (Android `KEYCODE_SPACE`), que el motor convierte en `KeySet(0x39)` — cubre tanto la vía de ratón como la de teclado del motor Sandlot.
- **Modo Tilt con pantalla centrada y estable**:
  - Causa: el juego nunca recibía datos del sensor, así que en modo Tilt la cámara/dirección derivaba.
  - Solución: se resuelve `Java_com_sandlotgames_snailmail_AccelerometerListener_JNIAccelerometer` y se alimenta cada frame (juego y menús) con el vector fijo `(0, 0, 1)` — el `cAccelerometer::Input` lo normaliza y `cAccelerometer::AI()` lo interpreta como dispositivo quieto en plano. El stick/D-Pad siguen gobernando vía `RShellSetMouse` en ambos modos.
- **Menú de controles START+SELECT (estilo Carnivores)**:
  - Nuevo módulo `source/controls_menu.h/.c`: overlay vitaGL con fuente bitmap 8x8 embebida, aislamiento estricto de estado GL (`glPushAttrib` + matrices + `glColor4f(1,1,1,1)` final, según lección anti-tinte de Fase 11).
  - Apertura/cierre por flanco de START+SELECT; mientras está abierto el juego se congela (`nativeRender(pause=1)`), consume todo el input y START solo no pausa.
  - Items: SHOOT, STEER LEFT, STEER RIGHT, PAUSE (CROSS para capturar botón, SELECT cancela), DEADZONE (±5, 5–50), SENSITIVITY (±10, 20–300), RESET DEFAULTS, CLOSE. Navegación UP/DOWN, ajuste LEFT/RIGHT, CROSS editar, CIRCLE cerrar. Guardado automático en `controls.txt`.
  - `source/controls.h/.c`: nuevos `controls_reset_defaults()`, `controls_mask_to_string()`, `controls_button_from_mask()`.
  - `CMakeLists.txt`: compila `source/controls_menu.c`.
- **Compilación verificada**: `snailmail`, `build/eboot.bin` (735 KB) y `build/snailmail.vpk` (1.1 MB) generados sin errores.

## Fase 12b: Triage Crash del Menú START+SELECT (`snailmail_010.log` + `snailmail-psp2core-1791489200-0x0000a52aad`) (Completada — 2026-10-08)
- **Diagnóstico con `so-crash-triage` + `vita-parse-core`**:
  - **Excepción**: Data abort en `PC = 0x8105350e` (`glVertex2f` de vitaGL, `str r0,[r3,#0]` con `r3 = 0`).
  - **LR**: `controls_menu_draw+0xde` (`draw_quad` del overlay) — el crash fue en el PRIMER quad del menú, por eso el overlay nunca llegó a verse (pantalla "congelada" = thread principal muerto antes del swap).
  - **Causa raíz**: `gl_init()` llamaba `vglInitExtended(0, ...)` → `legacy_pool_size = 0` → `scene_reset()` (`gxm.c:591`) nunca aloca `legacy_pool` → `legacy_pool_ptr = NULL` → cualquier `glBegin/glVertex` escribe en NULL. El juego nunca lo notó porque solo usa vertex arrays (`glVertexPointer`), no immediate-mode; el menú fue el primer usuario de `glBegin`.
- **Solución (como en otros ports: pool de immediate-mode > 0)**:
  - `source/utils/glutil.c`: `vglInitExtended(2*1024*1024, ...)` — 2 MB cubre el peor caso del overlay (~30k verts ≈ 840 KB).
  - `source/controls_menu.c`: dibujo reescrito a un ÚNICO `glBegin(GL_QUADS)/glEnd` por frame (1 draw call en vez de ~7k), con `glColor4f` por quad — verificado en fuente vitaGL que es seguro dentro del begin.
- **Compilación verificada**: `build/eboot.bin` y `build/snailmail.vpk` regenerados sin errores ni warnings.

## Fase 12c: Triage GPU-Hang al Continuar Tutorial + CROSS/R sin Respuesta (En curso — 2026-10-08)
- **Evidencia (`snailmail_011.log` + `...-GPUCRASH.psp2dmp` vía `vita-parse-core`)**:
  - Ningún thread con excepción CPU (todos `Waiting`/`Running`) → cuelgue de GPU, no data abort. El log (con buffer) se corta en `nativeResize`, sin cola útil.
  - Desensamblado `cRContinue::AI` (`0x51c0c`, ARM): las pantallas Continue/tutorial SOLO avanzan con el flag `0x20` de un `cRBorder` (botón tocado). No hay fallback de teclado. El tap fijo al centro `(480,272)` falla si el botón no está ahí → CROSS/R1 "no hacen nada".
  - Pool circular vitaGL de 32 MB descarta que el `legacy_pool` de 2 MB sature GPU (con overrun cae a alloc normal).
- **Cambios (todos verificados en desensamblado/fuente, sin adivinar)**:
  - `source/main.c`: cursor virtual en menús — D-pad (±10/12 px) y stick mueven `menu_mouse_*`, se propaga con `RShellSetMouse` (en espacio 640x480 como en gameplay) para que el engine ilumine el border apuntado; CROSS/R1 (`btn_shoot` completo) tapean DOWN/UP EN el cursor. Touch frontal sigue funcionando y mueve el cursor.
  - `source/main.c`: guard `engine_mouse_ok` (`Game` y `Game+0x224` no NULL) en TODAS las llamadas a `RShellSetMouse` (gameplay y menús) — equivale del lado loader al blindaje binario de Fase 11, ausente en el código actual.
  - `source/main.c`: `log_set_buffered(0)` + log de cada cambio de `game_state` (`Game+0x718fc`) para que el próximo log muestre los estados del tutorial aunque cuelgue.
- **Compilación verificada**: `build/eboot.bin` y `build/snailmail.vpk` regenerados sin errores.
- **Pendiente de hardware**: confirmar VPK reinstalado, reproducir, y enviar `snailmail_012.log` (traerá los `game_state`) + dump si vuelve a colgar.

## Fase 12d: Data Abort en Splash por RShellSetMouse del Cursor (`snailmail_012.log` + `...-0x0000e32481-...psp2dmp`) (Completada — 2026-10-08)
- **Diagnóstico con `so-crash-triage` + `vita-parse-core`**:
  - **Excepción**: Data abort en `PC = 0x98020d10` = `MouseSet+0x64` (`vstr [r3,#88]`, `r3 = *(base+0x224) = NULL`) — la misma instrucción de Fase 11.
  - **LR**: `main+0x52e` = la llamada a `RShellSetMouse` del cursor virtual (rama menús, código nuevo de Fase 12c).
  - **Log** (ya sin buffer): muere en `game_state 0` (splash, ~200 ms tras entrar al loop). En splash los sub-objetos del ratón no existen y no hay borders que iluminar.
  - **Por qué falló el guard**: `engine_mouse_ok` lee `Game+0x224`, pero `MouseSet` resuelve su base por su propia GOT (objeto manager del ratón, distinto de `Game`); en splash uno puede ser no-NULL y el otro NULL. Lección: el guard del loader no cubre ese puntero — la condición fiable es el estado del engine.
- **Solución**: en la rama menús, `RShellSetMouse` solo si `engine_mouse_ok && game_state >= 1`. Los taps (`JNIMouseEvent`, vía probada de Fase 8) siguen sin gate.
- **Compilación verificada**: `build/eboot.bin` y `build/snailmail.vpk` regenerados sin errores.
- **Pendiente de hardware**: reinstalar VPK, confirmar que pasa el splash y enviar `snailmail_013.log` (mostrará los estados de menús/tutorial).

## Fase 12e: Menú START+SELECT Espejado + CROSS/START sin Efecto en Gameplay (En curso — 2026-10-08)
- **Evidencia (hardware, `screenshots/jf/2026-10-08/2026-10-08-162958.jpg`)**:
  - El overlay de controles se lee espejado horizontal (ej. `CONTROLS` como `ƆON⊥ЯO⅃Ƨ`, `< 15% >` como `> 15% <`): orden de caracteres conservado, cada glifo espejado.
  - En gameplay, START no pausa y CROSS no dispara.
- **Causa raíz**:
  - `emit_text()` en `source/controls_menu.c` testeaba `(1 << (7-col))`, pero en `font8x8_basic` el bit menos significativo es el píxel izquierdo (ver glifo `L`: `0x0F` debe dar trazo a la izquierda) → cada glifo sale espejado.
  - Disparo: `JNIMouseEvent` normaliza a `x*640/W, y*480/H` y el fuego solo existe como botón táctil del centro-abajo; `RShellInputRegisterMouseClickOnly()` + `JNIKey(62)` nunca tocan esa zona. `JNIKey` además solo llama a `KeySet` si `Game+0xBF0 == 2`.
  - Pausa: `nativePause()` solo conmuta un flag de delta de tiempo interno, no abre el submenú de pausa (`cRSubPause`); en Android ese export ni se llama desde Java. La pausa real es el botón táctil `MENU` de arriba-izquierda.
- **Cambios**:
  - `source/controls_menu.c`: test de bit a `(1 << col)`.
  - `source/main.c` (gameplay): CROSS (`btn_shoot`) manda `JNIMouseEvent` DOWN/UP en `(480, 492)` (centro-abajo, espacio 960x544), conserva `ClickOnly` (autofuego) + `JNIKey(62)`; START (`btn_pause`) tapea `MENU` en `(80, 32)`; se elimina la llamada a `nativePause`. El dedo real tiene prioridad sobre los toques sintéticos y el combo START+SELECT los libera.
- **Fix del sistema de build (bloqueaba `psvita-toolkit build`)**:
  - Causa: el toolkit compila en una copia temporal que respeta `.gitignore`; el patrón desnudo `Makefile` excluía `vendor/vitaGL/Makefile` de la copia → `make clean` / `make -C` fallaban con errores crípticos.
  - Solución: en `.gitignore`, las 4 reglas de restos de CMake in-source (`Makefile`, `CMakeCache.txt`, `CMakeFiles/`, `cmake_install.cmake`) ahora van ancladas a raíz (`/...`).
  - Endurecido: `vendor/vitaGL/build_vitagl.sh` valida dir/Makefile y usa el make de CMake; `CMakeLists.txt` (raíz y `lib/libc_bridge`) usa `${CMAKE_MAKE_PROGRAM}` + `VERBATIM` en vez de `make` pelado.
- **Compilación verificada**: `psvita-toolkit build --preset release` → Build OK (`build/eboot.bin` 736 KB, `build/snailmail.vpk` 1.1 MB, 2026-10-08).
- **Pendiente de hardware**: reinstalar VPK, confirmar texto legible, disparo con CROSS y pausa con START.

## Fase 12f: GPU-Hang Casi al Final de la Carrera (`snailmail_015.log` + `...-GPUCRASH.psp2dmp`) (En curso — 2026-10-08)
- **Evidencia**:
  - Log: carrera tutorial (Track Length 1340) en `game_state 2` durante ~77 s; último mensaje `@Message Lasers or Rockets...`, sin errores del loader antes del corte. Controles del usuario funcionan (CROSS dispara, START pausa, menú START+SELECT abre/cierra y guarda).
  - Dump (`vita-parse-core` vs `build/snailmail`): GPU hang clásico — 0 threads con excepción CPU (todos `Waiting`, solo `snailmail_audio_thread` en `Running`); el hilo principal espera en kernel (típico `vglSwapBuffers`/display stall tras fault de GPU).
- **Descartado con desensamblado ARM real del `.so`** (verificado instrucción por instrucción):
  - `cRSubGame::Complete()` (`0x6a8ac`), `ScoreStatsDisplay` (`0x5ed58`), `AddArcadePro` (`0x55d00`), detección de fin en `cRSubGoldy::AI` (`0x6ac54`), `cRSubGame::AI` (`0x72214`) y `G0Render` (`0x7ad84`): **cero llamadas `gl*`** — la transición de fin de carrera es solo CPU/objetos/guardado/fade. El hang no viene de esta ruta.
  - `RShellScreenGrab()` (`0x1a300`) es `bx lr` (stub vacío) — descartado como causa.
- **Instrumentación enviada a hardware (este VPK)**:
  - `source/dynlib.c`: `glTexImage2D`/`glDrawArrays`/`glDrawElements` ahora pasan por validadores que registran anomalías (`w/h` absurdos, `border != 0`, `count` absurdo; tope 32 líneas) y reenvían a vitaGL sin cambiar comportamiento.
  - `source/main.c`: cada 300 frames de gameplay se registra `race: frames=N vram_free=X ram_free=Y` (el log no tiene buffer, sobrevive al cuelgue).
- **Compilación verificada**: `psvita-toolkit build --preset release` → Build OK (`build/eboot.bin`, `build/snailmail.vpk` 2026-10-08).
- **Pendiente de hardware**: reinstalar VPK, correr la carrera hasta el cuelgue y enviar `snailmail_016.log` (+ decir si el punto del corte es siempre el mismo: línea de meta, pantalla de resultados, o aleatorio; y si ocurre sin mantener CROSS).

## Fase 12f: GPU-Hang en Gameplay (`snailmail_015.log` + `...-1791493343-GPUCRASH.psp2dmp`) (Completada en build — 2026-10-08)
- **Evidencia**: ningún thread con excepción; `PSVSM0001` bloqueado en espera de kernel dentro de `vglSwapBuffers` (igual que el GPUCRASH `1791489392`) → la GPU dejó de responder. El log muestra el menú START+SELECT abierto a los 49 s y el cuelgue a los 101 s, ya en gameplay.
- **Correlación**: los dos GPUCRASH aparecieron solo después de la Fase 12b (pool immediate-mode 0 → 2 MB). Antes, el juego llegaba a completar vueltas sin cuelgues de GPU.
- **Causa raíz (verificada en la fuente vendorizada de vitaGL + conteo de glifos)**:
  - En vitaGL cada vértice immediate-mode sin textura ocupa `LEGACY_NT_VERTEX_STRIDE` = 22 floats = **88 B** (la Fase 12b asumió ~28 B).
  - El overlay emitía un quad por píxel de glifo: ~6.200 quads ≈ 24.800 vértices ≈ **2,18 MB** > pool de 2 MB. Con `NO_DEBUG`, `glEnd` no corta ni loguea al desbordar (`ffp.c:2606`): sigue escribiendo pasado `legacy_pool_end`, sobre memoria visible por la GPU → corrupción → hang en un frame posterior.
  - Además, `scene_reset()` (`gxm.c:591`) reserva el pool del pool circular en **cada** frame aunque el juego nunca use `glBegin` (2 MB/frame menos para los vértices del juego).
- **Solución**:
  - `source/controls_menu.c`: un quad por tramo horizontal de píxeles → ~2.400 quads ≈ 860 KB en el peor caso (2,4x de margen).
  - `source/controls_menu.c`: `legacy_pool_size` (global de vitaGL) = 2 MB al abrir el menú y 0 al cerrarlo; open/close corren antes de `nativeRender`, así que la escena del frame ya lo reserva fresco.
  - `source/utils/glutil.c`: `vglInitExtended(0, ...)` → gameplay vuelve a la config previa a la 12b, que era estable.
- **Compilación verificada**: `psvita-toolkit build --preset release` → Build OK.
- **Pendiente de hardware**: reinstalar el VPK, abrir/cerrar el menú varias veces y jugar el tutorial completo (láser/cohetes). Si vuelve a colgar, los validadores GL de `dynlib.c` (`glDrawArrays/glDrawElements/glTexImage2D_checked`, ya incluidos en este build) loguean cualquier llamada anómala.

## Fase 12g: GPU-Hang Persistente sin Menú (`snailmail_016.log` + `...-1791494454-GPUCRASH.psp2dmp`) (Completada en build — 2026-10-08)
- **Evidencia**: el menú START+SELECT no se abrió en este run → el fix de la 12f era correcto pero no era el único disparador. Cuelgue a ~163 s (tutorial, "If I grab enough ~ I will become invincible!"), más lejos que antes. Memoria estable (`vram_free`/`ram_free` constantes en la telemetría `race:`), así que no hay fuga. ~20 `Cannot find Texture X/...` al cargar (Laser/Blaster/RocketLauncher/JetPack...).
- **Causa raíz (fuente vitaGL vendorizada)**: la ruta FFP de `glDrawElements`/`glDrawArrays` (`_glDrawElements_FixedFunctionIMPL`, `ffp.c`), que es la ÚNICA que usa el motor, hace `sceGxmSetFragmentTexture(&tex->gxm_tex)` **sin chequear `tex->status`**. `vglDrawObjects` sí lo chequea (`draw.c:593`). Una textura bindeada que nunca se subió (`TEX_UNINITIALIZED`, `gxm_tex` en cero → dirección 0) o que se borró (`glDeleteTextures` → `TEX_UNUSED` + memoria liberada en diferido) hace que la GPU lea memoria nula/liberada → GPU-hang. En Android el driver muestrea una textura incompleta como negro, sin colgarse.
- **Solución (vitaGL vendorizada)**:
  - `ffp.c`: nuevo `ffp_textures_valid()` (misma selección de unidades que `reload_ffp_shaders`); devuelve `GL_FALSE` si una unidad FFP habilitada tiene textura no `TEX_VALID`, y cuenta los casos en `vgl_skipped_tex_draws`.
  - `draw.c`: las 6 ramas FFP (`glDrawArrays`, `glMultiDrawArrays`, `glDrawElements*`, `glDrawRangeElements*`) saltean el draw si la textura no es válida — misma política que `vglDrawObjects`.
  - `shared.h`: declaración.
  - `source/main.c`: la telemetría `race:` loguea `skipped_tex_draws=N` cada 300 frames.
- **Compilación verificada**: Build OK; `ffp_textures_valid` y `vgl_skipped_tex_draws` presentes en `build/snailmail.elf`.
- **Pendiente de hardware**: jugar el tutorial completo. `skipped_tex_draws > 0` confirma que este era el disparador (esos draws en Android salían negros/vacíos).

## Fase 12h: GPU-Hang Determinista al Activarse la Invencibilidad (`snailmail_017.log` + `...-1791495585-GPUCRASH.psp2dmp`) (Build listo, pendiente de hardware — 2026-10-08)
- **Corrección de la 12f/12g**: según el usuario, el GPU-hang ya existía en el último commit (`e85fd22`). La correlación con el pool de la 12b era falsa. Los fixes 12f (desborde del pool del menú) y 12g (texturas inválidas en la ruta FFP) corrigen bugs latentes reales, pero no este cuelgue: `skipped_tex_draws=0` en el run 017.
- **Evidencia (determinista)**: en los runs 016 y 017 el cuelgue cae entre 6 y 10 s después de `@Message If I grab enough ~>I will become invincible!`. `LEVELS/TUTORIAL.TXT` (DAT): ese mensaje abre 4 segmentos `Tutorial 4` de 12 s llenos de `~` → el caracol junta suficientes y se activa `cRInvincible` (modelo `invincible-base-000.x` translúcido que rota con `Alpha` + `RotLocalY`, más `cRSnailSkin::Change`). Memoria estable, sin fugas.
- **Mecanismo sospechado**: vitaGL manda a la GPU, sin chequear rangos, los índices y vértices que vienen de VBO (`is_full_vbo` → no calcula `top_idx`), y las posiciones las pasa tal cual. Un índice fuera de rango o una posición no finita en un draw del motor → la GPU lee fuera del buffer y se cuelga. En Android el driver lo tolera.
- **Cambio (fix + evidencia), `source/dynlib.c`**:
  - Wrappers `_tracked` de `glBindBuffer`, `glBufferData`, `glBufferSubData`, `glVertexPointer`, `glTexCoordPointer` y `glEnable/DisableClientState`: replican en CPU el estado de buffers y arrays (tamaño de cada VBO y copia de los index buffers).
  - `glDrawElements_checked` (única primitiva de dibujo del motor) valida índices dentro del IBO, vértices dentro del VBO y posiciones float finitas (≤ 1e7) en arrays de cliente. Si falla, **descarta el draw** y loguea `GL bad draw (<motivo>) skipped: caller=so+0x...` con el estado completo (máx. 64 líneas).
  - `source/main.c`: la telemetría `race:` agrega `bad_draws=N`.
- **Compilación verificada**: Build OK (`build/snailmail.vpk` 17:46).
- **Próximo paso**: con el log 018, si aparece `GL bad draw`, el `caller=so+0x...` identifica la función exacta del motor → fix definitivo en el origen (parche o hook). Si el cuelgue persiste con `bad_draws=0`, se descartan los rangos y las posiciones, y queda el estado de render (blend/depth/scissor) de `cRInvincible`.
- **Resultado en hardware (`snailmail_018.log`)**: tutorial completo sin cuelgue (`game_state 2 -> 8` a los 145 s, pasando por los mensajes de invencibilidad y "Excellent. Time to hit the Post Office."). Pero `bad_draws=0` y `skipped_tex_draws=0`: el validador no descartó ningún draw y el ritmo se mantuvo igual (300 frames cada ~5 s, como en el 017). **No queda demostrado que el fix sea este cambio.** Pendiente: 2-3 corridas más del tutorial y confirmar que el caracol llegó a volverse invencible. Si el cuelgue reaparece con `bad_draws=0`, seguir con el estado de render de `cRInvincible` (blend/depth/scissor).

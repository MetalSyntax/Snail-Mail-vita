#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "audio.h"
#include "controls.h"
#include "controls_menu.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <math.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

int _newlib_heap_size_user = 128 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

int main() {
    l_info("Starting Snail Mail (PS Vita)...");
    soloader_init_all();

    l_info("Initializing OpenGL...");
    gl_init();

    l_info("Initializing Audio...");
    audio_init();

    controls_init();
    controls_menu_init();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    void (*JNIDatInit)(void *env, void *thiz, void *fd, int start, int length) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_SnailMailActivity_JNIDatInit");
    void (*nativeInit)(void *env, void *thiz) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADRenderer_nativeInit");
    void (*nativeResize)(void *env, void *thiz, int w, int h) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADRenderer_nativeResize");
    void (*nativeRender)(void *env, void *thiz, int pause) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADRenderer_nativeRender");
    void (*JNIMouseEvent)(void *env, void *thiz, int action, float x, float y) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADGLSurfaceView_JNIMouseEvent");
    void (*JNIKey)(void *env, void *thiz, int keycode) =
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADGLSurfaceView_JNIKey");
    void (*JNIAccelerometer)(void *env, void *thiz, float x, float y, float z) =
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_AccelerometerListener_JNIAccelerometer");
    // Nota: nativePause() existe en el .so pero solo congela el delta de
    // tiempo interno y no abre el menu de pausa; la pausa real se hace
    // tocando el boton "MENU" (ver toque sintetico en el loop de gameplay).
    void (*RShellSetMouse)(int mouse, float x, float y) = 
        (void *)so_symbol(&so_mod, "_Z14RShellSetMouseiff");
    void (*RShellInputRegisterMouseClickOnly)(void) = 
        (void *)so_symbol(&so_mod, "_Z33RShellInputRegisterMouseClickOnlyv");

    if (JNIDatInit) {
        l_info("Calling JNIDatInit...");
        JNIDatInit(&jni, (jobject)1, (jobject)1, 0, 0);
    }

    if (nativeInit) {
        l_info("Calling nativeInit...");
        nativeInit(&jni, (jobject)1);
    }

    if (nativeResize) {
        l_info("Calling nativeResize (960, 544)...");
        nativeResize(&jni, (jobject)1, 960, 544);
    }

    void **pGame = (void **)so_symbol(&so_mod, "Game");
    void *gConfig = (void *)so_symbol(&so_mod, "gConfig");

    if (gConfig) {
        // Default to Touch Mode (1) so physical controls immediately steer Turbo in gameplay
        *(int *)((char *)gConfig + 8) = 1;
    }

    log_set_buffered(0); // sin buffer: si hay crash/GPU-hang el log conserva la cola
    l_info("Entering main game loop.");

    int was_touching = 0;
    float last_touch_x = 480.0f;
    float last_touch_y = 272.0f;

    // Cursor virtual para menus: el motor solo avanza pantallas
    // Continue/tutorial tocando el border real (cRContinue::AI espera el
    // flag 0x20 de un cRBorder), asi que CROSS/R1 tapean DONDE esta el
    // cursor y el D-pad/stick lo mueven (el engine ilumina lo apuntado).
    float menu_mouse_x = 480.0f;
    float menu_mouse_y = 272.0f;

    int last_game_state = -1;

    // Diagnostico GPU-hang fin de carrera (Fase 12f): contador de frames de
    // gameplay + memoria GPU libre. El log no tiene buffer, asi que estas
    // lineas sobreviven al cuelgue y dicen el punto exacto del corte.
    int race_frames = 0;

    uint32_t last_buttons = 0;
    int button_touch_active = 0;
    // Toque sintetico de disparo (CROSS): el motor solo dispara con un toque
    // ("Touch anywhere to fire"); ClickOnly/SPACE solos no alcanzan. El toque
    // tambien fija el destino de steering (cRMouse::ClickiPhone), asi que se
    // da EN la posicion de steering actual para no arrastrar al caracol.
    int shoot_touch_active = 0;
    float shoot_touch_x = 480.0f, shoot_touch_y = 272.0f;
    // Tap sintetico de pausa (START): toca el boton "MENU" de arriba-izq.
    // nativePause() por si solo no abre el menu de pausa en el engine.
    int pause_touch_active = 0;
    // Posiciones tactiles en espacio de pantalla 960x544 (como el touch real).
    const float MENU_TOUCH_X = 80.0f;   // boton "MENU" arriba-izquierda
    const float MENU_TOUCH_Y = 32.0f;

    while (1) {
        // Detect whether the player is currently in gameplay (game_state == 2).
        // Se loguea cada cambio para el triage (el tutorial/continue pasa
        // por varios estados y hay que verlos en el log aunque cuelgue).
        int game_state = -1;
        int in_gameplay = 0;
        if (pGame && *pGame) {
            game_state = *(int *)((char *)(*pGame) + 0x718fc);
            if (game_state == 2) {
                in_gameplay = 1;
            }
        }
        if (game_state != last_game_state) {
            l_info("game_state: %d -> %d %s", last_game_state, game_state,
                   in_gameplay ? "(gameplay)" : "(menu)");
            last_game_state = game_state;
        }

        // RShellSetMouse desreferencia Game+0x224 (Fase 11: NULL al cambiar
        // de nivel/pantalla -> data abort). Solo llamarlo con sub-objeto vivo.
        int engine_mouse_ok = 0;
        if (pGame && *pGame) {
            void *sub = *(void **)((char *)(*pGame) + 0x224);
            if (sub) engine_mouse_ok = 1;
        }

        // 1. Process touch
        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        // 2. Process controller
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        uint32_t just_pressed = pad.buttons & ~last_buttons;
        uint32_t just_released = ~pad.buttons & last_buttons;

        // START + SELECT abre/cierra el menu de controles (estilo Carnivores).
        // Se detecta por flanco: ambos pulsados ahora, pero no en el frame anterior.
        int both_now = ((pad.buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) ==
                        (SCE_CTRL_START | SCE_CTRL_SELECT));
        int both_before = ((last_buttons & (SCE_CTRL_START | SCE_CTRL_SELECT)) ==
                           (SCE_CTRL_START | SCE_CTRL_SELECT));
        if (both_now && !both_before) {
            if (controls_menu_is_open()) {
                controls_menu_close();
            } else {
                // Soltar toques a medias para que el juego no se quede colgado
                if (was_touching) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, last_touch_x, last_touch_y);
                    was_touching = 0;
                }
                if (button_touch_active) {
                    button_touch_active = 0;
                }
                if (shoot_touch_active) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, shoot_touch_x, shoot_touch_y);
                    shoot_touch_active = 0;
                }
                if (pause_touch_active) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, MENU_TOUCH_X, MENU_TOUCH_Y);
                    pause_touch_active = 0;
                }
                controls_menu_open();
            }
            last_buttons = pad.buttons;
            // Frame pausado con el menu encima
            if (JNIAccelerometer) JNIAccelerometer(&jni, (jobject)1, 0.0f, 0.0f, 1.0f);
            if (nativeRender) {
                nativeRender(&jni, (jobject)1, 1);
            }
            controls_menu_draw();
            gl_swap();
            continue;
        }

        // Menu abierto: consume todo el input y congela la logica del juego
        if (controls_menu_is_open()) {
            controls_menu_update(pad.buttons, just_pressed);
            last_buttons = pad.buttons;
            if (JNIAccelerometer) JNIAccelerometer(&jni, (jobject)1, 0.0f, 0.0f, 1.0f);
            if (nativeRender) {
                nativeRender(&jni, (jobject)1, 1);
            }
            controls_menu_draw();
            gl_swap();
            continue;
        }

        // Acelerometro estable y centrado: en modo Tilt la pantalla se quedaba
        // inclinada/derivando porque nunca se alimentaba el sensor. Con un
        // vector fijo (0,0,1) el juego lo ve como dispositivo quieto en plano.
        if (JNIAccelerometer) JNIAccelerometer(&jni, (jobject)1, 0.0f, 0.0f, 1.0f);

        if (!in_gameplay) {
            // ================= MENUS / PAUSE / CONTINUE / TUTORIAL CARDS ===
            // Real front touch controls menus natively (y mueve el cursor)
            if (touch.reportNum > 0) {
                float tx = (float)touch.report[0].x * 960.0f / 1920.0f;
                float ty = (float)touch.report[0].y * 544.0f / 1088.0f;
                last_touch_x = tx;
                last_touch_y = ty;
                menu_mouse_x = tx;
                menu_mouse_y = ty;

                if (!was_touching) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, tx, ty); // DOWN
                    was_touching = 1;
                } else {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 1, tx, ty); // MOVE
                }
            } else if (was_touching) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, last_touch_x, last_touch_y); // UP
                was_touching = 0;
            }

            // Cursor virtual con D-pad y stick izquierdo: el engine ilumina
            // el border bajo el raton cada frame (cRBorder::MouseTest).
            if (pad.buttons & SCE_CTRL_UP)    menu_mouse_y -= 10.0f;
            if (pad.buttons & SCE_CTRL_DOWN)  menu_mouse_y += 10.0f;
            if (pad.buttons & SCE_CTRL_LEFT)  menu_mouse_x -= 12.0f;
            if (pad.buttons & SCE_CTRL_RIGHT) menu_mouse_x += 12.0f;
            {
                float sx = (float)(pad.lx - 128) / 128.0f;
                float sy = (float)(pad.ly - 128) / 128.0f;
                if (fabsf(sx) > 0.25f) menu_mouse_x += sx * 12.0f;
                if (fabsf(sy) > 0.25f) menu_mouse_y += sy * 10.0f;
            }
            if (menu_mouse_x < 0.0f) menu_mouse_x = 0.0f;
            if (menu_mouse_x > 959.0f) menu_mouse_x = 959.0f;
            if (menu_mouse_y < 0.0f) menu_mouse_y = 0.0f;
            if (menu_mouse_y > 543.0f) menu_mouse_y = 543.0f;
            // Solo con sub-objetos vivos Y fuera del splash (state 0):
            // MouseSet+0x64 desreferencia base+0x224 de SU propio objeto
            // manager (no Game), NULL hasta que el engine crea los borders.
            if (RShellSetMouse && engine_mouse_ok && !was_touching && game_state >= 1) {
                RShellSetMouse(0, menu_mouse_x * (640.0f / 960.0f),
                                  menu_mouse_y * (480.0f / 544.0f));
            }

            // CROSS y R1 (btn_shoot) tapean DONDE esta el cursor para
            // avanzar/confirmar (Continue, tutorial cards, menus).
            if (just_pressed & g_controls.btn_shoot) {
                if (!was_touching) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, menu_mouse_x, menu_mouse_y);
                    button_touch_active = 1;
                }
            } else if (just_released & g_controls.btn_shoot) {
                if (button_touch_active) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, menu_mouse_x, menu_mouse_y);
                    button_touch_active = 0;
                }
            }
        } else {
            // ================= IN GAMEPLAY =================
            // 1. Steering input (el toque sintetico de disparo/pausa va
            // DESPUES para que su posicion gane el frame del flanco).
            race_frames++;
            if ((race_frames % 300) == 0) {
                extern uint32_t vgl_skipped_tex_draws; // vendor/vitaGL ffp.c
                extern uint32_t g_bad_draws;           // dynlib.c
                extern uint32_t vgl_matrix_stack_errors; // vendor/vitaGL matrices.c
                extern uint32_t g_bad_tex_binds;         // dynlib.c
                extern uint32_t g_trimmed_draws;         // dynlib.c
                l_info("race: frames=%d vram_free=%u ram_free=%u skipped_tex_draws=%u bad_draws=%u matrix_stack_errors=%u bad_tex_binds=%u trimmed_draws=%u", race_frames,
                       (unsigned)vglMemFree(VGL_MEM_VRAM),
                       (unsigned)vglMemFree(VGL_MEM_RAM),
                       (unsigned)vgl_skipped_tex_draws, (unsigned)g_bad_draws,
                       (unsigned)vgl_matrix_stack_errors, (unsigned)g_bad_tex_binds,
                       (unsigned)g_trimmed_draws);
            }
            float lx = (float)(pad.lx - 128) / 128.0f;
            float deadzone = (float)g_controls.deadzone / 100.0f;
            if (fabsf(lx) < deadzone) {
                lx = 0.0f;
            } else {
                float sign = (lx > 0.0f) ? 1.0f : -1.0f;
                lx = sign * (fabsf(lx) - deadzone) / (1.0f - deadzone);
            }

            // Digital buttons override
            if (pad.buttons & g_controls.btn_steer_left)  lx = -1.0f;
            if (pad.buttons & g_controls.btn_steer_right) lx = 1.0f;

            // Apply sensitivity
            lx *= ((float)g_controls.sensitivity / 100.0f);
            if (lx < -1.0f) lx = -1.0f;
            if (lx > 1.0f)  lx = 1.0f;

            // Real physical touch takes priority
            if (touch.reportNum > 0) {
                float tx = (float)touch.report[0].x * 960.0f / 1920.0f;
                float ty = (float)touch.report[0].y * 544.0f / 1088.0f;
                last_touch_x = tx;
                last_touch_y = ty;

                if (!was_touching) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, tx, ty);
                    was_touching = 1;
                } else {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 1, tx, ty);
                }
            } else {
                if (was_touching) {
                    if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, last_touch_x, last_touch_y);
                    was_touching = 0;
                }

                // Physical controls steer Turbo cleanly via RShellSetMouse without triggering clicks
                // Target track position = 320.0f + 2.0f * (mouse_x - 320.0f)
                // When mouse_x = 320.0f + lx * 160.0f: target position = 320.0f + lx * 320.0f in [0, 640]
                float mouse_x = 320.0f + lx * 160.0f;
                float mouse_y = 240.0f; // > 30.0f, active on screen
                if (RShellSetMouse && engine_mouse_ok) {
                    RShellSetMouse(0, mouse_x, mouse_y);
                }
                // Mismo punto en espacio de pantalla 960x544 (JNIMouseEvent
                // escala x*640/960, y*480/544): el toque de disparo cae donde
                // ya apunta el steering y no cambia la posicion del caracol.
                shoot_touch_x = mouse_x * (960.0f / 640.0f);
                shoot_touch_y = mouse_y * (544.0f / 480.0f);
            }

            // 2. Disparo (btn_shoot, CROSS por defecto): el motor solo
            // dispara con un toque (verificado en hardware: ClickOnly/SPACE
            // solos no disparan). El toque va en la posicion de steering.
            // DOWN al pulsar, UP al soltar; el latch del engine mantiene el
            // fuego mientras se sostiene y el steering sigue funcionando.
            if ((just_pressed & g_controls.btn_shoot) && !was_touching && !shoot_touch_active) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, shoot_touch_x, shoot_touch_y);
                shoot_touch_active = 1;
                if (JNIKey) JNIKey(&jni, (jobject)1, 62); // Android KEYCODE_SPACE (via extra)
            } else if (shoot_touch_active && !was_touching) {
                // Disparo sostenido: el toque sigue al steering (izq/centro/der)
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 1, shoot_touch_x, shoot_touch_y);
            }
            if ((pad.buttons & g_controls.btn_shoot) && RShellInputRegisterMouseClickOnly) {
                RShellInputRegisterMouseClickOnly(); // autofuego mientras se mantiene
            }
            if ((just_released & g_controls.btn_shoot) && shoot_touch_active) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, shoot_touch_x, shoot_touch_y);
                shoot_touch_active = 0;
            }
            // El dedo real tiene prioridad: si entra a mitad de un disparo
            // sintetico, se cede el slot tactil sin mandar UP (el UP del
            // dedo real libera el latch; mandar UP aqui cortaria su toque).
            if (was_touching && shoot_touch_active) {
                shoot_touch_active = 0;
            }

            // 3. Pausa (btn_pause, START por defecto): toca el boton "MENU"
            // de arriba-izquierda. nativePause() solo congela el delta de
            // tiempo y no abre el menu de pausa (verificado: START no hacia
            // nada). Ignorado si SELECT acompana (combo del menu START+SELECT).
            if ((just_pressed & g_controls.btn_pause) && !(pad.buttons & SCE_CTRL_SELECT)
                && !was_touching && !pause_touch_active) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, MENU_TOUCH_X, MENU_TOUCH_Y);
                pause_touch_active = 1;
            }
            if ((just_released & g_controls.btn_pause) && pause_touch_active) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, MENU_TOUCH_X, MENU_TOUCH_Y);
                pause_touch_active = 0;
            }
            if (was_touching && pause_touch_active) {
                pause_touch_active = 0;
            }
        }

        last_buttons = pad.buttons;

        // 3. Render frame
        if (nativeRender) {
            nativeRender(&jni, (jobject)1, 0);
        }

        gl_swap();
    }

    audio_term();
    sceKernelExitDeleteThread(0);
}

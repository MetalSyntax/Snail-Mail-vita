#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "audio.h"

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
    void (*JNIAccelerometer)(void *env, void *thiz, float x, float y, float z) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_AccelerometerListener_JNIAccelerometer");
    void (*JNIKey)(void *env, void *thiz, int keycode) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADGLSurfaceView_JNIKey");
    void (*nativePause)(void *env, void *thiz) = 
        (void *)so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_ADGLSurfaceView_nativePause");

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

    log_set_buffered(1);
    l_info("Entering main game loop.");

    int was_touching = 0;
    float last_touch_x = 480.0f;
    float last_touch_y = 272.0f;

    uint32_t last_buttons = 0;
    int button_touch_active = 0;

    while (1) {
        // 1. Process touch
        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        if (touch.reportNum > 0) {
            float tx = (float)touch.report[0].x * 960.0f / 1920.0f;
            float ty = (float)touch.report[0].y * 544.0f / 1088.0f;
            last_touch_x = tx;
            last_touch_y = ty;

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

        // 2. Process controller
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        float lx = (float)(pad.lx - 128) / 128.0f;
        float ly = (float)(pad.ly - 128) / 128.0f;
        if (fabsf(lx) < 0.15f) lx = 0.0f;
        if (fabsf(ly) < 0.15f) ly = 0.0f;

        if (pad.buttons & SCE_CTRL_LEFT)  lx = -1.0f;
        if (pad.buttons & SCE_CTRL_RIGHT) lx = 1.0f;
        if (pad.buttons & SCE_CTRL_UP)    ly = -1.0f;
        if (pad.buttons & SCE_CTRL_DOWN)  ly = 1.0f;

        if (JNIAccelerometer) {
            // Note: in landscape, turning left/right maps to lx, tilt forward/back to ly
            JNIAccelerometer(&jni, (jobject)1, lx, ly, 1.0f);
        }

        // Action button (Cross / R-Trigger) -> shoot / tap
        int action_pressed = (pad.buttons & (SCE_CTRL_CROSS | SCE_CTRL_R1)) != 0;
        int action_just_pressed = action_pressed && !(last_buttons & (SCE_CTRL_CROSS | SCE_CTRL_R1));
        int action_just_released = !action_pressed && (last_buttons & (SCE_CTRL_CROSS | SCE_CTRL_R1));

        if (action_just_pressed) {
            if (JNIKey) JNIKey(&jni, (jobject)1, 62); // Space
            if (!was_touching) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 0, 480.0f, 272.0f);
                button_touch_active = 1;
            }
        } else if (action_just_released) {
            if (button_touch_active) {
                if (JNIMouseEvent) JNIMouseEvent(&jni, (jobject)1, 2, 480.0f, 272.0f);
                button_touch_active = 0;
            }
        }

        // Start button -> Pause
        if ((pad.buttons & SCE_CTRL_START) && !(last_buttons & SCE_CTRL_START)) {
            if (nativePause) nativePause(&jni, (jobject)1);
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

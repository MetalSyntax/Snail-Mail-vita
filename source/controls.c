/*
 * Copyright (C) 2026 Snail Mail PS Vita Port
 *
 * Controls customization module (Carnivores / Vita port style)
 */

#include "controls.h"
#include "utils/logger.h"

#include <psp2/ctrl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define CONTROLS_PATH DATA_PATH "controls.txt"

ControlsConfig g_controls;

typedef struct {
    const char *name;
    uint32_t mask;
} ButtonMapping;

static const ButtonMapping s_button_names[] = {
    { "CROSS",       SCE_CTRL_CROSS },
    { "X",           SCE_CTRL_CROSS },
    { "CIRCLE",      SCE_CTRL_CIRCLE },
    { "O",           SCE_CTRL_CIRCLE },
    { "SQUARE",      SCE_CTRL_SQUARE },
    { "SQ",          SCE_CTRL_SQUARE },
    { "TRIANGLE",    SCE_CTRL_TRIANGLE },
    { "TRI",         SCE_CTRL_TRIANGLE },
    { "L1",          SCE_CTRL_LTRIGGER },
    { "L",           SCE_CTRL_LTRIGGER },
    { "LTRIGGER",    SCE_CTRL_LTRIGGER },
    { "R1",          SCE_CTRL_RTRIGGER },
    { "R",           SCE_CTRL_RTRIGGER },
    { "RTRIGGER",    SCE_CTRL_RTRIGGER },
    { "UP",          SCE_CTRL_UP },
    { "DPAD_UP",     SCE_CTRL_UP },
    { "DOWN",        SCE_CTRL_DOWN },
    { "DPAD_DOWN",   SCE_CTRL_DOWN },
    { "LEFT",        SCE_CTRL_LEFT },
    { "DPAD_LEFT",   SCE_CTRL_LEFT },
    { "RIGHT",       SCE_CTRL_RIGHT },
    { "DPAD_RIGHT",  SCE_CTRL_RIGHT },
    { "START",       SCE_CTRL_START },
    { "SELECT",      SCE_CTRL_SELECT },
};

#define BUTTON_NAMES_COUNT (sizeof(s_button_names) / sizeof(s_button_names[0]))

static void trim_and_upper(const char *in, char *out, size_t max_len) {
    while (*in && isspace((unsigned char)*in)) in++;
    size_t i = 0;
    while (*in && !strchr(" \t,=:#;\r\n", *in) && i < max_len - 1) {
        out[i++] = (char)toupper((unsigned char)*in++);
    }
    out[i] = '\0';
}

static uint32_t parse_single_button(const char *str) {
    char token[32];
    trim_and_upper(str, token, sizeof(token));
    if (!token[0] || strcmp(token, "NONE") == 0) return 0;

    for (size_t i = 0; i < BUTTON_NAMES_COUNT; i++) {
        if (strcmp(token, s_button_names[i].name) == 0) {
            return s_button_names[i].mask;
        }
    }
    l_warn("controls: unknown button '%s'", token);
    return 0;
}

static uint32_t parse_button_list(const char *str) {
    uint32_t mask = 0;
    const char *p = str;
    while (*p && *p != '#' && *p != ';' && *p != '\r' && *p != '\n') {
        mask |= parse_single_button(p);
        while (*p && *p != ',' && *p != '#' && *p != ';' && *p != '\r' && *p != '\n') p++;
        if (*p == ',') p++;
    }
    return mask;
}

static void buttons_to_string(uint32_t mask, char *out, size_t size) {
    out[0] = '\0';
    size_t len = 0;

    static const struct { uint32_t bit; const char *name; } canonical[] = {
        { SCE_CTRL_CROSS,    "CROSS" },
        { SCE_CTRL_CIRCLE,   "CIRCLE" },
        { SCE_CTRL_SQUARE,   "SQUARE" },
        { SCE_CTRL_TRIANGLE, "TRIANGLE" },
        { SCE_CTRL_LTRIGGER, "L1" },
        { SCE_CTRL_RTRIGGER, "R1" },
        { SCE_CTRL_UP,       "UP" },
        { SCE_CTRL_DOWN,     "DOWN" },
        { SCE_CTRL_LEFT,     "LEFT" },
        { SCE_CTRL_RIGHT,    "RIGHT" },
        { SCE_CTRL_START,    "START" },
        { SCE_CTRL_SELECT,   "SELECT" },
    };

    for (size_t i = 0; i < sizeof(canonical) / sizeof(canonical[0]); i++) {
        if (mask & canonical[i].bit) {
            int written = snprintf(out + len, size - len, "%s%s", len ? ", " : "", canonical[i].name);
            if (written > 0 && (size_t)written < size - len) {
                len += written;
            }
        }
    }

    if (len == 0) {
        snprintf(out, size, "NONE");
    }
}

static void controls_defaults(void) {
    // CROSS dispara (principal) + R1 alternativo; START pausa.
    g_controls.btn_shoot       = SCE_CTRL_CROSS | SCE_CTRL_R1;
    g_controls.btn_steer_left  = SCE_CTRL_LEFT;
    g_controls.btn_steer_right = SCE_CTRL_RIGHT;
    g_controls.btn_pause       = SCE_CTRL_START;
    g_controls.deadzone        = 15;
    g_controls.sensitivity     = 100;
}

void controls_reset_defaults(void) {
    controls_defaults();
    controls_save();
}

void controls_mask_to_string(uint32_t mask, char *out, size_t size) {
    buttons_to_string(mask, out, size);
}

uint32_t controls_button_from_mask(uint32_t mask) {
    // Devuelve un solo boton (prioridad estilo Carnivores) desde una mascara.
    static const uint32_t order[] = {
        SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
        SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER,
        SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_RIGHT,
        SCE_CTRL_START, SCE_CTRL_SELECT,
    };
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (mask & order[i]) return order[i];
    }
    return 0;
}

void controls_save(void) {
    FILE *f = fopen(CONTROLS_PATH, "w");
    if (!f) {
        l_error("controls: cannot open %s for writing", CONTROLS_PATH);
        return;
    }

    char shoot_str[64], left_str[64], right_str[64], pause_str[64];
    buttons_to_string(g_controls.btn_shoot, shoot_str, sizeof(shoot_str));
    buttons_to_string(g_controls.btn_steer_left, left_str, sizeof(left_str));
    buttons_to_string(g_controls.btn_steer_right, right_str, sizeof(right_str));
    buttons_to_string(g_controls.btn_pause, pause_str, sizeof(pause_str));

    fprintf(f,
        "# ========================================================\n"
        "# Snail Mail PS Vita - Controls Configuration\n"
        "# Location: %s\n"
        "#\n"
        "# Available Buttons:\n"
        "# CROSS, CIRCLE, SQUARE, TRIANGLE, L1, R1, UP, DOWN, LEFT,\n"
        "# RIGHT, START, SELECT, NONE\n"
        "# (Multiple buttons can be separated by commas)\n"
        "# ========================================================\n"
        "\n"
        "SHOOT = %s\n"
        "STEER_LEFT = %s\n"
        "STEER_RIGHT = %s\n"
        "PAUSE = %s\n"
        "DEADZONE = %d\n"
        "SENSITIVITY = %d\n",
        CONTROLS_PATH,
        shoot_str,
        left_str,
        right_str,
        pause_str,
        g_controls.deadzone,
        g_controls.sensitivity
    );

    fclose(f);
    l_info("controls: saved configuration to %s", CONTROLS_PATH);
}

void controls_load(void) {
    controls_defaults();

    FILE *f = fopen(CONTROLS_PATH, "r");
    if (!f) {
        l_info("controls: %s not found, writing defaults", CONTROLS_PATH);
        controls_save();
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == ';' || *p == '\r' || *p == '\n' || *p == '\0') {
            continue;
        }

        char *eq = strpbrk(p, "=:");
        if (!eq) continue;
        *eq = '\0';
        char *val = eq + 1;

        char key[32];
        trim_and_upper(p, key, sizeof(key));

        if (strcmp(key, "SHOOT") == 0) {
            uint32_t btns = parse_button_list(val);
            if (btns != 0) g_controls.btn_shoot = btns;
        } else if (strcmp(key, "STEER_LEFT") == 0) {
            uint32_t btns = parse_button_list(val);
            if (btns != 0) g_controls.btn_steer_left = btns;
        } else if (strcmp(key, "STEER_RIGHT") == 0) {
            uint32_t btns = parse_button_list(val);
            if (btns != 0) g_controls.btn_steer_right = btns;
        } else if (strcmp(key, "PAUSE") == 0) {
            uint32_t btns = parse_button_list(val);
            if (btns != 0) g_controls.btn_pause = btns;
        } else if (strcmp(key, "DEADZONE") == 0) {
            int dz = atoi(val);
            if (dz >= 5 && dz <= 50) g_controls.deadzone = dz;
        } else if (strcmp(key, "SENSITIVITY") == 0) {
            int sens = atoi(val);
            if (sens >= 20 && sens <= 300) g_controls.sensitivity = sens;
        }
    }

    fclose(f);
    l_info("controls: loaded successfully (SHOOT=0x%x, LEFT=0x%x, RIGHT=0x%x, PAUSE=0x%x)",
           g_controls.btn_shoot, g_controls.btn_steer_left, g_controls.btn_steer_right, g_controls.btn_pause);
}

void controls_init(void) {
    controls_load();
}

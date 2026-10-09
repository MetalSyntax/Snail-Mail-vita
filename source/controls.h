#ifndef CONTROLS_H
#define CONTROLS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t btn_shoot;
    uint32_t btn_steer_left;
    uint32_t btn_steer_right;
    uint32_t btn_pause;
    int deadzone;       // 5 to 50 percent
    int sensitivity;    // 50 to 200 percent
} ControlsConfig;

extern ControlsConfig g_controls;

void controls_init(void);
void controls_load(void);
void controls_save(void);
void controls_reset_defaults(void);

// Single-button canonical name for menu display (CROSS, CIRCLE, ...).
// For combo masks returns "COMBO" style via controls_mask_to_string.
void controls_mask_to_string(uint32_t mask, char *out, size_t size);
uint32_t controls_button_from_mask(uint32_t mask);

#endif // CONTROLS_H

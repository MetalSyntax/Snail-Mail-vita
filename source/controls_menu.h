#ifndef CONTROLS_MENU_H
#define CONTROLS_MENU_H

#include <stdint.h>

void controls_menu_init(void);
int controls_menu_is_open(void);
void controls_menu_open(void);
void controls_menu_close(void);

// Procesa input del menu. Devuelve 1 si el menu consumio el frame
// (el juego debe pausarse y no recibir input ese frame).
int controls_menu_update(uint32_t buttons, uint32_t just_pressed);
void controls_menu_draw(void);

#endif // CONTROLS_MENU_H

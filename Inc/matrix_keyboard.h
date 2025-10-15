#ifndef __MATRIX_KEYBOARD_H__
#define __MATRIX_KEYBOARD_H__

void matrix_keyboard_init_pio(void);
void is_any_key_pressed(void);
void matrix_keyboard_scan(void);
void get_pressed_key(void);


#endif // __MATRIX_KEYBOARD_H__
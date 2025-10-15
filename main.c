#include <stdio.h>
#include "pico/stdlib.h"
#include "Inc/matrix_keyboard.h"

int main()
{
    stdio_init_all();
    sleep_ms(2000); // Wait for USB to be ready
    matrix_keyboard_init_pio();
    while (true) {

    }
}

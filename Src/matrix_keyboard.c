#include "Inc/matrix_keyboard.h"
#include "hardware/pio.h"
#include "matrix_keyboard.pio.h" // Add this if your PIO program header is named like this
#include "Inc/buzzer.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"

#define PIO pio0              ///< PIO instance
#define SM 0                  ///< State machine index
#define BASE_OUT 2            ///< Starting pin for output pins
#define BASE_IN 6             ///< Starting pin for input pins
#define OUTPUT_DIR 1          ///< Set pin direction to output
#define MAX_PASSWORD_LENGTH 4 ///< Maximum length of the password
#define DEBOUNCE_DELAY  500   ///< Debounce delay in milliseconds

static bool key_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t debounce_timer;                ///< Timer for debouncing the button press

static void keyboard_isr_pio(void)                     
{
    pio_sm_set_enabled(PIO, SM, false); // Disable the state machine
    add_repeating_timer_ms(DEBOUNCE_DELAY, key_timer_callback, NULL, &debounce_timer);
    // clear the interrupt
    pio_interrupt_clear(PIO, 0);
}

static inline void detection_keypad(uint32_t key, uint8_t *current_position,  char *password)
{
    const char keymap[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    uint8_t row = (key >> 2);        // Extract row (bits 2 and 3)
    uint8_t col = key & 0x03;        // Extract column (bits 0 and 1)

    if (*current_position < MAX_PASSWORD_LENGTH) {
        password[*current_position] = keymap[row][col];
        (*current_position)++;
    }else {
        password[*current_position] = '\0'; // Null-terminate the string
        *current_position = 0; // Reset for next input
    }
}

static bool key_timer_callback(repeating_timer_t *rt) {
    if(!(gpio_get(BASE_IN)|gpio_get(BASE_IN+1)|gpio_get(BASE_IN+2)|gpio_get(BASE_IN+3))) {
        if (!pio_sm_is_rx_fifo_empty(pio0, 0)) {
            uint32_t key = pio_sm_get(pio0, 0);
            static uint8_t current_position = 0;
            static char password [ MAX_PASSWORD_LENGTH ];

            detection_keypad(key, &current_position, password);
            buzzer_beep();
        }
        pio_sm_set_enabled(PIO, SM, true); // Re-enable the state machine
        return false;  // stop timer
    }
    return true;  // hold timer
}

void matrix_keyboard_init_pio(void)
{
    for (int i = 0; i < 4; i++){
        // output pins
        pio_gpio_init(PIO, BASE_OUT + i);
        // input pins with pull down
        pio_gpio_init(PIO, BASE_IN + i);
        gpio_pull_down(BASE_IN + i);
    }
    // load the pio program into the pio memory
    uint8_t offset = pio_add_program(PIO, &keypad_trigger_program);
    // make a sm config
    pio_sm_config c = keypad_trigger_program_get_default_config(offset);
    // set the 'in' pins
    sm_config_set_in_pins(&c, BASE_IN);
    // set the 4 output pins to output
    pio_sm_set_consecutive_pindirs(PIO, SM, BASE_OUT, 4, OUTPUT_DIR);
    // set the 'set' pins
    sm_config_set_set_pins(&c, BASE_OUT, 4);
    // set shift such that bits shifted by 'in' end up in the lower 16 bits
    sm_config_set_in_shift(&c, 0, false, 0);

    pio_set_irq0_source_enabled(PIO, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, keyboard_isr_pio);
    irq_set_enabled(PIO0_IRQ_0, true);
    
    // init the pio sm with the config
    pio_sm_init(PIO, SM, offset, &c);
    // enable the sm
    pio_sm_set_enabled(PIO, SM, true);
    buzzer_init_pio();
}



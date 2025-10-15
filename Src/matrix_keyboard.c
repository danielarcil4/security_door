#include "Inc/matrix_keyboard.h"
#include "hardware/pio.h"
#include "matrix_keyboard.pio.h" // Add this if your PIO program header is named like this

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"

#define PIO pio0
#define SM 0
#define BASE_OUT 2
#define BASE_IN 6
#define OUTPUT_DIR 1

static void keyboard_isr_pio(void)
{
    if (!pio_sm_is_rx_fifo_empty(pio0, 0)) {
        uint32_t keys = pio_sm_get(pio0, 0);
    }
    // clear the interrupt
    pio_interrupt_clear(PIO, 0);
}

void matrix_keyboard_init_pio(void)
{
    for (int i = 0; i < 4; i++)
        {
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
    sm_config_set_in_shift(&c, 0, 0, 0);

    pio_set_irq0_source_enabled(PIO, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, keyboard_isr_pio);
    irq_set_enabled(PIO0_IRQ_0, true);
    
    // init the pio sm with the config
    pio_sm_init(PIO, SM, offset, &c);
    // enable the sm
    pio_sm_set_enabled(PIO, SM, true);
    
}



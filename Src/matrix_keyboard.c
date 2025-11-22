#include "Inc/matrix_keyboard.h"
#include "hardware/pio.h"
#include "matrix_keyboard.pio.h" // Add this if your PIO program header is named like this
#include "Inc/buzzer.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"

#define SM 0                  ///< State machine index
#define PIO pio0              ///< PIO instance
#define BASE_IN 6             ///< Starting pin for input pins
#define BASE_OUT 2            ///< Starting pin for output pins
#define PIN_RELE 22           ///< Pin number for the relay control
#define OUTPUT_DIR 1          ///< Set pin direction to output
#define DEBOUNCE_DELAY  250   ///< Debounce delay in milliseconds
#define MAX_PASSWORD_LENGTH 4 ///< Maximum length of the password

typedef struct {
    uint8_t row;
    uint8_t col;
    uint32_t key_value;
} key_position_t;

static bool key_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t debounce_timer;                ///< Timer for debouncing the button press

static bool rele_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t on_rele_timer;                ///< Timer for debouncing the button press

// Interrupt Service Routine for keyboard PIO where key press is detected
static void keyboard_isr_pio(void)                     
{
    pio_sm_set_enabled(PIO, SM, false); // Disable the state machine
    add_repeating_timer_ms(DEBOUNCE_DELAY, key_timer_callback, NULL, &debounce_timer);
    // clear the interrupt
    pio_interrupt_clear(PIO, 0);
}

// Function to validate the entered password
bool validate_password(const char *input_password)
{
    for(uint8_t i = 0; i < MAX_PASSWORD_LENGTH; i++) {
        if(input_password[i] != "123A"[i]) return false;
    }
    return true;
}

// Function to decode the key position from the key value
void decode_key(key_position_t * key)
{
    // Decode for last 4 bits 
    if(key->key_value & 0xF000){
        key->row = 0;
        if((key->key_value >> 12) & 0x01){
            key->col = 0;
        } else if((key->key_value >> 12) & 0x02){
            key->col = 1;
        } else if((key->key_value >> 12) & 0x04){
            key->col = 2;
        } else if((key->key_value >> 12) & 0x08){
            key->col = 3;
        }
        // Decode for next 4 bits
    } else if (key->key_value & 0x0F00){
        key->row = 1;
        if((key->key_value >> 8) & 0x01){
            key->col = 0;
        } else if((key->key_value >> 8) & 0x02){
            key->col = 1;
        } else if((key->key_value >> 8) & 0x04){
            key->col = 2;
        } else if((key->key_value >> 8) & 0x08){
            key->col = 3;
        }
        // Decode for next 4 bits
    } else if (key->key_value & 0x00F0){
        key->row = 2;
        if((key->key_value >> 4) & 0x01){
            key->col = 0;
        } else if((key->key_value >> 4) & 0x02){
            key->col = 1;
        } else if((key->key_value >> 4) & 0x04){
            key->col = 2;
        } else if((key->key_value >> 4) & 0x08){
            key->col = 3;
        }
        // Decode for fisrt 4 bits
    }else if (key->key_value & 0x000F){
        key->row = 3;
        if((key->key_value >> 0) & 0x01){
            key->col = 0;
        } else if((key->key_value >> 0) & 0x02){
            key->col = 1;
        } else if((key->key_value >> 0) & 0x04){
            key->col = 2;
        } else if((key->key_value >> 0) & 0x08){
            key->col = 3;
        }
    }else {
        return; // No valid key detected
    }
}

// Function to handle key detection and password input
static inline void detection_keypad(uint32_t key_value, uint8_t *current_position,  char *password)
{
    const char keymap[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    key_position_t key = {0,0, key_value};

    // Extract row and column from the key value
    decode_key(&key);
    
    password[*current_position] = keymap[key.row][key.col];
    if (*current_position < MAX_PASSWORD_LENGTH-1) {
        (*current_position)++;
        play_melody('B'); // Play beep melody on key press
    }else {
        // Play melody based on password validity
        if(validate_password((const char *) password)) {
            printf("Password correct!\n");
            play_melody('C'); // Correct password melody
            gpio_put(PIN_RELE, 1); // Activate relay
            add_repeating_timer_ms(2000, rele_timer_callback, NULL, &on_rele_timer); // Deactivate relay after 2 seconds
        } else {
            play_melody('I'); // Incorrect password melody
        }
        password[*current_position] = '\0'; // Null-terminate the string
        *current_position = 0;              // Reset for next input
    }
}

// Timer callback for debouncing key press
static bool key_timer_callback(repeating_timer_t *rt) {
    if(!(gpio_get(BASE_IN)|gpio_get(BASE_IN+1)|gpio_get(BASE_IN+2)|gpio_get(BASE_IN+3))) {
        if (!pio_sm_is_rx_fifo_empty(pio0, 0)) {
            uint32_t key = pio_sm_get(pio0, 0);
            static uint8_t current_position = 0;
            static char password [ MAX_PASSWORD_LENGTH ];
            // Handle key press and update password
            detection_keypad(key, &current_position, password);
        }
        pio_sm_set_enabled(PIO, SM, true); // Re-enable the state machine
        return false;  // stop timer
    }
    return true;  // hold timer
}

bool rele_timer_callback(repeating_timer_t *rt)
{
    gpio_put(PIN_RELE, 0); // Deactivate relay
    return false;
}

// Initialize the matrix keyboard PIO
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

    // Initialize relay control pin
    gpio_init(PIN_RELE);
    gpio_set_dir(PIN_RELE, GPIO_OUT);
    // put pull-down to ensure relay is off initially
    gpio_pull_down(PIN_RELE);
    gpio_put(PIN_RELE, 0); // Ensure relay is off initially
}



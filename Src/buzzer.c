#include "Inc/buzzer.h"
#include "hardware/pio.h"
#include "buzzer_square.pio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define SM 1
#define PIO pio0
#define OUTPUT_DIR 1
#define BUZZER_PIN 15
#define SYS_CLK 125000000.0f

volatile bool is_playing = true;  ///< Flag to indicate if the song has finished
volatile uint offset;                       ///< Offset of the PIO program in the PIO instruction memory

typedef struct {
    float frequency;
    uint16_t duration_ms;
} note_t;

typedef struct {
    note_t *notes;
    uint16_t length;
    uint16_t current_note_index;
} song_t;

note_t beep_notes[] = {
    {440.0f, 100}
};

note_t correct_password_notes[] = {
    {220.0f, 100},
    {440.0f, 100}
};

song_t beep_song = {
    beep_notes,
    sizeof(beep_notes) / sizeof(beep_notes[0]),
    0
};

song_t correct_password_song = {
    correct_password_notes,
    sizeof(correct_password_notes) / sizeof(correct_password_notes[0]),
    0
};
 
static bool duration_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t duration_timer;                     ///< Timer for duration control

uint32_t cycles_from_freq(float freq) {
    return (uint32_t)(SYS_CLK / (freq * 2));
}

void play_note(note_t note) {
    uint32_t cycles = cycles_from_freq(note.frequency);

    pio_sm_exec(PIO, SM, pio_encode_jmp(offset));  // salta al inicio del programa
    pio_sm_put_blocking(PIO, SM, cycles);
    add_repeating_timer_ms(note.duration_ms, duration_timer_callback, NULL, &duration_timer);
}

static bool duration_timer_callback(repeating_timer_t *rt) {
    beep_song.current_note_index++;

    if (beep_song.current_note_index >= (beep_song.length)) {
        // pin set cycle to 0 using 
        beep_song.current_note_index = 0;
        is_playing = true;
        pio_sm_set_enabled(PIO, SM, false);
        pio_sm_exec(PIO, SM, pio_encode_set(pio_pins, 0));
        pio_sm_drain_tx_fifo(PIO, SM);
        return false;
    }
    play_note(beep_song.notes[beep_song.current_note_index]);
    return false;
}

void buzzer_init_pio() {
    pio_gpio_init(PIO, BUZZER_PIN);
    offset = pio_add_program(PIO, &buzzer_square_program);
    pio_sm_config c = buzzer_square_program_get_default_config(offset);
    pio_sm_set_consecutive_pindirs(PIO, SM, BUZZER_PIN, 1, OUTPUT_DIR);
    sm_config_set_set_pins(&c, BUZZER_PIN, 1);
    sm_config_set_in_shift(&c, 0, false, 0);
    
    pio_sm_init(PIO, SM, offset, &c);
    pio_sm_set_enabled(PIO, SM, false);
}

void buzzer_beep() {
    if(is_playing){
        is_playing = false;
        note_t first_note = beep_song.notes[beep_song.current_note_index];
  
        pio_sm_set_enabled(PIO, SM, true);
        play_note(first_note);
    } 
}

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

volatile bool finish_song = true;  ///< Flag to indicate if the song has finished

typedef struct {
    float frequency;
    uint16_t duration_ms;
} melody_note_t;

typedef struct {
    melody_note_t *notes;
    uint16_t length;
    uint16_t current_note_index;
}song_t;

melody_note_t buzzer_beep_notes[] = {
    {1000.0f, 440},
    {2000.0f, 660}
};

song_t beep_song = {
    .notes = buzzer_beep_notes,
    .length = sizeof(buzzer_beep_notes) / sizeof(buzzer_beep_notes[0]),
    .current_note_index = 0
};
 
static bool duration_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t duration_timer;                     ///< Timer for duration control

uint32_t cycles_from_freq(float freq) {
    return (uint32_t)(SYS_CLK / (freq * 2));
}

void play_song(song_t song) {
    melody_note_t note = song.notes[song.current_note_index];
    uint32_t cycles = cycles_from_freq(note.frequency);
    pio_sm_put(PIO, SM, cycles);
    add_repeating_timer_ms(note.duration_ms, duration_timer_callback, NULL, &duration_timer);
}

static bool duration_timer_callback(repeating_timer_t *rt) {
    beep_song.current_note_index++;
    // disable timer
    cancel_repeating_timer(&duration_timer);
    if (beep_song.current_note_index >= beep_song.length) {
        printf("Song finished\n");
        beep_song.current_note_index = 0;
        finish_song = true;
        pio_sm_set_enabled(PIO, SM, false);
        return false;
    }
    play_song(beep_song);
    return false;
}

void buzzer_init_pio() {
    pio_gpio_init(PIO, BUZZER_PIN);
    uint offset = pio_add_program(PIO, &buzzer_square_program);
    pio_sm_config c = buzzer_square_program_get_default_config(offset);
    pio_sm_set_consecutive_pindirs(PIO, SM, BUZZER_PIN, 1, OUTPUT_DIR);
    sm_config_set_set_pins(&c, BUZZER_PIN, 1);
    
    pio_sm_init(PIO, SM, offset, &c);
    pio_sm_set_enabled(PIO, SM, true);
}

void buzzer_beep() {
    if(finish_song){
        pio_sm_set_enabled(PIO, SM, true);
        finish_song = false;
        play_song(beep_song);
    } 
}

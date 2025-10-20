#include "Inc/buzzer.h"
#include "hardware/pio.h"
#include "buzzer_square.pio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define SM 1                 ///< State machine index    
#define PIO pio0             ///< PIO instance
#define OUTPUT_DIR 1         ///< Set pin direction to output
#define BUZZER_PIN 15        ///< Pin connected to the buzzer
#define SYS_CLK 125000000.0f ///< System clock frequency in Hz

volatile bool is_playing = false;  ///< Flag to indicate if the song has finished
volatile uint offset;              ///< Offset of the PIO program in the PIO instruction memory

///< NOTE AND SONG STRUCTS SECTION >///
typedef struct {                   
    float frequency;
    uint16_t duration_ms;
} note_t;

typedef struct {
    const note_t * notes;
    const uint16_t length;
    uint16_t current_note_index;
} song_t;

/*************************************STRUCTS SONG/NOTES SECTION********************************************/
const note_t beep_notes[] = {
    {440.0f, 100}
};

song_t beep_song = {
    .notes = beep_notes,
    .length = 1,
    .current_note_index = 0
};

const note_t correct_password_notes[] = {
    {220.0f, 500},
    {440.0f, 500}
};

song_t correct_password_song = {
    .notes = correct_password_notes,
    .length = 2,
    .current_note_index = 0
};

const note_t incorrect_password_notes[] = {
    {220.0f, 1000}
};

song_t incorrect_password_song = {
    .notes = incorrect_password_notes,
    .length = 1,
    .current_note_index = 0
};
/******************************************************************************************************/

static bool duration_timer_callback(repeating_timer_t *rt);  ///< Timer callback for debouncing
static repeating_timer_t duration_timer;                     ///< Timer for duration control

// Convert frequency to PIO cycles
uint32_t cycles_from_freq(float freq) {
    return (uint32_t)(SYS_CLK / (freq * 2));
}

// Play a single note
void play_note(note_t note) {
    uint32_t cycles = cycles_from_freq(note.frequency);

    pio_sm_exec(PIO, SM, pio_encode_jmp(offset));  // salta al inicio del programa
    pio_sm_put_blocking(PIO, SM, cycles);
    add_repeating_timer_ms(note.duration_ms, duration_timer_callback, NULL, &duration_timer);
}

// Timer callback to handle note duration
static bool duration_timer_callback(repeating_timer_t *rt) {
    beep_song.current_note_index++;                         // Advance to the next index note

    // Check if the song has finished
    if (beep_song.current_note_index >= (beep_song.length)) {
        // Reset song state
        beep_song.current_note_index = 0;
        is_playing = false;
        pio_sm_set_enabled(PIO, SM, false);                  // Disable the PIO state machine
        pio_sm_exec(PIO, SM, pio_encode_set(pio_pins, 0));   // Turn off the buzzer
        pio_sm_drain_tx_fifo(PIO, SM);                       // Clear TX FIFO
        return false;
    }
    play_note(beep_song.notes[beep_song.current_note_index]); // Play the next note
    return false;
}

// Initialize the buzzer PIO and state machine
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

// Play melody based on the type
void play_melody(char melody_type) {
    if(!is_playing){
        song_t * current_song;        ///< Current song being played
        const note_t * first_note;    ///< Pointer to the first note of the song
        is_playing = true;            ///< Set the playing flag to true
        if(melody_type == 'B'){
            current_song = &beep_song;               ///< Set current song to beep song
            first_note = &(* current_song).notes[beep_song.current_note_index];   /// Get the first note
        } else if(melody_type == 'C'){
            current_song = &correct_password_song;   ///< Set current song to correct password song
            first_note = &(* current_song).notes[correct_password_song.current_note_index]; 
        } else if(melody_type == 'I'){
            current_song = &incorrect_password_song; ///< Set current song to correct password song
            first_note = &(* current_song).notes[incorrect_password_song.current_note_index];
            printf("incorrect password melody\n");
        } else {
            is_playing = false;       ///< Reset playing flag if no song is played
            return;
        }
      
        pio_sm_set_enabled(PIO, SM, true); ///< Enable the PIO state machine
        play_note(*first_note);            ///< Play the first note
    } 
}

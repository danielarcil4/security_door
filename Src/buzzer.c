#include "Inc/buzzer.h"
#include "hardware/pio.h"
#include "buzzer_square.pio.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define SM 1                         ///< State machine index    
#define PIO pio0                     ///< PIO instance
#define OUTPUT_DIR 1                 ///< Set pin direction to output
#define BUZZER_PIN 15                ///< Pin connected to the buzzer
#define SYS_CLK 125000000.0f         ///< System clock frequency in Hz
#define SILENCE_BETWEEN_NOTES_MS 100 ///< Silence duration between notes in milliseconds

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
    {3000.0f, 200}
};

song_t beep_song = {
    .notes = beep_notes,
    .length = 1,
    .current_note_index = 0
};

const note_t correct_password_notes[] = {
    {2200.0f, 500},
    {4400.0f, 1000}
};

song_t correct_password_song = {
    .notes = correct_password_notes,
    .length = 2,
    .current_note_index = 0
};

const note_t incorrect_password_notes[] = {
    {2200.0f, 1000}
};

song_t incorrect_password_song = {
    .notes = incorrect_password_notes,
    .length = 1,
    .current_note_index = 0
};
/******************************************************************************************************/

// Write `period` to the input shift register
void pio_pwm_set_period(uint32_t period) {
    pio_sm_put_blocking(PIO, SM, period);
    pio_sm_exec(PIO, SM, pio_encode_pull(false, false));
    pio_sm_exec(PIO, SM, pio_encode_out(pio_isr, 32));
}

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(uint32_t level) {
    pio_sm_put_blocking(PIO, SM, level);
}

uint32_t freq_to_cicles(float frequency) {
    return (uint32_t)(SYS_CLK / (frequency*2));
}
/******************************************************************************************************/
static bool finished_note_timer_callback(repeating_timer_t *rt);  ///< Timer callback for duration control
static bool silence_timer_callback(repeating_timer_t *rt);   ///< Timer callback for silence between notes
static repeating_timer_t duration_timer;                     ///< Timer for duration control
static repeating_timer_t silence_timer;                      ///< Use a separate timer for silence

// Play a single note
void play_note(const note_t * note) {
    uint32_t cicles = freq_to_cicles((* note).frequency);

    pio_pwm_set_period(cicles);                    ///< Set the period based on frequency
    pio_pwm_set_level(cicles/2);                   ///< Set duty cycle to 50%
    pio_sm_exec(PIO, SM, pio_encode_jmp(offset));  ///< jump to start of program
    pio_sm_set_enabled(PIO, SM, true);             ///< Re-Enable the state machine
    add_repeating_timer_ms((* note).duration_ms, finished_note_timer_callback, NULL, &duration_timer); ///< Set timer for note duration
}

static bool silence_timer_callback(repeating_timer_t *rt) {
    // Advance to the next index note
    beep_song.current_note_index++; 
    // Check if the song has finished
    if (beep_song.current_note_index >= (beep_song.length)) {
        // Reset song state
        beep_song.current_note_index = 0;
        is_playing = false;
        return false;
    }
    // Move to the next note
    const note_t * next_note  = &beep_song.notes[beep_song.current_note_index];
    play_note(next_note); // Play the next note
    return false;
}

// Timer callback to finish note 
static bool finished_note_timer_callback(repeating_timer_t *rt) {
    pio_sm_set_enabled(PIO, SM, false);
    pio_sm_exec(PIO, SM, pio_encode_sideset_opt(1,0));          // Turn off the buzzer
    // pio_sm_drain_tx_fifo(PIO, SM);                       // Clear TX FIFO
    add_repeating_timer_ms(SILENCE_BETWEEN_NOTES_MS, silence_timer_callback, NULL, &silence_timer);
    return false;
}

// Initialize the buzzer PIO and state machine
void buzzer_init_pio() {
    offset = pio_add_program(PIO, &buzzer_square_program);
    pwm_program_init(PIO, SM, offset, BUZZER_PIN);    
    pio_sm_set_enabled(PIO, SM, false);
}

// Play melody based on the type
void play_melody(char melody_type) {
    if(!is_playing){                  /// Only play if no song is currently playing
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
        } else {
            is_playing = false;       ///< Reset playing flag if no song is played
            return;
        }
        play_note(first_note);            ///< Play the first note
    } 
}

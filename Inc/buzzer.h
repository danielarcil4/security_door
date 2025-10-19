#ifndef __BUZZER_H__
#define __BUZZER_H__

void buzzer_init_pio();
void buzzer_beep();
void buzzer_correct_password_beep(void);
void buzzer_incorrect_password_beep(void);


#endif // __BUZZER_H__
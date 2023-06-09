#if __EMSCRIPTEN__
    #include <time.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "watch.h"

#define CARD_DECK_SIZE 11

enum ApplicationMode {
    mode_title,
    mode_intro,
    mode_playing,
    mode_anim,
    mode_finish
};

typedef struct ApplicationState {
    enum ApplicationMode mode;
    uint8_t card_deck[CARD_DECK_SIZE];
    uint8_t dealer_score;
    uint8_t player_score;
    uint8_t drawn_card;
    char display_chars[7];
    uint8_t cursor_position;
    uint8_t top_animation_frame;
    uint8_t bottom_animation_frame;
} ApplicationState;

void cb_alarm_pressed(void);
void cb_mode_pressed(void);
void initialize_new_game(void);
void animate_draw(void);
void deal_card(bool player);
void announce_winner(void);
void announce_win(void);
void announce_failure(void);
uint8_t generate_random_number(void);

ApplicationState application_state;

const uint8_t card_deck_template[CARD_DECK_SIZE] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 16, 4};
const uint8_t animation_loading_template[9][2] = {{1,16}, {2,2}, {2,4}, {2,5}, {1,6}, {0,6}, {0,3}, {0,2}, {1,2}};

const BuzzerNote game_win_beep[] = {
    BUZZER_NOTE_G4,
    BUZZER_NOTE_REST,
    BUZZER_NOTE_G4,
    BUZZER_NOTE_C5,
    BUZZER_NOTE_B4,

    BUZZER_NOTE_C5,
    BUZZER_NOTE_B4,
    BUZZER_NOTE_G4,
    BUZZER_NOTE_A4
};

const BuzzerNote game_fail_beep[] = {
    BUZZER_NOTE_D4,
    BUZZER_NOTE_C4SHARP_D4FLAT,
    BUZZER_NOTE_C4,
    BUZZER_NOTE_B3
};

const uint16_t game_win_beep_durations[] = {
    200,
    50,
    200,
    200,
    200,

    200,
    200,
    200,
    200
};

void app_wake_from_standby(void){}
void app_prepare_for_standby(void){}

void app_init(void) {
    memset(&application_state, 0, sizeof(application_state));

    #if __EMSCRIPTEN__
        srand(time(0));
    #endif
    application_state.mode = mode_title;
}

void app_setup(void) {
    watch_enable_external_interrupts();
    watch_register_extwake_callback(BTN_ALARM, cb_alarm_pressed, INTERRUPT_TRIGGER_RISING);
    watch_register_interrupt_callback(BTN_MODE, cb_mode_pressed, INTERRUPT_TRIGGER_RISING);

    watch_enable_display();

    watch_enable_buzzer();
}

void initialize_new_game(void) {
    application_state.mode = mode_anim;
    application_state.player_score = 0;
    application_state.dealer_score = 0;
    application_state.drawn_card = 0;
    memcpy(application_state.card_deck, card_deck_template, CARD_DECK_SIZE);
    strcpy(application_state.display_chars, "000000");
    application_state.top_animation_frame = 0;
    application_state.bottom_animation_frame = 5;

    watch_display_string("ME", 0);
    watch_display_string("000000", 4);
}

void announce_winner(void) {
    if (application_state.player_score > 21) {
        announce_failure();
    } else if (application_state.dealer_score > 21) {
        announce_win();
    } else if (application_state.player_score > application_state.dealer_score) {
        announce_win();
    } else if (application_state.dealer_score > application_state.player_score) {
        announce_failure();
    }

    application_state.mode = mode_finish;
}

void announce_win(void) {
    for (size_t i = 0; i < 3; i++) {
        for (size_t j = 0; j < 5; j++) {
            watch_buzzer_play_note(game_win_beep[j], game_win_beep_durations[j]);
        }
    }

    for(size_t i = 5, count = sizeof(game_win_beep) / sizeof(game_win_beep[0]); i < count; i++) {
        watch_buzzer_play_note(game_win_beep[i], game_win_beep_durations[i]);
    }

    watch_display_string("ME", 0);
}

void announce_failure(void) {
    for(size_t i = 0, count = sizeof(game_fail_beep) / sizeof(game_fail_beep[0]); i < count; i++) {
        watch_buzzer_play_note(game_fail_beep[i], 300);
    }
    
    watch_display_string("DE", 0);
}

void animate_draw() {
    watch_display_string("  ", 8);
    
    if (application_state.bottom_animation_frame == 9) {
        application_state.bottom_animation_frame = 1;
    }

    if (application_state.top_animation_frame == 9) {
        application_state.mode = mode_intro;
        application_state.top_animation_frame = 0;
    } else {
        application_state.top_animation_frame++;
        watch_set_pixel(animation_loading_template[application_state.top_animation_frame][0], animation_loading_template[application_state.top_animation_frame][1]);
        watch_set_pixel(animation_loading_template[application_state.bottom_animation_frame][0], animation_loading_template[application_state.bottom_animation_frame][1]);
        //sleep(1);
    }
}

void deal_card(bool is_dealer) {
    application_state.drawn_card = generate_random_number();

    if (is_dealer) {
        application_state.dealer_score += application_state.drawn_card;
    } else {
        application_state.player_score += application_state.drawn_card;

        if (application_state.player_score > 21) {
            announce_winner();
        } 
    }
}

void cb_alarm_pressed(void) {
    if (application_state.mode == mode_title) {
        initialize_new_game();

        for (int i = 0; i < CARD_DECK_SIZE; i++) {
            printf("%d ", application_state.card_deck[i]);
        }
        printf("\n");
    } else if (application_state.mode == mode_playing) {
        deal_card(false);
        application_state.mode = mode_anim;
    }
}

void cb_mode_pressed(void) {
    if (application_state.mode == mode_playing) {
        watch_display_string("DE", 0);
        deal_card(true);
        application_state.mode = mode_anim;
    }
}

uint8_t generate_random_number(void) {
    #if __EMSCRIPTEN__
        uint8_t drawn_rand_num = ((rand() % (CARD_DECK_SIZE - 1)) + 1);
    #else
        uint8_t drawn_rand_num = (arc4random_uniform(CARD_DECK_SIZE - 1) + 1);
    #endif

    printf("You rolled: %d\n", drawn_rand_num);

    if (application_state.card_deck[drawn_rand_num - 1] > 0) {
        application_state.card_deck[drawn_rand_num - 1] -= 1;
        return drawn_rand_num;
    } else {
        return generate_random_number();
    }
}

bool app_loop(void) {
    if (application_state.mode == mode_title) {
        watch_display_string("    21", 4);
        return true;
    } else {
        bool loop_return = true;

        switch (application_state.mode) {
            case mode_intro:
                if ((application_state.player_score == 0) && (application_state.dealer_score == 0)) {
                    deal_card(false);
                    application_state.mode = mode_anim;
                    loop_return = false;
                } else if ((application_state.player_score > 0) && (application_state.dealer_score == 0)) {
                    watch_display_string("DE", 0);
                    deal_card(true);
                    application_state.mode = mode_anim;
                    loop_return = false;
                } else {
                    watch_display_string("ME", 0);
                    deal_card(false);
                    application_state.mode = mode_playing;
                    loop_return = false;
                }
                //return false;
            case mode_anim:
                animate_draw();
                loop_return = false;
                break;
            case mode_finish:
                watch_display_string(" WINS  ", 4);
                return true;
        }

        sprintf(application_state.display_chars, "%02d%02d%02d", application_state.dealer_score, application_state.player_score, application_state.drawn_card);
        watch_display_string(application_state.display_chars, 4);

        return loop_return;
    }
}
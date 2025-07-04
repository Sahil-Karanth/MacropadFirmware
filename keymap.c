#include QMK_KEYBOARD_H
#include "raw_hid.h"

#define KEYMAP_UK

// -------------------------------------------------------------------------- //
// Declarations and Globals
// -------------------------------------------------------------------------- //

#define NUM_LAYERS 4
#define HID_BUFFER_SIZE 33

char received_data[HID_BUFFER_SIZE] = "no data";

enum layer_names {
    _BASE,
    _PROGRAMING,
    _GIT,
    _MARKDOWN,
};

int curr_layer = _BASE;


enum custom_keycodes {
    LOCK_COMPUTER = SAFE_RANGE,
    VSCODE_OPEN,
    EMAIL,

    COMMENT_SEPARATOR,
    DOXYGEN_COMMENT,
    NAME_CASE_CHANGE,

    GIT_COMMIT_ALL,
    GIT_COMMIT_TRACKED,
    GIT_STATUS,
};

enum tap_dance_codes {
    TD_CYCLE_LAYERS
};

bool process_record_user(uint16_t keycode, keyrecord_t *record);

// -------------------------------------------------------------------------- //
// Helper Functions
// -------------------------------------------------------------------------- //

void cycleLayers(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        curr_layer = (curr_layer + 1) % NUM_LAYERS;
        layer_move(curr_layer);
    } else if (state->count == 2) {
        curr_layer = (curr_layer - 1 + NUM_LAYERS) % NUM_LAYERS;
        layer_move(curr_layer);
    }
}

void handleOpenVscode(keyrecord_t *record) {
    if (record->event.pressed) {
        tap_code16(LGUI(KC_R));
        wait_ms(100);
        send_string("code");
        tap_code(KC_ENTER);
    }
}

void handleGitCommit(keyrecord_t *record, bool commitTrackedOnly) {
    if (record->event.pressed) {

        // Focus VSCode terminal (Ctrl + `)
        tap_code16(LCTL(KC_GRAVE));
        wait_ms(100);

        // Send git commit command
        if (commitTrackedOnly) {
            send_string("git commit -am '' ");
            tap_code(KC_NUHS); // UK # key
            send_string(" COMMIT TRACKED ONLY");
        } else {
            send_string("git add . && git commit -m ''          ");
            tap_code(KC_NUHS);
            send_string(" COMMIT ALL");
        }

        // Move cursor inside the quotes
        for (int i = 0; i < 23; i++) {
            tap_code(KC_LEFT);
        }
    }
}


void handleGitStatus(keyrecord_t *record) {
    if (record->event.pressed) {
        // Focus VSCode terminal (Ctrl + `)
        tap_code16(LCTL(KC_GRAVE));
        wait_ms(100);

        send_string("git status");

        tap_code(KC_ENTER);
    }
}


void handleCommentSep(keyrecord_t *record) {
    if (record -> event.pressed) {
        send_string("// -------------------------------------------------------------------------- //");
    }
}


void handleDoxygenComment(keyrecord_t *record) {
    if (record->event.pressed) {
        send_string(
            " * @brief BRIEF\n"
            " *\n"
            " * DESCR\n"
            " *\n"
            " * @param PNAME PDESC\n"
            " * @return RDESC\n"
        );
    }
}


void handleNameCaseChange(keyrecord_t *record) {
    if (record -> event.pressed) {
        oled_write_ln(received_data, false);
    }
}

// -------------------------------------------------------------------------- //
// Override Functions1
// -------------------------------------------------------------------------- //


void raw_hid_receive(uint8_t *data, uint8_t length) {
    
    if (length >= HID_BUFFER_SIZE) {
        length = HID_BUFFER_SIZE - 1;
    }
    
    memcpy(received_data, data, length);
    received_data[length] = '\0';
    
    oled_write_ln("got something!", false);
}

// void raw_hid_receive(uint8_t *data, uint8_t length) {
//     uint8_t response[length];
//     memset(response, 0, length);
//     response[0] = 'B';

//     if(data[0] == 'A') {
//         raw_hid_send(response, length);
//     }
// }

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {

        // Home macros
        case LOCK_COMPUTER: {
            tap_code16(LGUI(KC_L));
            return false;
        }
        case VSCODE_OPEN: {
            handleOpenVscode(record);
            return false;
        }
        case EMAIL: {
            if (record->event.pressed) {
                send_string("skkaranth1@gmail.com");
            }
            return false;
        }

        // Git macros
        case GIT_COMMIT_ALL: {
            handleGitCommit(record, false);
            return false;
        }
        case GIT_COMMIT_TRACKED: {
            handleGitCommit(record, true);
            return false;
        }
        case GIT_STATUS: {
            handleGitStatus(record);
            return false;
        }

        // Programming macros
        case COMMENT_SEPARATOR: {
            handleCommentSep(record);
            return false;
        }
        case DOXYGEN_COMMENT: {
            handleDoxygenComment(record);
            return false;
        }
        case NAME_CASE_CHANGE: {
            handleNameCaseChange(record);
            return false;
        }

    }

    return true;
}


bool oled_task_user(void) {
    switch (curr_layer) {
        case _BASE:
            oled_write_ln("Home Layer", false);
            oled_write_ln("\n", false);
            oled_write_ln("< lock computer", false);
            oled_write_ln("v open vscode", false);
            oled_write_ln("> email", false);
            oled_write_ln(received_data, false);
            
            break;
        
        case _PROGRAMING:
            oled_write_ln("Programming Layer", false);
            oled_write_ln("\n", false);
            oled_write_ln("< comment separator", false);
            oled_write_ln("v doxygen comment", false);
            oled_write_ln("> UNIMPLEMENTED", false);
            break;
    
        case _GIT:
            oled_write_ln("Git Layer", false);
            oled_write_ln("\n", false);
            oled_write_ln("< commit all", false);
            oled_write_ln("v commit tracked", false);
            oled_write_ln("> git status", false);
            break;
    
        case _MARKDOWN:
            oled_write_ln("Markdown Layer", false);
            oled_write_ln("\n", false);
            oled_write_ln("< UNIMPLEMENTED", false);
            oled_write_ln("v UNIMPLEMENTED", false);
            oled_write_ln("> UNIMPLEMENTED", false);
            break;
        
    }
    
    return false;
}


// -------------------------------------------------------------------------- //
// Macropad Layout
// -------------------------------------------------------------------------- //

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            TD(TD_CYCLE_LAYERS),
        EMAIL, VSCODE_OPEN, LOCK_COMPUTER
    ),

    
    [_PROGRAMING] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            TD(TD_CYCLE_LAYERS),
        KC_D, DOXYGEN_COMMENT, COMMENT_SEPARATOR
    ),

    [_GIT] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            TD(TD_CYCLE_LAYERS),
        GIT_STATUS, GIT_COMMIT_TRACKED, GIT_COMMIT_ALL
    ),

    [_MARKDOWN] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            TD(TD_CYCLE_LAYERS),
        KC_D, KC_E, KC_F
    ),    
};


tap_dance_action_t tap_dance_actions[] = {
    [TD_CYCLE_LAYERS] = ACTION_TAP_DANCE_FN(cycleLayers),
};

const uint16_t PROGMEM backlight_combo[] = {KC_UP, KC_DOWN, COMBO_END};
combo_t key_combos[] = {
    COMBO(backlight_combo, BL_STEP)
};
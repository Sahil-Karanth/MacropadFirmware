#include QMK_KEYBOARD_H
#define KEYMAP_UK

// -------------------------------------------------------------------------- //
// Declarations and Globals
// -------------------------------------------------------------------------- //

#define NUM_LAYERS 4

enum layer_names {
    _BASE,
    _PROGRAMING,
    _GIT,
    _MARKDOWN,
};

int curr_layer = _BASE;


enum custom_keycodes {
    CYCLE_LAYERS = SAFE_RANGE,

    COMMENT_SEPARATOR,

    GIT_COMMIT_ALL,
    GIT_COMMIT_TRACKED,
    GIT_STATUS,
};

bool process_record_user(uint16_t keycode, keyrecord_t *record);





// -------------------------------------------------------------------------- //
// Helper Functions
// -------------------------------------------------------------------------- //

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
            tap_code(KC_NUHS); // UK # key
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



// -------------------------------------------------------------------------- //
// Override Functions
// -------------------------------------------------------------------------- //


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case CYCLE_LAYERS: {
            if (record -> event.pressed) {
                curr_layer = (curr_layer + 1) % NUM_LAYERS;
                layer_move(curr_layer);
            }
            return false;
        }

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

        case COMMENT_SEPARATOR: {
            handleCommentSep(record);
            return false;
        }

    }

    return true;
}


bool oled_task_user(void) {
    switch (curr_layer) {
        case _BASE:
            oled_write_ln("Home Layer", false);
            break;
        
        case _PROGRAMING:
            oled_write_ln("Programming Layer", false);
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
            CYCLE_LAYERS,
        KC_A, KC_B, KC_C
    ),

    
    [_PROGRAMING] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            CYCLE_LAYERS,
        KC_D, KC_E, KC_F
    ),

    [_GIT] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            CYCLE_LAYERS,
        GIT_STATUS, GIT_COMMIT_TRACKED, GIT_COMMIT_ALL
    ),

    [_MARKDOWN] = LAYOUT(
                           KC_MEDIA_PLAY_PAUSE,
            CYCLE_LAYERS,
        KC_D, KC_E, KC_F
    ),    
};

const uint16_t PROGMEM backlight_combo[] = {KC_UP, KC_DOWN, COMBO_END};
combo_t key_combos[] = {
    COMBO(backlight_combo, BL_STEP)
};
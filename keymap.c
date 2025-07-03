#include QMK_KEYBOARD_H

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
};

bool process_record_user(uint16_t keycode, keyrecord_t *record);


// -------------------------------------------------------------------------- //
// Functions
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
    }
    return true;
}


bool oled_task_user(void) {
    oled_write_ln("Hello, world!", false);

    switch (curr_layer) {
        case _BASE:
            oled_write_ln("Home Layer", false);
            break;
        
        case _PROGRAMING:
            oled_write_ln("Programming Layer", false);
            break;
    
        case _GIT:
            oled_write_ln("Git Layer", false);
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
        KC_D, KC_E, KC_F
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
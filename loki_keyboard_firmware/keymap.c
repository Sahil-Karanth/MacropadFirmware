#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include <stdbool.h>

// -------------------------------------------------------------------------- //
// Declarations and Globals
// -------------------------------------------------------------------------- //

const hsv_t colour_map[] = {
    { HSV_RED },
    { HSV_BLUE },
    { HSV_WHITE },
    { HSV_PURPLE },
    { HSV_YELLOW },
    { HSV_GREEN },
    { HSV_ORANGE },
};


enum custom_keycodes {
    SET_RGB_RED = SAFE_RANGE,
    RGB_TOGGLE,
};

bool rgb_on = true;
bool fixed_red = false;

#define COLOUR_MAP_SIZE (sizeof(colour_map) / sizeof(colour_map[0]))

// -------------------------------------------------------------------------- //
// Custom Key Handlers
// -------------------------------------------------------------------------- //

void handle_rgb_toggle(keyrecord_t *record) {
    if (record -> event.pressed) {
        if (rgb_on) {
            rgblight_disable();
        } else {
            rgblight_enable();
        }
        rgb_on = !rgb_on;
    }
}

void handle_set_rgb_red(keyrecord_t *record) {
    if (record -> event.pressed) {
        if (!fixed_red) {
            rgblight_sethsv(HSV_RED);
            fixed_red = true;
        } else {
            rgblight_sethsv(HSV_BLUE);
            fixed_red = false;
        }
    }
}


// -------------------------------------------------------------------------- //
// QMK Override Functions
// -------------------------------------------------------------------------- //

void keyboard_post_init_user(void) {
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv(HSV_BLUE);
}

void raw_hid_receive(uint8_t *data, uint8_t length) {

    if (fixed_red) {
        return;
    }

    // Validate input length
    if (length < 2) {
        print("LOKI: Invalid data length\n");
        return;
    }
    
    print("RECEIVED ON LOKI\n");
    
    uint8_t layer_num = data[0];
    
    if (layer_num >= COLOUR_MAP_SIZE) {
        printf("LOKI: Invalid layer %d (max: %d)\n", layer_num, COLOUR_MAP_SIZE - 1);
        return;
    }
    
    hsv_t colour_struct = colour_map[layer_num];
    printf("LOKI: Setting RGB to layer %d\n", layer_num);
    
    rgblight_sethsv(colour_struct.h, colour_struct.s, colour_struct.v);
}


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case RGB_TOGGLE: {
            handle_rgb_toggle(record);
            return false;
        }
        case SET_RGB_RED: {
            handle_set_rgb_red(record);
            return false;
        }
    }

    return true;
}


// -------------------------------------------------------------------------- //
// Main Keyboard Layout
// -------------------------------------------------------------------------- //

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[0] = LAYOUT_all( /* Base */
    QK_GESC, KC_1,    KC_2,   KC_3,   KC_4,   KC_5,   KC_6,   KC_7,   KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, SET_RGB_RED,
    KC_TAB,  KC_Q,    KC_W,   KC_E,   KC_R,   KC_T,   KC_Y,   KC_U,   KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, RGB_TOGGLE,
    KC_CAPS, KC_A,    KC_S,   KC_D,   KC_F,   KC_G,   KC_H,   KC_J,   KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENT,  SET_RGB_RED,
    KC_LSFT, KC_NUBS, KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,   KC_N,   KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_BSLS, KC_UP,   KC_END,
    KC_LCTL, KC_LGUI, KC_LALT,                        KC_SPC,                           MO(1),   KC_RCTL, KC_LEFT, KC_DOWN, KC_RGHT),

[1] = LAYOUT_all( /* FN */
    KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL,  KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_VOLU, KC_VOLD, KC_MUTE,                            KC_TRNS,                            KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)
};

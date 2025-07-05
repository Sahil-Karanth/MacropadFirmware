#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "string.h"
#include "stdbool.h"
#include "print.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define KEYMAP_UK

// -------------------------------------------------------------------------- //
// Declarations and Globals
// -------------------------------------------------------------------------- //

#define NUM_LAYERS 4
#define HID_BUFFER_SIZE 33

// Globals for Raw HID communication
char received_data[HID_BUFFER_SIZE] = "n/a";
// volatile bool has_new_data = false;
static uint32_t pc_status_timer = 0;

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


// -------------------------------------------------------------------------- //
// Raw HID Declarations
// -------------------------------------------------------------------------- //

enum PC_req_types {
    PC_PERFORMANCE = 1,
    CURRENT_SONG,
};

#define MAX_QUEUE_SIZE 100

typedef struct {
    int data[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} queue_t;

void initQueue(queue_t *q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
}

int isFull(queue_t *q) {
    return q->size == MAX_QUEUE_SIZE;
}

int isEmpty(queue_t *q) {
    return q->size == 0;
}

int enqueue(queue_t *q, int value) {
    if (isFull(q)) return 0;
    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->size++;
    return 1;
}

int dequeue(queue_t *q, int *value) {
    if (isEmpty(q)) return 0;
    *value = q->data[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return 1;
}


// the request queue (initially empty)
queue_t req_queue;

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
        tap_code16(LCTL(KC_GRAVE));
        wait_ms(100);
        if (commitTrackedOnly) {
            send_string("git commit -am '' ");
            tap_code(KC_NUHS); // UK # key
            send_string(" COMMIT TRACKED ONLY");
        } else {
            send_string("git add . && git commit -m ''       ");
            tap_code(KC_NUHS);
            send_string(" COMMIT ALL");
        }
        for (int i = 0; i < 23; i++) {
            tap_code(KC_LEFT);
        }
    }
}

void handleGitStatus(keyrecord_t *record) {
    if (record->event.pressed) {
        tap_code16(LCTL(KC_GRAVE));
        wait_ms(100);
        send_string("git status");
        tap_code(KC_ENTER);
    }
}

void handleCommentSep(keyrecord_t *record) {
    if (record->event.pressed) {
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
    if (record->event.pressed) {
        oled_write_ln(received_data, false);
    }
}

// -------------------------------------------------------------------------- //
// QMK Override Functions
// -------------------------------------------------------------------------- //


void keyboard_post_init_user(void) {
    initQueue(&req_queue);
}


void matrix_scan_user(void) {
    // Check if 5 seconds have passed since the last request
    if (timer_elapsed32(pc_status_timer) > 5000) {

        // Reset the timer
        pc_status_timer = timer_read32();

        // queue the byte
        enqueue(&req_queue, PC_PERFORMANCE);

        print("byte enqueued\n");
    }
}


void raw_hid_receive(uint8_t *data, uint8_t length) {

    // if (length >= HID_BUFFER_SIZE) {
    //     length = HID_BUFFER_SIZE - 1;
    // }

    print("Raw HID data received\n");

    // save received data
    memcpy(received_data, data, length);
    received_data[length] = '\0';

    // responding to client with next request
    uint8_t response[length];
    memset(response, 0, length);

    int req_enum;
    dequeue(&req_queue, &req_enum);

    response[0] = req_enum;

    raw_hid_send(response, length);

    print("Next raw HID request sent\n");

}


// This function runs to update the OLED display
bool oled_task_user(void) {
    // This static buffer will hold the last valid parsed data
    static char pc_status_str[100] = "RAM:-- CPU:--";

    char ram_buf[16] = "--";
    char cpu_buf[16] = "--";
    
    // Parse the new data
    sscanf(received_data, "%15[^|]|%15s", ram_buf, cpu_buf);

    // Format the display string and save it for future display
    snprintf(pc_status_str, sizeof(pc_status_str), "RAM:%s CPU:%s", ram_buf, cpu_buf);

    switch (curr_layer) {
        case _BASE:
            oled_write_ln("Home Layer", false);
            oled_write_ln("< lock computer", false);
            oled_write_ln("v open vscode", false);
            oled_write_ln("> email", false);
            break;
        case _PROGRAMING:
            oled_write_ln("Programming Layer", false);
            oled_write_ln("< comment separator", false);
            oled_write_ln("v doxygen comment", false);
            oled_write_ln("> UNIMPLEMENTED", false);
            break;
        case _GIT:
            oled_write_ln("Git Layer", false);
            oled_write_ln("< commit all", false);
            oled_write_ln("v commit tracked", false);
            oled_write_ln("> git status", false);
            break;
        case _MARKDOWN:
            oled_write_ln("Markdown Layer", false);
            oled_write_ln("< UNIMPLEMENTED", false);
            oled_write_ln("v UNIMPLEMENTED", false);
            oled_write_ln("> UNIMPLEMENTED", false);
            break;
    }
    
    oled_write_ln("\n", false);
    oled_write_ln(pc_status_str, false);
    
    return false;
}

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
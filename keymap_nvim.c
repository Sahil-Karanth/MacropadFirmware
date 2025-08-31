// same as keymap.c but replace the git layer with neovim stuff 

#include QMK_KEYBOARD_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "raw_hid.h"
#include "print.h"

#include "bitmaps.h"

#define KEYMAP_UK

// -------------------------------------------------------------------------- //
// Declarations and Globals
// -------------------------------------------------------------------------- //

#define NUM_LAYERS_TO_CYCLE 7
#define HID_BUFFER_SIZE 33

#define NUM_SCREEN_LINES 8
#define SCREEN_CHAR_WIDTH 20

// Globals for Raw HID communication
char received_data[HID_BUFFER_SIZE] = "--";
char received_pc_stats[HID_BUFFER_SIZE] = "--";
char received_network_stats[HID_BUFFER_SIZE] = "--";
char received_song_info[HID_BUFFER_SIZE] = "--";
char received_timer_info[HID_BUFFER_SIZE] = "--";

static uint32_t pc_status_timer = 0;
static uint32_t blink_timer = 0;
static uint32_t conditional_timer_poll = 0; // for timer completion polling on other layers (when timer active)

static bool blink_state = false;
bool received_first_communication = false; // only build queue after we connect

enum layer_names {
    _BASE,
    _PROGRAMING,
    _NVIM,
    _MARKDOWN,
    _NETWORK,
    _MEDIA,
    _POMODORO,
    _ARROWS,
};

enum blink_colours_as_layer {
    WHITE = _NVIM,
    GREEN = _MEDIA,
};

int curr_layer = _BASE;
int return_layer = _BASE; // for arrow toggle
int chosen_image = 0;

bool display_enabled = true;
bool timer_completed = false;
bool timer_active = false;

enum custom_keycodes {
    LOCK_COMPUTER = SAFE_RANGE,
    VSCODE_OPEN,
    EMAIL,
    COMMENT_SEPARATOR,
    DOXYGEN_COMMENT,
    TODO_COMMENT,
    REQUEST_RETEST_KEY,
    ARROW_TOGGLE,
    SNIPPING_TOOL,
    CODE_BLOCK,
    LATEX_BLOCK,
    LATEX_BLOCK_INLINE,
    DISPLAY_TOGGLE,
    TIMER_PAUSE,
    TIMER_RESTART,
    TIMER_RESET,
    NVIM_FIND_AND_REPLACE,
    CLOSE_NVIM_BUFFERS,
    OPEN_LUA_INIT,
};

enum tap_dance_codes {
    TD_LAYER_CYCLE,
};

// -------------------------------------------------------------------------- //
// Raw HID Declarations
// -------------------------------------------------------------------------- //

enum PC_req_types {
    PC_PERFORMANCE = 1,
    NETWORK_TEST = 2,
    CURRENT_SONG = 3,
    REQUEST_RETEST = 4,
    RGB_SEND = 5,
    TIMER_STATUS = 6,
    TIMER_PAUSE_REQ = 7,
    TIMER_RESTART_REQ = 8,
    TIMER_RESET_REQ = 9,
};

#define MAX_QUEUE_SIZE 100

typedef struct {
    int data[MAX_QUEUE_SIZE];
    int size;
} queue_t;

// Function declarations
void initQueue(queue_t *q);
int isFull(queue_t *q);
int isEmpty(queue_t *q);
int enqueue(queue_t *q, int value);
int dequeue(queue_t *q, int *value);
void cycleLayers(bool forward);
void handleOpenVscode(keyrecord_t *record);
void handleCommandRun(keyrecord_t *record, char *command_str);
void handleCommentSep(keyrecord_t *record);
void handleDoxygenComment(keyrecord_t *record);
void handleArrowToggle(keyrecord_t *record);
void copy_buffer(uint8_t *src_buf, char *dest_buf);
void categorise_received_data(void);
void write_pc_status_oled(void);
void write_network_oled(void);
void write_song_info_oled(void);
void write_timer_info_oled(void);

// -------------------------------------------------------------------------- //
// LIFO queue
// -------------------------------------------------------------------------- //

void initQueue(queue_t *q) {
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
    q->data[q->size++] = value;
    return 1;
}

// Pop from the stack (LIFO)
int dequeue(queue_t *q, int *value) {
    if (isEmpty(q)) return 0;
    *value = q->data[--q->size];
    return 1;
}

queue_t req_queue;

// -------------------------------------------------------------------------- //
// Helper Functions
// -------------------------------------------------------------------------- //

void send_rgb_to_keyboard(int data_to_send) {

    if (received_first_communication) {
        uint8_t rgb_send_buffer[HID_BUFFER_SIZE - 1];
        memset(rgb_send_buffer, 0, HID_BUFFER_SIZE - 1);
    
        rgb_send_buffer[0] = RGB_SEND;
        rgb_send_buffer[1] = data_to_send; // either layer number or the blinking colour
    
        raw_hid_send(rgb_send_buffer, HID_BUFFER_SIZE - 1);
    }
}

int random_int_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

void cycleLayers(bool forward) {
    if (forward) {
        curr_layer = (curr_layer + 1) % NUM_LAYERS_TO_CYCLE;
        layer_move(curr_layer);
    } else {
        curr_layer = (curr_layer - 1 + NUM_LAYERS_TO_CYCLE) % NUM_LAYERS_TO_CYCLE;
        layer_move(curr_layer);
    }

    // Reset timer completion state when switching layers
    // timer_completed = false;

    // send rgb to python client to forward to keyboard
    send_rgb_to_keyboard(curr_layer);
}


void layerCycleTapDance(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        cycleLayers(true);
    } else if (state->count == 2) {
        cycleLayers(false);
    }
}


void handleNvimBufferClose(keyrecord_t *record) {

  if (record->event.pressed) {
    tap_code16(LSFT(KC_SEMICOLON));
    tap_code16(LSFT(KC_5));
    tap_code(KC_B);
    tap_code(KC_D);
    tap_code16(LSFT(KC_NUBS));
    tap_code(KC_E);
    tap_code(KC_NUHS);
  }
}

void handleNvimReplace(keyrecord_t *record) {

  if (record->event.pressed) {
    tap_code16(KC_COLON);
    SEND_STRING("%s///g");
    for (int i = 0; i < 3; i++) {
      tap_code16(KC_LEFT);
    }
  }
}

void handleOpenLuaInit(keyrecord_t *record) {

  if (record->event.pressed) {
        tap_code16(LSFT(KC_SEMICOLON));
        tap_code(KC_E);
        tap_code(KC_SPC);
        tap_code16(LSFT(KC_NUHS));
        SEND_STRING("/.config/nvim/init.lua");
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

void handleCommandRun(keyrecord_t *record, char *command_str) {
    if (record->event.pressed) {
        tap_code16(LCTL(KC_GRAVE));
        wait_ms(100);
        send_string(command_str);
        tap_code(KC_ENTER);
    }
}

void handleDateTodoComment(keyrecord_t *record) {
    if (record -> event.pressed) {
        send_string("// TODO (");
        tap_code16(KC_F12); // autohotkey bound to date
        wait_ms(200);
        send_string("): ");
    }
}

void handleCommentSep(keyrecord_t *record) {
    if (record->event.pressed) {
        send_string("// -------------------------------------------------------------------------- //\n");
        send_string("// SECTION_TITLE\n");
        send_string("// -------------------------------------------------------------------------- //\n");
    }
}


void handleDoxygenComment(keyrecord_t *record) {
    if (record->event.pressed) {
        SEND_STRING("/**" SS_TAP(X_ENTER)); // C comment but replace with triple quotes for python
        tap_code(KC_ENTER);
        tap_code(KC_UP);


        SEND_STRING(" * \"brief BRIEF" SS_TAP(X_ENTER));
        tap_code(KC_BSPC); // Remove auto-indent
        SEND_STRING(" *" SS_TAP(X_ENTER));
        tap_code(KC_BSPC);
        SEND_STRING(" * DESCR" SS_TAP(X_ENTER));
        tap_code(KC_BSPC);
        SEND_STRING(" *" SS_TAP(X_ENTER));
        tap_code(KC_BSPC);
        SEND_STRING(" * \"param PNAME PDESC" SS_TAP(X_ENTER));
        tap_code(KC_BSPC);
        SEND_STRING(" * \"return RDESC");
    }
}


void handleArrowToggle(keyrecord_t *record) {

    if (record -> event.pressed) {
        
        // display random image
        chosen_image = random_int_range(0, NUM_IMAGES - 1);

        if (curr_layer != _ARROWS) {
            return_layer = curr_layer;
            curr_layer = _ARROWS;
        } else {
            curr_layer = return_layer;
        }
        layer_move(curr_layer);
    }
}

void handleExitArrows(keyrecord_t *record) {

    if (record -> event.pressed) {
        curr_layer = return_layer;
        layer_move(curr_layer);
    }
}

void handleSnippingTool(keyrecord_t *record) {
    if (record -> event.pressed) {
        tap_code16(LGUI(LSFT(KC_S)));
    }
}

void handleCodeBlock(keyrecord_t *record) {
    if (record -> event.pressed) {
        for (int i = 0; i < 3; i++) {
            tap_code16(KC_GRAVE);
        }
    }
}

void handleLatexBlock(keyrecord_t *record) {
    if (record->event.pressed) {
        for (int i = 0; i < 4; i++) {
            tap_code16(LSFT(KC_4));
        }
        tap_code(KC_LEFT);
        tap_code(KC_LEFT);
        tap_code(KC_ENTER);
        tap_code(KC_ENTER);
        tap_code(KC_UP);
    }
}

void handleLatexInline(keyrecord_t *record) {
    if (record -> event.pressed) {
        for (int i = 0; i < 2; i++) {
            tap_code16(LSFT(KC_4));
        }
        tap_code(KC_LEFT);
    }
}


void copy_buffer(uint8_t *src_buf, char *dest_buf) {
    memcpy(dest_buf, (char*)src_buf, HID_BUFFER_SIZE - 1);
    dest_buf[HID_BUFFER_SIZE - 1] = '\0';
}

void categorise_received_data(void) {
    int data_id = received_data[0] - '0';

    switch (data_id) {
        case PC_PERFORMANCE:
            copy_buffer((uint8_t*)(received_data + 1), received_pc_stats);
            break;
        case NETWORK_TEST:
            copy_buffer((uint8_t*)(received_data + 1), received_network_stats);
            break;
        case CURRENT_SONG:
            copy_buffer((uint8_t*)(received_data + 1), received_song_info);
            break;
        case TIMER_STATUS:
            copy_buffer((uint8_t*)(received_data + 1), received_timer_info);
            // Check if timer completed
            if (strstr(received_timer_info, "COMPLETED") != NULL) {
                timer_completed = true;
                timer_active = false;
                blink_timer = timer_read32();
            } else {
                timer_completed = false;
            }
            break;
    }
}

void write_pc_status_oled(void) {
    static char pc_status_str[100] = "RAM:-- CPU:-- B:--";

    char ram_buf[16] = "--";
    char cpu_buf[16] = "--";
    char bat_buf[16] = "--";

    // Parse the new data
    sscanf(received_pc_stats, "%15[^|]|%15[^|]|%15s", ram_buf, cpu_buf, bat_buf);

    snprintf(pc_status_str, sizeof(pc_status_str), "RAM:%s CPU:%s B:%s", ram_buf, cpu_buf, bat_buf);

    oled_write_ln(pc_status_str, false);
    oled_write_ln("", false);
}

void write_network_oled(void) {
    static char network_display[200] = "No test data";
    
    // Check if we have received network data
    if (strcmp(received_network_stats, "--") != 0) {
        char download[16] = "--";
        char upload[16] = "--";
        
        // Parse the received data
        sscanf(received_network_stats, "%15[^|]|%15s", download, upload);
        
        // Check if it's a test in progress
        if (strstr(download, "testing") != NULL) {
            snprintf(network_display, sizeof(network_display), 
                     "Testing... %s", upload);
        } else if (strcmp(download, "--") != 0 && strcmp(upload, "--") != 0) {
            // We have actual results
            snprintf(network_display, sizeof(network_display), 
                     "Download: %s Mbps\nUpload: %s Mbps", download, upload);
        } else {
            strcpy(network_display, "No data available");
        }
    }
    
    oled_write_ln(network_display, false);
    oled_write_ln("", false);
}

void write_song_info_oled(void) {
    // First, check if the data from the host is the default "no data" message.
    if (strcmp(received_song_info, "--") == 0 || strcmp(received_song_info, "--|--") == 0) {
        oled_write_ln("No song playing", false);
        oled_write_ln("", false);
        return;
    }

    char song_name[32] = "";
    char artist_name[32] = "";

    // Parse the "song|artist" string received from client.
    sscanf(received_song_info, "%31[^|]|%31[^\n]", song_name, artist_name);

    // Check if parsing was successful and gave us a valid song title.
    if (strlen(song_name) > 0) {

        oled_write_ln(song_name, false);
        oled_write_ln(artist_name, false);

    } else {
        oled_write_ln("No song playing", false);
        oled_write_ln("", false);
    }
}

void write_timer_info_oled(void) {
    // Check if we have received timer data
    if (strcmp(received_timer_info, "--") == 0) {
        oled_write_ln("Timer not started", false);
        oled_write_ln("", false);
        return;
    }

    char status[16] = "";
    char time_remaining[16] = "";

    // Parse the "status|time" string received from client
    sscanf(received_timer_info, "%15[^|]|%15s", status, time_remaining);

    if (strcmp(status, "COMPLETED") == 0) {
        oled_write_ln("TIMER FINISHED!", false);
    } else if (strcmp(status, "PAUSED") == 0) {
        oled_write_ln("Timer PAUSED", false);
        oled_write_ln(time_remaining, false);
    } else if (strcmp(status, "RUNNING") == 0) {
        oled_write_ln(time_remaining, false);
    } else {
        // handles the "STOPPED" state and any other initial states
        oled_write_ln("Timer Ready", false);
    }

    oled_write_ln("", false);
}


// -------------------------------------------------------------------------- //
// QMK Override Functions
// -------------------------------------------------------------------------- //


bool encoder_update_user(uint8_t index, bool clockwise) {
    if (!clockwise) {
        tap_code(KC_VOLU);
    } else {
        tap_code(KC_VOLD);
    }

    return false;
}


void keyboard_post_init_user(void) {
    initQueue(&req_queue);

    backlight_disable();
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv(HSV_RED);
}

void matrix_scan_user(void) {
    // Handle timer completion blinking
    if (timer_completed && timer_elapsed32(blink_timer) > 2000) {
        blink_timer = timer_read32();
        blink_state = !blink_state;
        if (blink_state) {
            rgblight_sethsv(HSV_WHITE);
            send_rgb_to_keyboard(WHITE);
        } else {
            rgblight_sethsv(HSV_GREEN);
            send_rgb_to_keyboard(GREEN);
        }
    }

    // Check if 2 seconds have passed since the last request
    if (timer_elapsed32(pc_status_timer) > 2000 && received_first_communication) {
        // Reset the timer
        pc_status_timer = timer_read32();
        if (curr_layer <= _MARKDOWN) {
            enqueue(&req_queue, PC_PERFORMANCE);
        } else if (curr_layer == _NETWORK) {
            enqueue(&req_queue, NETWORK_TEST);
        } else if (curr_layer == _MEDIA) {
            enqueue(&req_queue, CURRENT_SONG);
        } else if (curr_layer == _POMODORO) {
            enqueue(&req_queue, TIMER_STATUS);
        }
    }

    if (timer_active && timer_elapsed32(conditional_timer_poll) > 30000) {
        conditional_timer_poll = timer_read32();
        enqueue(&req_queue, TIMER_STATUS);
    }
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    // printf("Raw HID data received\n");

    if (!received_first_communication) {
        received_first_communication = true;
    }

    // save received data
    copy_buffer(data, received_data);

    categorise_received_data();

    // responding to client with next request
    uint8_t response[length];
    memset(response, 0, length);

    int req_enum;
    dequeue(&req_queue, &req_enum);

    response[0] = req_enum;

    raw_hid_send(response, length);

}

// This function runs to update the OLED display
bool oled_task_user(void) {

    if (display_enabled) {
        oled_on();
    } else {
        oled_off();
        return false;
    }

    static int last_layer = -1;
    
    if (last_layer != curr_layer) {
        oled_clear();
        last_layer = curr_layer;
    }

    switch (curr_layer) {
        case _BASE:
            if (!timer_completed) rgblight_sethsv(HSV_RED);
            oled_write_ln("Home Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< lock computer", false);
            oled_write_ln("v open vscode", false);
            oled_write_ln("> email", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _PROGRAMING:
            if (!timer_completed) rgblight_sethsv(HSV_BLUE);
            oled_write_ln("Programming Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< comment separator", false);
            oled_write_ln("v doxygen comment", false);
            oled_write_ln("> todo comment", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _NVIM:
            if (!timer_completed) rgblight_sethsv(HSV_WHITE);
            oled_write_ln("Nvim Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< open init.lua", false);
            oled_write_ln("v delete buffers", false);
            oled_write_ln("> find and replace", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _MARKDOWN:
            if (!timer_completed) rgblight_sethsv(HSV_PURPLE);
            oled_write_ln("Markdown Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< code block", false);
            oled_write_ln("v latex block", false);
            oled_write_ln("> latex inline", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _NETWORK:
            if (!timer_completed) rgblight_sethsv(HSV_YELLOW);
            oled_write_ln("Internet Speed", false);
            oled_write_ln("", false);
            oled_write_ln("'v' to retest", false);
            oled_write_ln("", false);
            write_network_oled();
            break;
        case _MEDIA:
            if (!timer_completed) rgblight_sethsv(HSV_GREEN);
            oled_write_ln("Media Player", false);
            oled_write_ln("", false);
            oled_write_ln("", false);
            write_song_info_oled();
            break;
        case _POMODORO:
            if (!timer_completed) rgblight_sethsv(HSV_ORANGE);
            oled_write_ln("Pomodoro Timer", false);
            oled_write_ln("", false);
            oled_write_ln("< reset | v pause", false);
            oled_write_ln("> start/restart", false);
            oled_write_ln("", false);
            write_timer_info_oled();
            break;
        case _ARROWS:
            if (!timer_completed) rgblight_sethsv(HSV_CYAN);
            oled_write_ln("Arrow Layer", false);
            oled_write_ln("", false);
            oled_write_raw_P((const char *)bitmaps_arr[chosen_image], BITMAP_SIZE);
            break;
    }

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
                send_string("skkaranth1\"gmail.com");
            }
            return false;
        }
        // Nvim macros
        case OPEN_LUA_INIT: {
            handleOpenLuaInit(record);
            return false;
        }
        case CLOSE_NVIM_BUFFERS: {
            handleNvimBufferClose(record);
            return false;
        }
        case NVIM_FIND_AND_REPLACE: {
            handleNvimReplace(record);
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
        case TODO_COMMENT: {
            handleDateTodoComment(record);
            return false;
        }
        // markdown (obsidian) macros
        case CODE_BLOCK: {
            handleCodeBlock(record);
            return false;
        }
        case LATEX_BLOCK: {
            handleLatexBlock(record);
            return false;
        }
        case LATEX_BLOCK_INLINE: {
            handleLatexInline(record);
            return false;
        }
        // To arrow layer
        case ARROW_TOGGLE: {
            handleArrowToggle(record);
            return false;
        }
        case SNIPPING_TOOL: {
            handleSnippingTool(record);
            return false;
        }
        // Network layer macro
        case REQUEST_RETEST_KEY: {
            if (record->event.pressed) {
                enqueue(&req_queue, REQUEST_RETEST);
            }
            return false;
        }
        // Timer macros
        case TIMER_PAUSE: {
            if (record->event.pressed) {
                enqueue(&req_queue, TIMER_PAUSE_REQ);
            }
            return false;
        }
        case TIMER_RESTART: {
            if (record->event.pressed) {
                enqueue(&req_queue, TIMER_RESTART_REQ);
                timer_completed = false;
                timer_active = true;
            }
            return false;
        }
        case TIMER_RESET: {
            if (record->event.pressed) {
                enqueue(&req_queue, TIMER_RESET_REQ);
                timer_completed = false;
                timer_active = false;
            }
            return false;
        }
        case DISPLAY_TOGGLE: {
            if (record->event.pressed) {
                display_enabled = !display_enabled;
                if (display_enabled) {
                    rgblight_enable();
                } else {
                    rgblight_disable();
                }
            }
            return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------------- //
// Tap Dance Configuration
// -------------------------------------------------------------------------- //

tap_dance_action_t tap_dance_actions[] = {
    [TD_LAYER_CYCLE] = ACTION_TAP_DANCE_FN(layerCycleTapDance),
};

// -------------------------------------------------------------------------- //
// Macropad Layout
// -------------------------------------------------------------------------- //

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),  // Up arrow: tap=cycle forward, double-tap=cycle back
        EMAIL, VSCODE_OPEN, LOCK_COMPUTER
    ),
    [_PROGRAMING] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        TODO_COMMENT, DOXYGEN_COMMENT, COMMENT_SEPARATOR
    ),
    [_NVIM] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        NVIM_FIND_AND_REPLACE, CLOSE_NVIM_BUFFERS, OPEN_LUA_INIT
    ),
    [_MARKDOWN] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        LATEX_BLOCK_INLINE, LATEX_BLOCK, CODE_BLOCK
    ),
    [_NETWORK] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        KC_PGUP, REQUEST_RETEST_KEY, KC_PGDN
    ),
    [_MEDIA] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        KC_MEDIA_NEXT_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_PREV_TRACK
    ),
    [_POMODORO] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        TD(TD_LAYER_CYCLE),
        TIMER_RESTART, TIMER_PAUSE, TIMER_RESET
    ),
    [_ARROWS] = LAYOUT(
        KC_MEDIA_PLAY_PAUSE,
        KC_UP,
        KC_RIGHT, KC_DOWN, KC_LEFT
    ),      
};

// -------------------------------------------------------------------------- //
// Combo Keys
// -------------------------------------------------------------------------- //

const uint16_t PROGMEM backlight_combo[] = {KC_UP, KC_DOWN, COMBO_END};

const uint16_t PROGMEM home_display_off_combo[] = {TD(TD_LAYER_CYCLE), VSCODE_OPEN, COMBO_END};
const uint16_t PROGMEM prog_display_off_combo[] = {TD(TD_LAYER_CYCLE), DOXYGEN_COMMENT, COMBO_END};
const uint16_t PROGMEM nvim_display_off_combo[] = {TD(TD_LAYER_CYCLE), CLOSE_NVIM_BUFFERS, COMBO_END};
const uint16_t PROGMEM markdown_display_off_combo[] = {TD(TD_LAYER_CYCLE), LATEX_BLOCK, COMBO_END};
const uint16_t PROGMEM media_display_off_combo[] = {TD(TD_LAYER_CYCLE), KC_MEDIA_PLAY_PAUSE, COMBO_END};
const uint16_t PROGMEM network_display_off_combo[] = {TD(TD_LAYER_CYCLE), REQUEST_RETEST_KEY, COMBO_END};
const uint16_t PROGMEM pomodoro_display_off_combo[] = {TD(TD_LAYER_CYCLE), TIMER_PAUSE, COMBO_END};


const uint16_t PROGMEM base_arrows_combo[] = {EMAIL, LOCK_COMPUTER, COMBO_END};
const uint16_t PROGMEM prog_arrows_combo[] = {TODO_COMMENT, COMMENT_SEPARATOR, COMBO_END};
const uint16_t PROGMEM nvim_arrows_combo[] = {OPEN_LUA_INIT, NVIM_FIND_AND_REPLACE, COMBO_END};
const uint16_t PROGMEM markdown_arrows_combo[] = {LATEX_BLOCK_INLINE, CODE_BLOCK, COMBO_END};
const uint16_t PROGMEM normal_arrows_combo[] = {KC_LEFT, KC_RIGHT, COMBO_END};
const uint16_t PROGMEM media_arrows_combo[] = {KC_MEDIA_NEXT_TRACK, KC_MEDIA_PREV_TRACK, COMBO_END};
const uint16_t PROGMEM network_arrows_combo[] = {KC_PGUP, KC_PGDN, COMBO_END};
const uint16_t PROGMEM pomodoro_arrows_combo[] = {TIMER_RESET, TIMER_RESTART, COMBO_END};


combo_t key_combos[] = {
    COMBO(backlight_combo, BL_STEP),

    COMBO(home_display_off_combo, DISPLAY_TOGGLE),
    COMBO(prog_display_off_combo, DISPLAY_TOGGLE),
    COMBO(nvim_display_off_combo, DISPLAY_TOGGLE),
    COMBO(markdown_display_off_combo, DISPLAY_TOGGLE),
    COMBO(media_display_off_combo, DISPLAY_TOGGLE),
    COMBO(network_display_off_combo, DISPLAY_TOGGLE),
    COMBO(pomodoro_display_off_combo, DISPLAY_TOGGLE),
    
    COMBO(base_arrows_combo, ARROW_TOGGLE),
    COMBO(prog_arrows_combo, ARROW_TOGGLE),
    COMBO(nvim_arrows_combo, ARROW_TOGGLE),
    COMBO(markdown_arrows_combo, ARROW_TOGGLE),
    COMBO(normal_arrows_combo, ARROW_TOGGLE),
    COMBO(media_arrows_combo, ARROW_TOGGLE),
    COMBO(network_arrows_combo, ARROW_TOGGLE),
    COMBO(pomodoro_arrows_combo, ARROW_TOGGLE),
};
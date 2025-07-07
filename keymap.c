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

#define NUM_LAYERS_TO_CYCLE 6
#define HID_BUFFER_SIZE 33
#define NUM_SCREEN_LINES 6

// Globals for Raw HID communication
char received_data[HID_BUFFER_SIZE] = "--";
char received_pc_stats[HID_BUFFER_SIZE] = "--";
char received_network_stats[HID_BUFFER_SIZE] = "--";
char received_song_info[HID_BUFFER_SIZE] = "--";

// volatile bool has_new_data = false;
static uint32_t pc_status_timer = 0;
bool received_first_communication = false; // only build queue after we connect

enum layer_names {
    _BASE,
    _PROGRAMING,
    _GIT,
    _MARKDOWN,
    _NETWORK,
    _MEDIA,
    _ARROWS,
};

int curr_layer = _BASE;
int return_layer = _BASE; // for arrow toggle

enum custom_keycodes {
    LOCK_COMPUTER = SAFE_RANGE,
    VSCODE_OPEN,
    EMAIL,
    COMMENT_SEPARATOR,
    DOXYGEN_COMMENT,
    TODO_COMMENT,
    GIT_COMMIT_ALL,
    GIT_COMMIT_TRACKED,
    GIT_STATUS,
    GIT_LOG,
    REQUEST_RETEST_KEY,
    ARROW_TOGGLE,
    SNIPPING_TOOL,
    CODE_BLOCK,
    LATEX_BLOCK,
    LATEX_BLOCK_INLINE,
};

// -------------------------------------------------------------------------- //
// Raw HID Declarations
// -------------------------------------------------------------------------- //

enum PC_req_types {
    PC_PERFORMANCE = 1,
    NETWORK_TEST = 2,
    CURRENT_SONG = 3,
    REQUEST_RETEST = 4,
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
void handleGitCommit(keyrecord_t *record, bool commitTrackedOnly);
void handleCommandRun(keyrecord_t *record, char *command_str);
void handleCommentSep(keyrecord_t *record);
void handleDoxygenComment(keyrecord_t *record);
void handleArrowToggle(keyrecord_t *record);
void copy_buffer(uint8_t *src_buf, char *dest_buf);
void categorise_received_data(void);
void write_pc_status_oled(void);
void write_network_oled(void);
void write_song_info_oled(void);

void initQueue(queue_t *q) {
    q->size = 0;
}

int isFull(queue_t *q) {
    return q->size == MAX_QUEUE_SIZE;
}

int isEmpty(queue_t *q) {
    return q->size == 0;
}

// Push onto the stack (top is at size - 1)
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

// the request queue (initially empty)
queue_t req_queue;

// -------------------------------------------------------------------------- //
// Helper Functions
// -------------------------------------------------------------------------- //

void cycleLayers(bool forward) {
    if (forward) {
        curr_layer = (curr_layer + 1) % NUM_LAYERS_TO_CYCLE;
        layer_move(curr_layer);
    } else {
        curr_layer = (curr_layer - 1 + NUM_LAYERS_TO_CYCLE) % NUM_LAYERS_TO_CYCLE;
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
        send_string(__DATE__);
        send_string(" ");
        send_string(__TIME__);
        send_string("): ");
    }
}

void handleCommentSep(keyrecord_t *record) {
    if (record->event.pressed) {
        send_string("// -------------------------------------------------------------------------- //\n");
        send_string("// SECTION_TITLE //\n");
        send_string("// -------------------------------------------------------------------------- //\n");
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

void handleArrowToggle(keyrecord_t *record) {
    if (record -> event.pressed) {
        if (curr_layer != _ARROWS) {
            return_layer = curr_layer;
            curr_layer = _ARROWS;
        } else {
            curr_layer = return_layer;
        }
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
    }
}

void write_pc_status_oled(void) {
    static char pc_status_str[100] = "RAM:-- CPU:--";

    char ram_buf[16] = "--";
    char cpu_buf[16] = "--";

    // Parse the new data
    sscanf(received_pc_stats, "%15[^|]|%15s", ram_buf, cpu_buf);

    // Format the display string and save it for future display
    snprintf(pc_status_str, sizeof(pc_status_str), "RAM:%s CPU:%s", ram_buf, cpu_buf);

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
    static char song_display[200] = "No song playing";
    
    // Check if we have received song data
    if (strcmp(received_song_info, "--") != 0) {
        char song_name[32] = "--";
        char artist_name[32] = "--";
        
        // Parse the received data (song|artist format)
        sscanf(received_song_info, "%31[^|]|%31[^\n]", song_name, artist_name);
        
        if (strcmp(song_name, "--") != 0 && strcmp(artist_name, "--") != 0) {
            snprintf(song_display, sizeof(song_display), 
                    "%s\n%s", song_name, artist_name);
        } else {
            strcpy(song_display, "No song playing");
            oled_write_ln(song_display, false);
            oled_write_ln("", false);
            oled_write_ln("", false);
            return;
        }
    }
    
    oled_write_ln(song_display, false);

}

void oled_full_clear(void) {
    oled_set_cursor(0, 0);
    for (int i = 0; i < NUM_SCREEN_LINES; i++) {
        oled_write_ln("", false);
    }
    oled_set_cursor(0, 0);
}

// -------------------------------------------------------------------------- //
// QMK Override Functions
// -------------------------------------------------------------------------- //

void keyboard_post_init_user(void) {
    initQueue(&req_queue);

    backlight_disable();
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv(HSV_RED);
}


#define ENCODER_LAYER_SKIP 2
bool encoder_update_user(uint8_t index, bool clockwise) {
    static uint8_t encoder_tick = 0;

    encoder_tick++;
    if (encoder_tick % ENCODER_LAYER_SKIP != 0) {
        return false;
    }

    if (!clockwise) {
        cycleLayers(true);
    } else {
        cycleLayers(false);
    }

    return false;
}

void matrix_scan_user(void) {
    // Check if 2 seconds have passed since the last request
    if (timer_elapsed32(pc_status_timer) > 2000 && received_first_communication) {
        // Reset the timer
        pc_status_timer = timer_read32();

        // queue the appropriate request based on current layer
        if (curr_layer <= _MARKDOWN) {
            enqueue(&req_queue, PC_PERFORMANCE);
        } else if (curr_layer == _NETWORK) {
            // Always request network status when on network layer
            enqueue(&req_queue, NETWORK_TEST);
        } else if (curr_layer == _MEDIA) {
            // Always request song info when on media layer
            enqueue(&req_queue, CURRENT_SONG);
        }
    }
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    print("Raw HID data received\n");

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

    print("Next raw HID request sent\n");
}

// gungeon guy, looks good just a little big
const unsigned char epd_bitmap_gungeon [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x1f, 0x07, 0x07, 0x01, 0x01, 0x81, 0x81, 0x80, 0xe0, 0xe0, 
	0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x07, 0x07, 0x1f, 0x1f, 0x7f, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0x3f, 0x3f, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x3f, 0x3f, 0x0f, 0x0f, 
	0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0xc0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xfc, 0xfc, 
	0xfc, 0xfc, 0xfc, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 
	0x0f, 0x0f, 0x3f, 0x3f, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 
	0xe3, 0xe3, 0xe3, 0xe3, 0xe3, 0xf0, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xfe, 0xfe, 0xff, 0xff, 
	0xfc, 0xfc, 0x03, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xfc, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x3f, 0x3f, 0xff, 0xff, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xff, 0xff, 0xcf, 0xcf, 0xff, 0xff, 
	0xf0, 0xf0, 0x0f, 0x0f, 0x3f, 0x3f, 0x3f, 0x03, 0x03, 0x3c, 0x3c, 0x3c, 0x3c, 0x30, 0x30, 0x00, 
	0x00, 0xff, 0xff, 0x03, 0x03, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
	0xf0, 0xf0, 0xfc, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x03, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x3f, 0x3f, 0xcf, 0xcf, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 0xf3, 0xfc, 0xfc, 0xfc, 0xfc, 
	0xfc, 0xfc, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfc, 0xfc, 0x0f, 0x0f, 0xc3, 0xc3, 0xc0, 
	0xc0, 0xf0, 0xf0, 0xfc, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xcf, 0xcf, 0xc3, 0xc3, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xcf, 0xcf, 0xcf, 0xcf, 0xf3, 0xf3, 0xf3, 0xf3, 
	0xf0, 0xf0, 0xc3, 0xc3, 0xc3, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};





// This function runs to update the OLED display
bool oled_task_user(void) {
    static int last_layer = -1;
    
    if (last_layer != curr_layer) {
        oled_clear();
        last_layer = curr_layer;
    }

    switch (curr_layer) {
        case _BASE:
            rgblight_sethsv(HSV_RED);
            oled_write_ln("Home Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< lock computer", false);
            oled_write_ln("v open vscode", false);
            oled_write_ln("> email", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _PROGRAMING:
            rgblight_sethsv(HSV_BLUE);
            oled_write_ln("Programming Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< comment separator", false);
            oled_write_ln("v doxygen comment", false);
            oled_write_ln("> todo comment", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _GIT:
            rgblight_sethsv(HSV_WHITE);
            oled_write_ln("Git Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< commit all", false);
            oled_write_ln("v commit tracked", false);
            oled_write_ln("> git status", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _MARKDOWN:
            rgblight_sethsv(HSV_PURPLE);
            oled_write_ln("Markdown Layer", false);
            oled_write_ln("", false);
            oled_write_ln("< code block", false);
            oled_write_ln("v latex block", false);
            oled_write_ln("> latex inline", false);
            oled_write_ln("", false);
            write_pc_status_oled();
            break;
        case _NETWORK:
            rgblight_sethsv(HSV_YELLOW);
            oled_write_ln("Internet Speed", false);
            oled_write_ln("", false);
            oled_write_ln("'v' to retest", false);
            oled_write_ln("", false);
            write_network_oled();
            break;
        case _MEDIA:
            rgblight_sethsv(HSV_GREEN);
            oled_write_ln("Media Player", false);
            oled_write_ln("", false);
            oled_write_ln("", false);
            write_song_info_oled();
            break;
        case _ARROWS:
            rgblight_sethsv(HSV_CYAN);
            // oled_write_ln("Arrow Layer", false);
            oled_write_raw_P((const char *)epd_bitmap_gungeon, sizeof(epd_bitmap_gungeon));
            // oled_write_ln("", false);
            // oled_write_ln("", false);
            // oled_write_ln("", false);
            // oled_write_ln("", false);
            // oled_write_ln("", false);
            // oled_write_ln("", false);
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
            handleCommandRun(record, "git status");
            return false;
        }
        case GIT_LOG: {
            handleCommandRun(record, "git log");
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
    }

    return true;
}

// -------------------------------------------------------------------------- //
// Macropad Layout
// -------------------------------------------------------------------------- //

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        ARROW_TOGGLE,
        SNIPPING_TOOL,
        EMAIL, VSCODE_OPEN, LOCK_COMPUTER
    ),
    [_PROGRAMING] = LAYOUT(
        ARROW_TOGGLE,
        SNIPPING_TOOL,
        TODO_COMMENT, DOXYGEN_COMMENT, COMMENT_SEPARATOR
    ),
    [_GIT] = LAYOUT(
        ARROW_TOGGLE,
        GIT_LOG,
        GIT_STATUS, GIT_COMMIT_TRACKED, GIT_COMMIT_ALL
    ),
    [_MARKDOWN] = LAYOUT(
        ARROW_TOGGLE,
        SNIPPING_TOOL,
        LATEX_BLOCK_INLINE, LATEX_BLOCK, CODE_BLOCK
    ),
    [_NETWORK] = LAYOUT(
        ARROW_TOGGLE,
        SNIPPING_TOOL,
        KC_NO, REQUEST_RETEST_KEY, KC_NO
    ),
    [_MEDIA] = LAYOUT(
        ARROW_TOGGLE,
        SNIPPING_TOOL,
        KC_MEDIA_NEXT_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_PREV_TRACK
    ),
    [_ARROWS] = LAYOUT(
        ARROW_TOGGLE,
        KC_UP,
        KC_RIGHT, KC_DOWN, KC_LEFT
    ),    
};

const uint16_t PROGMEM backlight_combo[] = {KC_UP, KC_DOWN, COMBO_END};
combo_t key_combos[] = {
    COMBO(backlight_combo, BL_STEP)
};


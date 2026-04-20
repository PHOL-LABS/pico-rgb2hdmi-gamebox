#ifndef _COMMON_CONFIGS_H
#define _COMMON_CONFIGS_H

//Keyboard
#ifdef _KEYBOARD_H
    #define KEYBOARD_N_PINS          3
    #define KEYBOARD_PIN_UP          0
    #define KEYBOARD_PIN_DOWN        1
    #define KEYBOARD_PIN_ACTION      2
    #define KEYBOARD_REFRESH_RATE_MS 25
    #define KEYBOARD_REPEAT_RATE_MS  2000
#endif

#ifdef _MENU_H
    #define MENU_VIDEO_OVERLAY_WIDTH   -200
    #define MENU_VIDEO_OVERLAY_HEIGHT  -104
#endif

//SYNC edge triggered Pins
#ifdef _RGB_SCANNER_H
    #define RGB_SCAN_VSYNC_PIN  27
    #define RGB_SCAN_HSYNC_PIN  27
#endif

//Digital RGB input pins (3-3-2)
#define RGB_INPUT_RED_0_PIN   11
#define RGB_INPUT_RED_1_PIN   12
#define RGB_INPUT_RED_2_PIN   13
#define RGB_INPUT_GREEN_0_PIN 14
#define RGB_INPUT_GREEN_1_PIN 15
#define RGB_INPUT_GREEN_2_PIN 16
#define RGB_INPUT_BLUE_0_PIN  17
#define RGB_INPUT_BLUE_1_PIN  18
// Pins 19-22, 26, 28 are not connected.

//VIDEO Timing
#define V_FRONT_PORCH            42
#define V_BACK_PORCH             54
#define REFRESH_RATE             50
#define HSYNC_FRONT_PORCH        100
#define HSYNC_BACK_PORCH         50
#define DEFAULT_SYMBOLS_PER_WORD 1

//Digital input defaults
#define DIGITAL_GAIN_DEFAULT       0
#define DIGITAL_OFFSET_DEFAULT     0
#define DIGITAL_NEG_OFFSET_DEFAULT 0

//DVI Specific configs
#ifdef _DVI_SERIALISER_H
    //DVI Config
    #define DVI_DEFAULT_SERIAL_CONFIG    picodvi_dvi_cfg
    #define DVI_DEFAULT_PIO_INST         pio0

    static const struct dvi_serialiser_cfg picodvi_dvi_cfg = {
        .pio = pio0,
        .sm_tmds = {0, 1, 2},
        .pins_tmds = {5, 7, 9},
        .pins_clk = 3,
        .invert_diffpairs = true,
        .symbols_per_word = false
    };
#endif

// Colors 16bits            0brrrrrggggggbbbbb
#define color_16_black      0b0000000000000000
#define color_16_dark_gray  0b0001100011100011
#define color_16_mid_gray   0b1010010100010000
#define color_16_light_gray 0b1100011000011000
#define color_16_white      0b1111111111111111
#define color_16_red        0b1111100000000000
#define color_16_green      0b0000011111100000
#define color_16_blue       0b0000000000011111
// Colors 8 bits            0brrrgggbb
#define color_8_black       0b00000000
#define color_8_dark_gray   0b01001001
#define color_8_mid_gray    0b10110110
#define color_8_light_gray  0b11011011
#define color_8_white       0b11111111
#define color_8_red         0b11100000
#define color_8_green       0b00011100
#define color_8_blue        0b00000011

#define DEFAULT_MENU_COLORS_16 { color_16_dark_gray, color_16_light_gray, color_16_white, color_16_black, color_16_mid_gray, color_16_green}
#define DEFAULT_MENU_COLORS_8  { color_8_dark_gray, color_8_light_gray, color_8_white, color_8_black, color_8_mid_gray, color_8_green}


#if DEFAULT_SYMBOLS_PER_WORD == 2
    #define DEFAULT_MENU_COLORS DEFAULT_MENU_COLORS_16
#else
    #define DEFAULT_MENU_COLORS DEFAULT_MENU_COLORS_8
#endif

// Global settings 
#ifdef _SETTINGS_H
    // Factory Settings
    #define GET_FACTORY_SETTINGS() { \
            .security_key = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13 },\
            .menu_colors = DEFAULT_MENU_COLORS, \
            .flags.auto_shut_down = 1, \
            .flags.default_display = 0, \
            .flags.scan_line = 0, \
            .flags.symbols_per_word = DEFAULT_SYMBOLS_PER_WORD == 2, \
            .displays = {{ \
                    .gain = { .red = DIGITAL_GAIN_DEFAULT, .green = DIGITAL_GAIN_DEFAULT, .blue = DIGITAL_GAIN_DEFAULT}, \
                    .offset = { .red = DIGITAL_OFFSET_DEFAULT, .green = DIGITAL_OFFSET_DEFAULT,   .blue = DIGITAL_OFFSET_DEFAULT, .negative = DIGITAL_NEG_OFFSET_DEFAULT}, \
                    .v_front_porch = V_FRONT_PORCH, .v_back_porch = V_BACK_PORCH, \
                    .h_front_porch = HSYNC_FRONT_PORCH, .h_back_porch = HSYNC_BACK_PORCH, \
                    .refresh_rate = REFRESH_RATE, \
                    .fine_tune = 0 \
                }, { \
                    .gain = { .red = DIGITAL_GAIN_DEFAULT, .green = DIGITAL_GAIN_DEFAULT, .blue = DIGITAL_GAIN_DEFAULT}, \
                    .offset = { .red = DIGITAL_OFFSET_DEFAULT, .green = DIGITAL_OFFSET_DEFAULT,   .blue = DIGITAL_OFFSET_DEFAULT, .negative = DIGITAL_NEG_OFFSET_DEFAULT }, \
                    .v_front_porch = V_FRONT_PORCH, .v_back_porch = V_BACK_PORCH, \
                    .h_front_porch = HSYNC_FRONT_PORCH, .h_back_porch = HSYNC_BACK_PORCH, \
                    .refresh_rate = REFRESH_RATE, \
                    .fine_tune = 0 \
                }, { \
                    .gain = { .red = DIGITAL_GAIN_DEFAULT, .green = DIGITAL_GAIN_DEFAULT, .blue = DIGITAL_GAIN_DEFAULT}, \
                    .offset = { .red = DIGITAL_OFFSET_DEFAULT, .green = DIGITAL_OFFSET_DEFAULT,   .blue = DIGITAL_OFFSET_DEFAULT, .negative = DIGITAL_NEG_OFFSET_DEFAULT}, \
                    .v_front_porch = V_FRONT_PORCH, .v_back_porch = V_BACK_PORCH, \
                    .h_front_porch = HSYNC_FRONT_PORCH, .h_back_porch = HSYNC_BACK_PORCH, \
                    .refresh_rate = REFRESH_RATE, \
                    .fine_tune = 0 \
                }, { \
                    .gain = { .red = DIGITAL_GAIN_DEFAULT, .green = DIGITAL_GAIN_DEFAULT, .blue = DIGITAL_GAIN_DEFAULT}, \
                    .offset = { .red = DIGITAL_OFFSET_DEFAULT, .green = DIGITAL_OFFSET_DEFAULT, .blue = DIGITAL_OFFSET_DEFAULT, .negative = DIGITAL_NEG_OFFSET_DEFAULT}, \
                    .v_front_porch = V_FRONT_PORCH, .v_back_porch = V_BACK_PORCH, \
                    .h_front_porch = HSYNC_FRONT_PORCH, .h_back_porch = HSYNC_BACK_PORCH, \
                    .refresh_rate = REFRESH_RATE, \
                    .fine_tune = 0 \
                } \
            }, \
            .eof_canary = 0 \
        }
    #endif
#endif

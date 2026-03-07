//System defined includes
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"

//Library related includes
#include "dvi.h"
#include "dvi_serialiser.h"
#include "keyboard.h"
#include "graphics.h"

//System configuration includes
#include "version.h"
#include "common_configs.h"

// System config definitions
#define FRAME_HEIGHT 240
#define FRAME_WIDTH_8_BITS  640
#define FRAME_WIDTH_16_BITS 320

uint8_t genbuf[FRAME_HEIGHT][FRAME_WIDTH_8_BITS];
uint8_t  *framebuf_8  = GET_RGB8_BUFFER(genbuf);
uint16_t *framebuf_16 = GET_RGB16_BUFFER(genbuf);

#define REFRESH_RATE 50
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

bool symbols_per_word = 0;
struct dvi_inst dvi0;
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
bool blink = true;
static uint hdmi_scanline = 2;

static graphic_ctx_t graphic_ctx = {
    .height       = FRAME_HEIGHT,
    .video_buffer = genbuf,
    .parent       = NULL
};

const uint color_8_list[]  = {
    color_8_red, color_8_green, color_8_blue,
    color_8_white, color_8_mid_gray, color_8_black
};

const uint color_16_list[] = {
    color_16_red, color_16_green, color_16_blue,
    color_16_white, color_16_mid_gray, color_16_black
};

static void dbg_wait_for_usb(uint32_t timeout_ms) {
    uint32_t elapsed = 0;

    printf("\n");
    printf("========================================\n");
    printf("Booting, waiting for USB serial...\n");
    printf("Timeout: %lu ms\n", (unsigned long)timeout_ms);
    printf("========================================\n");
    fflush(stdout);

    while (!stdio_usb_connected() && elapsed < timeout_ms) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
        elapsed += 200;
    }

    printf("USB serial %s after %lu ms\n",
           stdio_usb_connected() ? "connected" : "not connected",
           (unsigned long)elapsed);
    fflush(stdout);
}

void __not_in_flash_func(core1_main)(void) {
    printf("[CORE1] Entered core1_main()\n");
    fflush(stdout);

    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    printf("[CORE1] IRQs registered\n");
    fflush(stdout);

    dvi_start(&dvi0);
    printf("[CORE1] DVI started, symbols_per_word=%d\n", dvi0.ser_cfg.symbols_per_word);
    fflush(stdout);

    if (symbols_per_word) {
        printf("[CORE1] Running 16bpp scanbuf loop\n");
        fflush(stdout);
        dvi_scanbuf_main_16bpp(&dvi0);
    } else {
        printf("[CORE1] Running 8bpp scanbuf loop\n");
        fflush(stdout);
        dvi_scanbuf_main_8bpp(&dvi0);
    }

    __builtin_unreachable();
}

static inline void core1_scanline_callback(void) {
    void *bufptr = NULL;

    while (queue_try_remove_u32(&dvi0.q_colour_free, &bufptr)) {
    }

    if (dvi0.ser_cfg.symbols_per_word) {
        bufptr = &framebuf_16[graphic_ctx.width * hdmi_scanline];
    } else {
        bufptr = &framebuf_8[graphic_ctx.width * hdmi_scanline];
    }

    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);

    if (++hdmi_scanline >= FRAME_HEIGHT) {
        hdmi_scanline = 0;
    }
}

int main(void) {
    uint color_red       = symbols_per_word ? color_16_red      : color_8_red;
    uint color_blue      = symbols_per_word ? color_16_blue     : color_8_blue;
    uint color_mid_gray  = symbols_per_word ? color_16_mid_gray : color_8_mid_gray;
    uint color_white     = symbols_per_word ? color_16_white    : color_8_white;
    const uint *color_list = symbols_per_word ? color_16_list : color_8_list;

    graphic_ctx.bppx  = symbols_per_word ? rgb_16_565 : rgb_8_332;
    graphic_ctx.width = symbols_per_word ? FRAME_WIDTH_16_BITS : FRAME_WIDTH_8_BITS;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    stdio_init_all();

    // Make sure all printf output appears immediately
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    // Give Windows time to enumerate the COM port
    sleep_ms(3000);

    // Wait up to 10 seconds for terminal connection
    dbg_wait_for_usb(10000);

    printf("[MAIN] ========================================\n");
    printf("[MAIN] %s version %s starting\n", PROJECT_NAME, PROJECT_VER);
    printf("[MAIN] symbols_per_word=%d\n", symbols_per_word);
    printf("[MAIN] graphic_ctx.width=%u height=%u\n", graphic_ctx.width, graphic_ctx.height);
    printf("[MAIN] DVI bit clock target = %u kHz\n", DVI_TIMING.bit_clk_khz);
    printf("[MAIN] Setting core voltage\n");

    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);

    printf("[MAIN] Setting system clock\n");
    bool clk_ok = set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);
    printf("[MAIN] set_sys_clock_khz -> %s\n", clk_ok ? "OK" : "FAIL");
    printf("[MAIN] Current sys clock = %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));

    printf("[MAIN] Configuring DVI instance\n");
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi0.scanline_callback = core1_scanline_callback;
    dvi0.ser_cfg.symbols_per_word = symbols_per_word;

    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    printf("[MAIN] dvi_init done\n");

    printf("[MAIN] Priming initial scanlines\n");
    void *bufptr = NULL;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);
    bufptr = (uint8_t *)bufptr + graphic_ctx.width;
    queue_add_blocking_u32(&dvi0.q_colour_valid, &bufptr);

    printf("[MAIN] Launching core1\n");
    multicore_launch_core1(core1_main);

    printf("[MAIN] Start rendering test pattern\n");

    uint x, y, a;
    uint sizex = graphic_ctx.width / 2;
    uint sizey = graphic_ctx.height / 2;

    printf("[MAIN] Drawing boxes\n");
    for (int i = 0; i < 6; i++) {
        int valx = (graphic_ctx.width * i) / 30;
        int valy = (graphic_ctx.height * i) / 15;
        printf("[MAIN] Box %d: x=%d y=%d w=%d h=%d color=%u\n",
               i,
               valx,
               valy,
               graphic_ctx.width - (2 * valx),
               graphic_ctx.height - (2 * valy),
               color_list[i]);
        fill_rect(&graphic_ctx,
                  valx,
                  valy,
                  graphic_ctx.width - (2 * valx),
                  graphic_ctx.height - (2 * valy),
                  color_list[i]);
    }

    printf("[MAIN] Drawing circles\n");
    for (a = 0; a < 16; a++) {
        x = sizex + sizex / 2 * sin(2 * M_PI * a / 16);
        y = sizey + sizey / 2 * cos(2 * M_PI * a / 16);

        printf("[MAIN] Circle %u: x=%u y=%u\n", a, x, y);

        draw_circle(&graphic_ctx, x, y, 16, color_red);
        draw_circle(&graphic_ctx, x, y, 8, color_red);
        draw_flood(&graphic_ctx, x + 10, y, color_blue, color_red, true);
    }

    printf("[MAIN] Drawing diagonal lines\n");
    draw_line(&graphic_ctx, 0, 0, graphic_ctx.width - 1, graphic_ctx.height - 1, color_blue);
    draw_line(&graphic_ctx, graphic_ctx.width - 1, 0, 0, graphic_ctx.height - 1, color_blue);

    printf("[MAIN] Drawing outer rectangle\n");
    draw_rect(&graphic_ctx,
              graphic_ctx.width / 16,
              graphic_ctx.height / 12,
              graphic_ctx.width - graphic_ctx.width / 8,
              graphic_ctx.height - graphic_ctx.height / 8,
              color_mid_gray);

    printf("[MAIN] Drawing text\n");
    draw_textf(&graphic_ctx,
               graphic_ctx.width / 6,
               (graphic_ctx.height * 63) / 100,
               color_mid_gray,
               color_white,
               false,
               "This is a test of RGB%s %d",
               symbols_per_word ? "565" : "332",
               2023);

    printf("[MAIN] Render finished, entering heartbeat loop\n");

    while (1) {
        gpio_put(LED_PIN, 1);
        printf("[MAIN] Alive. hdmi_scanline=%u\n", hdmi_scanline);
        sleep_ms(500);

        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }

    __builtin_unreachable();
}

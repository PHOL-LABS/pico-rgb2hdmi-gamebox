#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pll.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/ssi.h"

#include "tmds_encode.h"
#include "dvi.h"
#include "dvi_serialiser.h"

#include "version.h"
#include "common_configs.h"

// Output timing is still VGA 640x480p60
#define OUT_WIDTH   640
#define OUT_HEIGHT  480

// Source buffer is half-height
#define SRC_WIDTH   640
#define SRC_HEIGHT  240

#define VREG_VSEL   VREG_VOLTAGE_1_20
#define DVI_TIMING  dvi_timing_640x480p_60hz

struct dvi_inst dvi0;
struct semaphore dvi_start_sem;

// RGB565 source image, 240 lines only
static uint16_t img_buf[SRC_HEIGHT][SRC_WIDTH];

static volatile uint32_t lines_sent = 0;
static volatile uint32_t frames_sent = 0;

static void wait_for_usb(uint32_t timeout_ms) {
    uint32_t elapsed = 0;

    printf("\n");
    printf("========================================\n");
    printf("Booting, waiting for USB serial...\n");
    printf("Timeout: %lu ms\n", (unsigned long)timeout_ms);
    printf("========================================\n");
    fflush(stdout);

    while (!stdio_usb_connected() && elapsed < timeout_ms) {
        sleep_ms(100);
        elapsed += 100;
    }

    printf("USB serial %s after %lu ms\n",
           stdio_usb_connected() ? "connected" : "not connected",
           (unsigned long)elapsed);
    fflush(stdout);
}

static inline void prepare_scanline(const uint16_t *colourbuf, uint32_t *tmdsbuf) {
    const uint pixwidth = OUT_WIDTH;

    // colourbuf is RGB565
    tmds_encode_data_channel_fullres_16bpp(
        (const uint32_t *)colourbuf,
        tmdsbuf + 0 * pixwidth,
        pixwidth,
        4, 0
    );
    tmds_encode_data_channel_fullres_16bpp(
        (const uint32_t *)colourbuf,
        tmdsbuf + 1 * pixwidth,
        pixwidth,
        10, 5
    );
    tmds_encode_data_channel_fullres_16bpp(
        (const uint32_t *)colourbuf,
        tmdsbuf + 2 * pixwidth,
        pixwidth,
        15, 11
    );
}

// Core1 handles DMA IRQs and encodes scanlines handed over through FIFO
void __not_in_flash("main") core1_main(void) {
    printf("[CORE1] entered\n");
    fflush(stdout);

    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    printf("[CORE1] IRQs registered\n");
    fflush(stdout);

    sem_acquire_blocking(&dvi_start_sem);
    dvi_start(&dvi0);

    printf("[CORE1] DVI started\n");
    fflush(stdout);

    while (1) {
        const uint16_t *colourbuf = (const uint16_t *)multicore_fifo_pop_blocking();
        uint32_t *tmdsbuf = (uint32_t *)multicore_fifo_pop_blocking();

        prepare_scanline(colourbuf, tmdsbuf);

        multicore_fifo_push_blocking(0);
    }

    __builtin_unreachable();
}

static void build_test_pattern(void) {
    printf("[MAIN] building %ux%u source pattern\n", SRC_WIDTH, SRC_HEIGHT);

    for (int y = 0; y < SRC_HEIGHT; y++) {
        for (int x = 0; x < SRC_WIDTH; x++) {
            int red   = x * 32 / SRC_WIDTH;
            int green = y * 64 / SRC_HEIGHT;
            int blue  = 31 - (x * 32) / SRC_WIDTH;

            // white grid over gradient
            img_buf[y][x] = ((x % 32) == 0 || (y % 16) == 0)
                ? 0xFFFF
                : (uint16_t)((blue << 11) | (green << 5) | red);
        }
    }

    // A few visible horizontal color bars
    for (int y = 20; y < 40; y++) {
        for (int x = 40; x < 200; x++) img_buf[y][x] = 0xF800; // red
        for (int x = 220; x < 380; x++) img_buf[y][x] = 0x07E0; // green
        for (int x = 400; x < 560; x++) img_buf[y][x] = 0x001F; // blue
    }

    printf("[MAIN] pattern ready\n");
    fflush(stdout);
}

int __not_in_flash("main") main(void) {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    sleep_ms(3000);
    wait_for_usb(10000);

    printf("[MAIN] ========================================\n");
    printf("[MAIN] %s version %s starting\n", PROJECT_NAME, PROJECT_VER);
#ifdef DVI_VERTICAL_REPEAT
    printf("[MAIN] DVI_VERTICAL_REPEAT=%d\n", DVI_VERTICAL_REPEAT);
#else
    printf("[MAIN] DVI_VERTICAL_REPEAT is NOT defined\n");
#endif
    printf("[MAIN] source = %ux%u\n", SRC_WIDTH, SRC_HEIGHT);
    printf("[MAIN] output = %ux%u\n", OUT_WIDTH, OUT_HEIGHT);

    printf("[MAIN] setting core voltage\n");
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);

    printf("[MAIN] setting system clock to %u kHz\n", DVI_TIMING.bit_clk_khz);
    bool clk_ok = set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);
    printf("[MAIN] set_sys_clock_khz -> %s\n", clk_ok ? "OK" : "FAIL");
    printf("[MAIN] current sys clock = %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));

    printf("[MAIN] configuring DVI\n");
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi0.ser_cfg.symbols_per_word = 0;

    build_test_pattern();

    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    printf("[MAIN] dvi_init done\n");

    sem_init(&dvi_start_sem, 0, 1);

    // Match known-good examples
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);

    printf("[MAIN] launching core1\n");
    multicore_launch_core1(core1_main);

    sem_release(&dvi_start_sem);
    printf("[MAIN] DVI started\n");

    absolute_time_t last_stat = get_absolute_time();

    while (1) {
        for (int out_y = 0; out_y < OUT_HEIGHT; out_y += 2) {
            uint32_t *their_tmds_buf = NULL;
            uint32_t *our_tmds_buf = NULL;

            // Vertical repeat = 2:
            // each pair of output lines uses the same source line
            int src_y = out_y / 2;

            const uint16_t *src_line = img_buf[src_y];

            queue_remove_blocking_u32(&dvi0.q_tmds_free, &their_tmds_buf);
            multicore_fifo_push_blocking((uintptr_t)src_line);
            multicore_fifo_push_blocking((uintptr_t)their_tmds_buf);

            queue_remove_blocking_u32(&dvi0.q_tmds_free, &our_tmds_buf);
            prepare_scanline(src_line, our_tmds_buf);

            multicore_fifo_pop_blocking();

            queue_add_blocking_u32(&dvi0.q_tmds_valid, &their_tmds_buf);
            queue_add_blocking_u32(&dvi0.q_tmds_valid, &our_tmds_buf);

            lines_sent += 2;
        }

        frames_sent++;

        if (absolute_time_diff_us(last_stat, get_absolute_time()) > 1000000) {
            printf("[MAIN] frames=%lu lines=%lu\n", (unsigned long)frames_sent, (unsigned long)lines_sent);
            last_stat = get_absolute_time();
        }
    }

    __builtin_unreachable();
}

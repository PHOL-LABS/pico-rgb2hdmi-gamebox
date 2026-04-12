/**
 * Initial tests of the rgbScan api
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "wm8213Afe.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"

// System configuration includes
#include "version.h"
#include "common_configs.h"

void core1_main(void);

uint16_t test_buf[32];

// A simple test the AFE (Analog Front End)
// In case of Gamebox setup we do receive all data digitally as from the frontend so we do not have the IC

static void step_print(const char *msg) {
    printf("[STEP] %s\n", msg);
    sleep_ms(1000);
}

int main(void) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
	sleep_ms(10);
	set_sys_clock_khz(250000, true);

    stdio_init_all();

    // Give time to open serial terminal after reset
    sleep_ms(5000);

    printf("AFE initial test\n");
    printf("%s version - AFE Test starting...\n", PROJECT_NAME, PROJECT_VER);
    sleep_ms(1000);

    step_print("Initializing AFE configuration");
    wm8213_afe_init(&afec_cfg);

    step_print("Updating capture format to rgb_8_332");
    // rgb_8_332 - from Atmega644 Uzebox/Gamebox
    wm8213_afe_capture_update_bppx(rgb_8_332, false);

    step_print("Starting AFE with sampling rate 2000000");
    if (wm8213_afe_start(2000000) > 0) {
        printf("[ERROR] AFE initialize failed\n");
    } else {
        printf("[OK] AFE initialize succeeded\n");
    }
    sleep_ms(1000);

    step_print("Filling test buffer with 0xFFFF");
    for (int cnt = 0; cnt < 32; cnt++) {
        test_buf[cnt] = 0xFFFF;
    }

    step_print("Initializing LED GPIO");
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    step_print("Launching core1");
    multicore_launch_core1(core1_main);

    step_print("Entering main capture loop");

    printf("%s version - AFE Test %s started!\n", PROJECT_NAME, PROJECT_VER);

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
        wm8213_afe_capture_run(1, (uintptr_t)test_buf, 32);
        wm8213_afe_capture_wait();
        printf("RED\tGREEN\tBLUE\n");
        for (int cnt = 0; cnt < 32; cnt++) {
            int b = test_buf[cnt] & 0x1F; 
            int g = (test_buf[cnt]>>5) & 0x3F;
            int r = (test_buf[cnt]>>11) & 0x1F;
            printf("%f\t%f\t%f\n",(float)r*3.3/31, (float)g*3.3/63, (float)b*3.3/31);
        }
        printf("\n");
    }
}

void core1_main() {
    while (true) {
        sleep_ms(1000);
    }
}
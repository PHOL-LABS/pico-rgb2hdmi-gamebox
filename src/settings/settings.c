#include <string.h>
#include "settings.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

const settings_t *flash_settings;
const settings_t *p_factory_settings;

settings_t ram_settings;

int settings_initialize(const settings_t *p_fact_sett) {
    memcpy(&ram_settings, p_factory_settings, sizeof(settings_t));
    return 0;
}

int settings_update() {
    memcpy(&ram_settings, p_factory_settings, sizeof(settings_t));
    return 0;
}

int settings_factory() {
    memcpy(&ram_settings, p_factory_settings, sizeof(settings_t));
    return 0;
}
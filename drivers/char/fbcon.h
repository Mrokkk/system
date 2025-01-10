#pragma once

#include "console_driver.h"

int fbcon_init(console_driver_t* driver, console_config_t* config, size_t* resy, size_t* resx);

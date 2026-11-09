#pragma once
#include "esp_err.h"
#include <stddef.h>
esp_err_t dashboard_init(void);
void dashboard_status(void);
void dashboard_address(char *out,size_t size);
void dashboard_postal(char out[6]);

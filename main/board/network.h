#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t board_network_init(void);
void board_network_command(const char *arg);
const char *board_network_status(void);
uint32_t board_network_last_sync(void);
uint32_t board_network_sync_count(void);
unsigned board_network_disconnect_reason(void);
unsigned board_network_sync_interval(void);

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t data[6];
    bool ok;
    char error[64];
} aurora_reply_t;

uint16_t aurora_crc(const uint8_t *buf, int len);
bool aurora_check_crc(const uint8_t *rx);
double aurora_dsp_value(const uint8_t *hex);

bool aurora_query_batch(aurora_reply_t *out_map, const uint8_t **keys, const uint8_t **cmds, int count,
                        const char *remote_ip, uint16_t remote_port);

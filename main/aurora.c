#include "aurora.h"
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

static int recv_full(int fd, uint8_t *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = recv(fd, buf + got, n - got, 0);
        if (r <= 0) return -1;
        got += r;
    }
    return got;
}

uint16_t aurora_crc(const uint8_t *cmd, int len) {
    int BccLo = 0xFF;
    int BccHi = 0xFF;
    for (int x = 0; x < len; x++) {
        int c = (int)cmd[x] ^ BccLo;
        int tmp = (c << 4) & 0xFF;
        c = tmp ^ c;
        tmp = (c >> 5) & 0xFF;
        BccLo = BccHi;
        BccHi = c ^ tmp;
        tmp = (c << 3) & 0xFF;
        BccLo = BccLo ^ tmp;
        tmp = (c >> 4) & 0xFF;
        BccLo = BccLo ^ tmp;
    }
    int CRC_L = (-(BccLo) - 1) & 0xFF;
    int CRC_H = (-(BccHi) - 1) & 0xFF;
    return (uint16_t)(((CRC_H << 8) & 0xFF00) | (CRC_L & 0xFF));
}

bool aurora_check_crc(const uint8_t *rx) {
    uint8_t rx_crc[2] = { rx[6], rx[7] };
    uint16_t my = aurora_crc(rx, 6);
    uint8_t my_crc[2] = { (uint8_t)(my & 0xFF), (uint8_t)((my >> 8) & 0xFF) };
    return rx_crc[0] == my_crc[0] && rx_crc[1] == my_crc[1];
}

double aurora_dsp_value(const uint8_t *hex) {
    int v = (hex[2] << 24) + (hex[3] << 16) + (hex[4] << 8) + hex[5];
    int x = (v & ((1 << 23) - 1)) + (1 << 23) * ((v >> 31) | 1);
    int exp = ((v >> 23) & 0xFF) - 127;
    return (double)x * pow(2.0, (double)exp - 23.0);
}

bool aurora_query_batch(aurora_reply_t *out_map, const uint8_t **keys, const uint8_t **cmds, int count,
                        const char *remote_ip, uint16_t remote_port) {
    const int max_attempts = 5;
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            continue;
        }
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_port);
        inet_aton(remote_ip, &addr.sin_addr);
        // connect with timeout can be added with non-blocking + select if needed
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            usleep((250 << attempt) * 1000);
            continue;
        }
        bool ok = true;
        for (int i = 0; i < count; i++) {
            uint8_t tx[8];
            memcpy(tx, cmds[i], 6);
            uint16_t c = aurora_crc(cmds[i], 6);
            tx[6] = (uint8_t)(c & 0xFF);
            tx[7] = (uint8_t)((c >> 8) & 0xFF);
            if (send(fd, tx, 8, 0) != 8) {
                ok = false; break;
            }
            uint8_t rx[8];
            if (recv_full(fd, rx, 8) != 8) {
                ok = false; break;
            }
            if (!aurora_check_crc(rx)) { ok = false; break; }
            memcpy(out_map[i].data, rx, 6);
            out_map[i].ok = true;
        }
        close(fd);
        if (ok) return true;
        usleep((250 << attempt) * 1000);
    }
    return false;
}

#pragma once

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctype.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "../config/AppConfig.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"

    // ... inside WifiWsServer ...
// Struct packed to exactly 20 bytes
#pragma pack(push, 1)
struct MatrixTelemetryHeader {
    uint8_t  magic = 0x57;       // Header Magic 'W'
    uint8_t  version = 0x01;     // Format version
    uint8_t  brightness;         // Dynamic Brightness (0-100 or 0-255)
    uint8_t  actual_fps;         // Calculated / Measured FPS
    uint16_t year;               // e.g. 2026
    uint8_t  month;              // 1-12
    uint8_t  day;                // 1-31
    uint8_t  hour;               // 0-23
    uint8_t  minute;             // 0-59
    uint8_t  second;             // 0-59
    uint8_t  effect_id;          // Active effect index
    uint8_t  sub_effect_id;      // Active sub-effect index
    uint8_t  moon_phase;         // 0: New, 1: Crescent, 2: Half, 3: Full
    uint8_t  celestial;          // 0: Night, 1: Sunrise, 2: Day, 3: Sunset
    uint8_t  reserved[5] = {0};  // Padding for aligned 20-byte block
};
#pragma pack(pop)

class WifiWsServer {
private:
    struct tcp_pcb* server_pcb = nullptr;
    static inline struct tcp_pcb* active_client = nullptr;

    // Lightweight Standalone SHA-1
    static void sha1(const uint8_t* data, size_t len, uint8_t hash[20]) {
        uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
        size_t new_len = (((len + 8) / 64) + 1) * 64;
        uint8_t msg[256] = {0};
        memcpy(msg, data, len);
        msg[len] = 0x80;

        uint64_t bits_len = (uint64_t)len * 8;
        for (int i = 0; i < 8; i++) {
            msg[new_len - 1 - i] = (bits_len >> (i * 8)) & 0xFF;
        }

        for (size_t offset = 0; offset < new_len; offset += 64) {
            uint32_t w[80];
            for (int i = 0; i < 16; i++) {
                w[i] = (msg[offset + i * 4] << 24) | (msg[offset + i * 4 + 1] << 16) |
                       (msg[offset + i * 4 + 2] << 8) | (msg[offset + i * 4 + 3]);
            }
            for (int i = 16; i < 80; i++) {
                uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
                w[i] = (val << 1) | (val >> 31);
            }

            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
            for (int i = 0; i < 80; i++) {
                uint32_t f, k;
                if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }

                uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
                e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
            }
            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
        }

        uint32_t h[5] = {h0, h1, h2, h3, h4};
        for (int i = 0; i < 5; i++) {
            hash[i * 4]     = (h[i] >> 24) & 0xFF;
            hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
            hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
            hash[i * 4 + 3] = h[i] & 0xFF;
        }
    }

    static void base64_encode(const uint8_t* in, size_t in_len, char* out) {
        static const char b64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        size_t i = 0;
        size_t j = 0;

        while (i < in_len) {
            size_t remaining = in_len - i;

            uint32_t a = in[i++];
            uint32_t b = (remaining > 1) ? in[i++] : 0;
            uint32_t c = (remaining > 2) ? in[i++] : 0;

            uint32_t triple = (a << 16) | (b << 8) | c;

            out[j++] = b64[(triple >> 18) & 0x3F];
            out[j++] = b64[(triple >> 12) & 0x3F];

            if (remaining > 1)
                out[j++] = b64[(triple >> 6) & 0x3F];
            else
                out[j++] = '=';

            if (remaining > 2)
                out[j++] = b64[triple & 0x3F];
            else
                out[j++] = '=';
        }

        out[j] = '\0';
    }

    static void generate_ws_accept_key(const char* client_key, char* out_accept_key) {
        static const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        char combined[128];
        snprintf(combined, sizeof(combined), "%s%s", client_key, WS_GUID);

        uint8_t sha1_result[20];
        sha1(reinterpret_cast<const uint8_t*>(combined), strlen(combined), sha1_result);
        base64_encode(sha1_result, 20, out_accept_key);
    }

    static void send_http_options(struct tcp_pcb* tpcb) {
        const char* resp = 
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: *\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Connection: close\r\n\r\n";

        cyw43_arch_lwip_begin();
        tcp_write(tpcb, resp, strlen(resp), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        cyw43_arch_lwip_end();
    }

    static void send_http_status(struct tcp_pcb* tpcb) {
        char json[160];
        int body_len = snprintf(json, sizeof(json),
            "{\"status\":\"online\",\"connected\":%s,\"width\":%d,\"height\":%d,\"brightness\":%d,\"fps\":%d}",
            (active_client != nullptr) ? "true" : "false",
            Config::MATRIX_WIDTH, 
            Config::MATRIX_HEIGHT,
            Config::BRIGHTNESS,
            Config::TARGET_FPS
        );

        char resp[300];
        int total_len = snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            body_len, json
        );

        cyw43_arch_lwip_begin();
        tcp_write(tpcb, resp, total_len, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        cyw43_arch_lwip_end();
    }

public:
    bool init() {
        if (cyw43_arch_init()) return false;
        cyw43_arch_enable_sta_mode();
        printf("[WIFI] Connecting to SSID: %s...\n", Config::WIFI_SSID);

        if (cyw43_arch_wifi_connect_timeout_ms(Config::WIFI_SSID, Config::WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("[WIFI] Connection failed!\n");
            return false;
        }

        printf("[WIFI] Connected! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
        return startServer(Config::WS_PORT);
    }

    void update() {
        #if defined(CYW43_LWIP) && CYW43_LWIP
            cyw43_arch_poll();
        #endif
    }

    // Dynamic telemetry broadcast
    void broadcastFrame(const uint8_t* buffer, size_t length, const MatrixTelemetryHeader& telemetry) {
        sendFrame(buffer, length, telemetry);
    }

    void broadcastFrame(const uint8_t* buffer, size_t length) {
        MatrixTelemetryHeader telemetry;
        telemetry.brightness = Config::BRIGHTNESS;
        telemetry.actual_fps = Config::TARGET_FPS;

        datetime_t t;
        if (rtc_get_datetime(&t)) {
            telemetry.year         = t.year;
            telemetry.month        = t.month;
            telemetry.day          = t.day;
            telemetry.hour         = t.hour;
            telemetry.minute       = t.min;
            telemetry.second       = t.sec;
        } else {
            // Fallback or epoch default (e.g., 2026-08-31 00:00:00)
            telemetry.year         = 2026;
            telemetry.month        = 8;
            telemetry.day          = 31;
            telemetry.hour         = 0;
            telemetry.minute       = 0;
            telemetry.second       = 0;
        }

        telemetry.effect_id     = 0;
        telemetry.sub_effect_id = 0;
        telemetry.moon_phase    = 0;
        telemetry.celestial     = 0;

        sendFrame(buffer, length, telemetry);
    }

    bool hasActiveClient() const {
        return active_client != nullptr;
    }

    bool startServer(uint16_t port) {
        server_pcb = tcp_new();
        if (!server_pcb) return false;
        if (tcp_bind(server_pcb, IP_ADDR_ANY, port) != ERR_OK) return false;

        server_pcb = tcp_listen(server_pcb);
        tcp_accept(server_pcb, onAccept);
        printf("[SERVER] Listening on http://%s:%d/\n", ip4addr_ntoa(netif_ip4_addr(netif_default)), port);
        return true;
    }

    static void sendFrame(const uint8_t* buffer, size_t length, const MatrixTelemetryHeader& telemetry) {
        if (active_client == nullptr) return;

        size_t telemetry_len = sizeof(MatrixTelemetryHeader);
        size_t total_payload = telemetry_len + length;

        // WebSocket Framing Header
        uint8_t ws_header[10];
        size_t ws_header_len = 0;

        ws_header[0] = 0x82; // Binary Frame (FIN = 1, Opcode = 2)

        if (total_payload <= 125) {
            ws_header[1] = (uint8_t)total_payload;
            ws_header_len = 2;
        } else if (total_payload <= 65535) {
            ws_header[1] = 126;
            ws_header[2] = (total_payload >> 8) & 0xFF;
            ws_header[3] = total_payload & 0xFF;
            ws_header_len = 4;
        } else {
            ws_header[1] = 127;
            for (int i = 0; i < 8; i++) {
                ws_header[2 + i] = (total_payload >> ((7 - i) * 8)) & 0xFF;
            }
            ws_header_len = 10;
        }

        cyw43_arch_lwip_begin();
        if (active_client != nullptr) {
            size_t total_packet_size = ws_header_len + total_payload;

            if (tcp_sndbuf(active_client) >= total_packet_size) {
                // 1. Write WebSocket framing header
                tcp_write(active_client, ws_header, ws_header_len, TCP_WRITE_FLAG_MORE);
                // 2. Write 20-byte Telemetry Header
                tcp_write(active_client, &telemetry, telemetry_len, TCP_WRITE_FLAG_MORE);
                // 3. Write Raw RGB Matrix Payload
                err_t err = tcp_write(active_client, buffer, length, TCP_WRITE_FLAG_COPY);

                if (err == ERR_OK) {
                    tcp_output(active_client);
                } else {
                    printf("[WS] Send error %d, resetting client connection\n", err);
                    active_client = nullptr;
                }
            }
        }
        cyw43_arch_lwip_end();
    }

private:
    static void onError(void* arg, err_t err) {
        struct tcp_pcb* tpcb = static_cast<struct tcp_pcb*>(arg);
        if (active_client == tpcb) {
            printf("[WS SERVER] Client error (%d). Resetting connection.\n", err);
            active_client = nullptr;
        }
    }

    static const char* custom_strcasestr(const char* haystack, const char* needle) {
        if (!haystack || !needle) return nullptr;
        if (!*needle) return haystack;

        for (; *haystack; ++haystack) {
            if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
                const char *h = haystack + 1;
                const char *n = needle + 1;
                while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
                    ++h;
                    ++n;
                }
                if (!*n) return haystack;
            }
        }
        return nullptr;
    }

    static err_t onReceive(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
        if (!p) {
            printf("[SERVER] Client disconnected cleanly.\n");
            if (active_client != nullptr && active_client == tpcb) {
                printf("[WS] Active WebSocket client disconnected.\n");
                active_client = nullptr;
            }
            tcp_close(tpcb);
            return ERR_OK;
        }

        tcp_recved(tpcb, p->tot_len);
        char* data = static_cast<char*>(p->payload);

        if (strncmp(data, "OPTIONS", 7) == 0) {
            send_http_options(tpcb);
            pbuf_free(p);
            tcp_close(tpcb);
            return ERR_OK;
        }

        if (strstr(data, "GET /api/v1/led/status") != nullptr) {
            send_http_status(tpcb);
            pbuf_free(p);
            tcp_close(tpcb);
            return ERR_OK;
        }

        const char* upgrade_hdr = custom_strcasestr(data, "Upgrade: websocket");
        const char* key_header   = custom_strcasestr(data, "Sec-WebSocket-Key:");

        if (upgrade_hdr && key_header) {
            if (active_client != nullptr && active_client != tpcb) {
                tcp_abort(active_client);
                active_client = nullptr;
            }

            key_header += 18;
            while (*key_header == ' ') key_header++;

            char client_key[64] = {0};
            int i = 0;
            while (key_header[i] != '\r' && key_header[i] != '\n' && key_header[i] != ' ' && i < 63) {
                client_key[i] = key_header[i];
                i++;
            }

            char accept_key[64] = {0};
            generate_ws_accept_key(client_key, accept_key);

            char response[256];
            snprintf(response, sizeof(response),
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: %s\r\n\r\n", 
                accept_key);

            cyw43_arch_lwip_begin();
            tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
            tcp_output(tpcb);
            cyw43_arch_lwip_end();

            active_client = tpcb;
            printf("[WS SERVER] Handshake complete persistent socket active.\n");
            
            pbuf_free(p);
            return ERR_OK;
        }

        if (active_client == tpcb) {
            pbuf_free(p);
            return ERR_OK;
        }

        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        cyw43_arch_lwip_begin();
        tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
        cyw43_arch_lwip_end();

        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_OK;
    }

    static err_t onAccept(void* arg, struct tcp_pcb* newpcb, err_t err) {
        if (err != ERR_OK || !newpcb) return ERR_VAL;
        tcp_arg(newpcb, newpcb);
        tcp_recv(newpcb, onReceive);
        tcp_err(newpcb, onError);
        return ERR_OK;
    }
};
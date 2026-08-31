#include "battery_led.h"
#include "bt.h"
#include "cmd.h"
#include "cmd_contract.h"
#include "config.h"
#include "usb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

int g_failures = 0;

#define EXPECT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            std::cerr << __FILE__ << ':' << __LINE__                             \
                      << ": expected true: " #condition << '\n';                \
            ++g_failures;                                                        \
        }                                                                        \
    } while (false)

#define EXPECT_EQ(actual, expected)                                              \
    do {                                                                         \
        const auto actual_value = (actual);                                      \
        const auto expected_value = (expected);                                  \
        if (!(actual_value == expected_value)) {                                 \
            std::cerr << __FILE__ << ':' << __LINE__                             \
                      << ": expected " #actual " == " #expected << '\n';      \
            ++g_failures;                                                        \
        }                                                                        \
    } while (false)

Config_body g_config{};
bool g_set_result = true;
int g_set_calls = 0;
size_t g_set_len = 0;
std::array<uint8_t, sizeof(Config_body)> g_set_body{};
int g_save_calls = 0;
int g_apply_calls = 0;
int g_reconnect_calls = 0;
int8_t g_rssi = -42;

void reset_fakes() {
    g_set_result = true;
    g_set_calls = 0;
    g_set_len = 0;
    g_set_body.fill(0);
    g_save_calls = 0;
    g_apply_calls = 0;
    g_reconnect_calls = 0;
    g_rssi = -42;
}

void test_bounded_copy() {
    constexpr std::array<uint8_t, 4> source{0x11, 0x22, 0x33, 0x44};
    std::array<uint8_t, 6> output{};
    output.fill(0xaa);

    EXPECT_EQ(cmd_copy_bounded(source.data(), source.size(), output.data(), 0),
              size_t{0});
    EXPECT_EQ(output[0], uint8_t{0xaa});

    EXPECT_EQ(cmd_copy_bounded(source.data(), source.size(), output.data(), 2),
              size_t{2});
    EXPECT_EQ(output[0], uint8_t{0x11});
    EXPECT_EQ(output[1], uint8_t{0x22});
    EXPECT_EQ(output[2], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(cmd_copy_bounded(source.data(), source.size(), output.data(),
                               output.size()),
              source.size());
    EXPECT_TRUE(std::memcmp(output.data(), source.data(), source.size()) == 0);
    EXPECT_EQ(output[source.size()], uint8_t{0xaa});
    EXPECT_EQ(cmd_copy_bounded(nullptr, source.size(), output.data(), output.size()),
              size_t{0});
    EXPECT_EQ(cmd_copy_bounded(source.data(), source.size(), nullptr, output.size()),
              size_t{0});
}

void test_status_response() {
    std::array<uint8_t, 4> output{};
    output.fill(0xaa);

    EXPECT_EQ(cmd_build_status_response(-42, output.data(), 0), size_t{0});
    EXPECT_EQ(output[0], uint8_t{0xaa});

    EXPECT_EQ(cmd_build_status_response(-42, output.data(), 1), size_t{1});
    EXPECT_EQ(output[0], static_cast<uint8_t>(-42));
    EXPECT_EQ(output[1], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(cmd_build_status_response(-128, output.data(), 2), size_t{2});
    EXPECT_EQ(output[0], uint8_t{0x80});
    EXPECT_EQ(output[1], uint8_t{0x80});

    output.fill(0xaa);
    EXPECT_EQ(cmd_build_status_response(0, output.data(), output.size()), size_t{2});
    EXPECT_EQ(output[0], uint8_t{0x00});
    EXPECT_EQ(output[1], uint8_t{0x80});
    EXPECT_EQ(output[2], uint8_t{0xaa});
    EXPECT_EQ(cmd_build_status_response(0, nullptr, 2), size_t{0});
}

void test_set_parser() {
    constexpr size_t kBodySize = sizeof(Config_body);
    std::array<uint8_t, 64> request{};

    for (uint8_t subcommand : {uint8_t{0x01}, uint8_t{0x02}, uint8_t{0x03}}) {
        request[0] = subcommand;
        const auto parsed =
            cmd_parse_set_request(0xf7, request.data(), request.size(), kBodySize);
        EXPECT_EQ(parsed.action, CmdSetAction::invalid);
    }

    EXPECT_EQ(cmd_parse_set_request(0xf6, nullptr, 0, kBodySize).action,
              CmdSetAction::invalid);
    EXPECT_EQ(cmd_parse_set_request(0xf6, request.data(), 0, kBodySize).action,
              CmdSetAction::invalid);

    request[0] = 0x01;
    for (size_t size = 1; size < kBodySize + 1; ++size) {
        const auto parsed =
            cmd_parse_set_request(0xf6, request.data(), size, kBodySize);
        EXPECT_EQ(parsed.action, CmdSetAction::invalid);
        EXPECT_EQ(parsed.payload, nullptr);
        EXPECT_EQ(parsed.payload_size, size_t{0});
    }

    auto parsed = cmd_parse_set_request(
        0xf6, request.data(), kBodySize + 1, kBodySize);
    EXPECT_EQ(parsed.action, CmdSetAction::update);
    EXPECT_EQ(parsed.payload, request.data() + 1);
    EXPECT_EQ(parsed.payload_size, kBodySize);

    parsed = cmd_parse_set_request(
        0xf6, request.data(), request.size(), kBodySize);
    EXPECT_EQ(parsed.action, CmdSetAction::update);
    EXPECT_EQ(parsed.payload_size, kBodySize);

    request[0] = 0x02;
    EXPECT_EQ(cmd_parse_set_request(0xf6, request.data(), 1, kBodySize).action,
              CmdSetAction::save);
    EXPECT_EQ(cmd_parse_set_request(0xf6, request.data(), request.size(), kBodySize)
                  .action,
              CmdSetAction::save);

    request[0] = 0x03;
    EXPECT_EQ(cmd_parse_set_request(0xf6, request.data(), 1, kBodySize).action,
              CmdSetAction::reconnect);

    request[0] = 0xff;
    EXPECT_EQ(cmd_parse_set_request(0xf6, request.data(), 1, kBodySize).action,
              CmdSetAction::unknown);
}

void test_runtime_get_reports() {
    reset_fakes();
    auto *config_bytes = reinterpret_cast<uint8_t *>(&g_config);
    for (size_t index = 0; index < sizeof(g_config); ++index) {
        config_bytes[index] = static_cast<uint8_t>(index + 1);
    }

    std::array<uint8_t, 64> output{};
    output.fill(0xaa);
    EXPECT_EQ(pico_cmd_get(0xf7, output.data(), 0), uint16_t{0});
    EXPECT_EQ(output[0], uint8_t{0xaa});
    EXPECT_EQ(pico_cmd_get(0xf7, output.data(), 7), uint16_t{7});
    EXPECT_TRUE(std::memcmp(output.data(), config_bytes, 7) == 0);
    EXPECT_EQ(output[7], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(pico_cmd_get(0xf7, output.data(), output.size()),
              static_cast<uint16_t>(sizeof(g_config)));
    EXPECT_TRUE(std::memcmp(output.data(), config_bytes, sizeof(g_config)) == 0);
    EXPECT_EQ(output[sizeof(g_config)], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(pico_cmd_get(0xf8, output.data(), 3), uint16_t{3});
    EXPECT_EQ(output[3], uint8_t{0xaa});

    output.fill(0xaa);
    const auto version_len = pico_cmd_get(0xf8, output.data(), output.size());
    EXPECT_TRUE(version_len > 0);
    EXPECT_TRUE(version_len < output.size());
    EXPECT_EQ(output[version_len], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(pico_cmd_get(0xf9, output.data(), 0), uint16_t{0});
    EXPECT_EQ(output[0], uint8_t{0xaa});
    EXPECT_EQ(pico_cmd_get(0xf9, output.data(), 1), uint16_t{1});
    EXPECT_EQ(output[0], static_cast<uint8_t>(g_rssi));
    EXPECT_EQ(output[1], uint8_t{0xaa});

    output.fill(0xaa);
    EXPECT_EQ(pico_cmd_get(0xf9, output.data(), output.size()), uint16_t{2});
    EXPECT_EQ(output[0], static_cast<uint8_t>(g_rssi));
    EXPECT_EQ(output[1], uint8_t{0x80});
    EXPECT_EQ(output[2], uint8_t{0xaa});

    EXPECT_EQ(pico_cmd_get(0xf6, output.data(), output.size()), uint16_t{0});
    EXPECT_EQ(pico_cmd_get(0x10, output.data(), output.size()), uint16_t{0});
}

void test_runtime_set_routing_and_side_effects() {
    static_assert(sizeof(Config_body) == 22, "command wire contract requires v5");

    std::array<uint8_t, 64> request{};
    for (size_t index = 1; index < request.size(); ++index) {
        request[index] = static_cast<uint8_t>(index);
    }

    for (uint8_t report_id : {uint8_t{0xf7}, uint8_t{0xf8}, uint8_t{0xf9}}) {
        for (uint8_t subcommand : {uint8_t{0x01}, uint8_t{0x02}, uint8_t{0x03}}) {
            reset_fakes();
            request[0] = subcommand;
            pico_cmd_set(report_id, request.data(), request.size());
            EXPECT_EQ(g_set_calls, 0);
            EXPECT_EQ(g_save_calls, 0);
            EXPECT_EQ(g_apply_calls, 0);
            EXPECT_EQ(g_reconnect_calls, 0);
        }
    }

    reset_fakes();
    request[0] = 0x01;
    pico_cmd_set(0xf6, request.data(), sizeof(Config_body));
    EXPECT_EQ(g_set_calls, 0);
    EXPECT_EQ(g_apply_calls, 0);

    reset_fakes();
    pico_cmd_set(0xf6, request.data(), sizeof(Config_body) + 1);
    EXPECT_EQ(g_set_calls, 1);
    EXPECT_EQ(g_set_len, sizeof(Config_body));
    EXPECT_TRUE(std::memcmp(g_set_body.data(), request.data() + 1,
                            sizeof(Config_body)) == 0);
    EXPECT_EQ(g_apply_calls, 1);

    reset_fakes();
    pico_cmd_set(0xf6, request.data(), request.size());
    EXPECT_EQ(g_set_calls, 1);
    EXPECT_EQ(g_set_len, sizeof(Config_body));
    EXPECT_EQ(g_apply_calls, 1);

    reset_fakes();
    g_set_result = false;
    pico_cmd_set(0xf6, request.data(), request.size());
    EXPECT_EQ(g_set_calls, 1);
    EXPECT_EQ(g_apply_calls, 0);

    reset_fakes();
    request[0] = 0x02;
    pico_cmd_set(0xf6, request.data(), 1);
    EXPECT_EQ(g_save_calls, 1);
    EXPECT_EQ(g_apply_calls, 0);

    reset_fakes();
    request[0] = 0x03;
    pico_cmd_set(0xf6, request.data(), 1);
    EXPECT_EQ(g_reconnect_calls, 1);

    reset_fakes();
    request[0] = 0xff;
    pico_cmd_set(0xf6, request.data(), 1);
    EXPECT_EQ(g_set_calls, 0);
    EXPECT_EQ(g_save_calls, 0);
    EXPECT_EQ(g_apply_calls, 0);
    EXPECT_EQ(g_reconnect_calls, 0);

    reset_fakes();
    pico_cmd_set(0xf6, nullptr, 0);
    EXPECT_EQ(g_set_calls, 0);
    EXPECT_EQ(g_save_calls, 0);
    EXPECT_EQ(g_reconnect_calls, 0);
}

}  // namespace

const Config_body &get_config() {
    return g_config;
}

bool set_config(const uint8_t *new_config, size_t len) {
    ++g_set_calls;
    g_set_len = len;
    if (new_config != nullptr && len >= g_set_body.size()) {
        std::memcpy(g_set_body.data(), new_config, g_set_body.size());
    }
    return g_set_result;
}

bool config_save() {
    ++g_save_calls;
    return true;
}

void battery_led_apply_config() {
    ++g_apply_calls;
}

void usb_reconnect() {
    ++g_reconnect_calls;
}

void bt_get_signal_strength(int8_t *rssi) {
    *rssi = g_rssi;
}

int main() {
    test_bounded_copy();
    test_status_response();
    test_set_parser();
    test_runtime_get_reports();
    test_runtime_set_routing_and_side_effects();

    if (g_failures != 0) {
        std::cerr << g_failures << " command contract expectation(s) failed\n";
        return 1;
    }

    std::cout << "command contract tests passed\n";
    return 0;
}

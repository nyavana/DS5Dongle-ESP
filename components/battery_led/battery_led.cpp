//
// Low-battery LED indicator (ported from legacy-pico/src/battery_led.cpp).
//
// The reference build drove its onboard LED helper. The S31 port uses a plain
// GPIO output with a compile-time disabled default because the real board LED
// pin is not known yet. Set DS5_BATTERY_LED_GPIO at compile time once hardware
// exists.
//

#include "battery_led.h"

#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "report_buffer.h"

#ifndef DS5_BATTERY_LED_GPIO
#define DS5_BATTERY_LED_GPIO -1
#endif

namespace {

constexpr int64_t kReportStaleUs = 2'000'000;  // assume disconnected if no report for 2 s
constexpr int64_t kBlinkPeriodUs = 500'000;    // 1 Hz, 50% duty
constexpr uint8_t kThresholdLevel = 1;         // PowerPercent <= 1 (<= 10%)
constexpr uint8_t kPowerStateDischarging = 0x0;
constexpr uint32_t kBatteryTaskStack = 2048;
constexpr TickType_t kTickPeriod = pdMS_TO_TICKS(50);

const char *TAG = "battery_led";

TaskHandle_t s_task = nullptr;
portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
int64_t s_last_report_us = 0;
int64_t s_last_toggle_us = 0;
bool s_blinking = false;
bool s_led_state = false;

bool led_available() {
    return DS5_BATTERY_LED_GPIO >= 0;
}

void write_led(const bool on) {
#if DS5_BATTERY_LED_GPIO >= 0
    gpio_set_level(static_cast<gpio_num_t>(DS5_BATTERY_LED_GPIO), on ? 1 : 0);
#else
    (void) on;
#endif
}

void set_state(const bool blinking, const bool led_state, const int64_t last_toggle_us) {
    taskENTER_CRITICAL(&s_lock);
    s_blinking = blinking;
    s_led_state = led_state;
    s_last_toggle_us = last_toggle_us;
    taskEXIT_CRITICAL(&s_lock);
}

void force_off() {
    set_state(false, false, 0);
    write_led(false);
}

bool latest_report_is_low_battery() {
    const uint8_t b = report_buffer_get_byte(52);
    const uint8_t pct = b & 0x0f;
    const uint8_t state = (b >> 4) & 0x0f;
    return state == kPowerStateDischarging && pct <= kThresholdLevel;
}

void battery_led_task(void *arg) {
    (void) arg;
    while (true) {
        battery_led_tick();
        vTaskDelay(kTickPeriod);
    }
}

} // namespace

void battery_led_init(void) {
    taskENTER_CRITICAL(&s_lock);
    s_last_report_us = 0;
    s_last_toggle_us = 0;
    s_blinking = false;
    s_led_state = false;
    taskEXIT_CRITICAL(&s_lock);

#if DS5_BATTERY_LED_GPIO >= 0
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << DS5_BATTERY_LED_GPIO;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io));
    write_led(false);
    ESP_LOGI(TAG, "battery LED GPIO %d configured", DS5_BATTERY_LED_GPIO);
#else
    ESP_LOGI(TAG, "battery LED GPIO disabled; set DS5_BATTERY_LED_GPIO when board pin is known");
#endif

    if (s_task == nullptr) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            battery_led_task,
            "battery_led",
            kBatteryTaskStack,
            nullptr,
            3,
            &s_task,
            tskNO_AFFINITY
        );
        if (created != pdPASS) {
            ESP_LOGE(TAG, "failed to create battery LED task");
            s_task = nullptr;
        }
    }
}

void battery_led_note_report(void) {
    taskENTER_CRITICAL(&s_lock);
    s_last_report_us = esp_timer_get_time();
    taskEXIT_CRITICAL(&s_lock);
}

void battery_led_on_disconnect(void) {
    taskENTER_CRITICAL(&s_lock);
    s_last_report_us = 0;
    taskEXIT_CRITICAL(&s_lock);
    force_off();
}

void battery_led_apply_config(void) {
    if (get_config().disable_pico_led) {
        force_off();
    }
}

void battery_led_tick(void) {
    const int64_t now = esp_timer_get_time();

    int64_t last_report_us = 0;
    int64_t last_toggle_us = 0;
    bool blinking = false;
    bool led_state = false;
    taskENTER_CRITICAL(&s_lock);
    last_report_us = s_last_report_us;
    last_toggle_us = s_last_toggle_us;
    blinking = s_blinking;
    led_state = s_led_state;
    taskEXIT_CRITICAL(&s_lock);

    if (!led_available() || get_config().disable_pico_led) {
        if (blinking || led_state) {
            force_off();
        }
        return;
    }

    if (last_report_us == 0 || (now - last_report_us) >= kReportStaleUs) {
        if (blinking || led_state) {
            force_off();
        }
        return;
    }

    if (!latest_report_is_low_battery()) {
        if (blinking || led_state) {
            force_off();
        }
        return;
    }

    if (!blinking) {
        set_state(true, true, now);
        write_led(true);
        return;
    }

    if ((now - last_toggle_us) >= kBlinkPeriodUs) {
        const bool next_state = !led_state;
        set_state(true, next_state, now);
        write_led(next_state);
    }
}

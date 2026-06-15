## ADDED Requirements

### Requirement: ESP32-S31 target selection and clocking

The firmware SHALL target the `esp32s31` ESP-IDF preview target and SHALL configure the SoC for its native 320 MHz operation through `sdkconfig.defaults` (not runtime register pokes). The Pico-specific `vreg_set_voltage` / `set_sys_clock_khz(320000)` calls MUST NOT appear in the port.

#### Scenario: Project builds for the preview target

- **WHEN** `idf.py --preview set-target esp32s31` then `idf.py build` is run with the `espidf:s31` (IDF master) toolchain
- **THEN** the build completes without errors and produces a flashable `.bin`

#### Scenario: CPU frequency comes from sdkconfig

- **WHEN** the build configuration is inspected
- **THEN** the CPU frequency is set via the Kconfig CPU-freq symbol in `sdkconfig.defaults` and no `set_sys_clock_khz` / `vreg_set_voltage` call exists in the source

### Requirement: Application entrypoint and task architecture

The firmware SHALL boot from `app_main()` and SHALL replace the Pico single super-loop with FreeRTOS tasks plus Bluetooth/USB event callbacks. `main/` SHALL stay thin, delegating subsystem logic to components.

#### Scenario: Boot brings up subsystems in dependency order

- **WHEN** `app_main()` runs
- **THEN** it initializes configuration (NVS), then Bluetooth host, then USB device, then the report-bridge and default-disabled audio scaffold, registering callbacks/tasks rather than polling them in a busy loop

#### Scenario: No Pico SDK super-loop remains

- **WHEN** the ported `main` source is inspected
- **THEN** there is no `while(1)` super-loop calling `cyw43_arch_poll`, `tud_task`, `audio_loop`, and `interrupt_loop`; those responsibilities are driven by tasks/callbacks

### Requirement: Logging and watchdog

The firmware SHALL use ESP-IDF logging (`esp_log`) for diagnostics and SHALL guard against hangs using the task watchdog (`esp_task_wdt`) in place of the Pico `hardware/watchdog.h` API.

#### Scenario: Boot is observable over the log

- **WHEN** the device boots
- **THEN** it emits an `ESP_LOGI` startup line identifying the firmware and target

#### Scenario: Watchdog replaces Pico watchdog

- **WHEN** the source is inspected
- **THEN** hang protection uses `esp_task_wdt` APIs and no `watchdog_enable` / `watchdog_update` / `watchdog_caused_reboot` Pico calls remain

### Requirement: Concurrency primitives

Shared state between callbacks and tasks (e.g. the input-report buffer) SHALL be protected with FreeRTOS-compatible critical sections (`portMUX_TYPE` / `taskENTER_CRITICAL`) rather than the Pico `critical_section_t` API.

#### Scenario: Report buffer access is guarded

- **WHEN** the BT input callback and the USB report path both access the shared input-report buffer
- **THEN** access is serialized with a `portMUX_TYPE` critical section so no torn reads/writes occur, and no `critical_section_t` usage remains

## 0. Pre-flight

- [x] 0.1 Confirm the `espidf:s31` Docker image is available and define the build-gate command used after every group: `docker run --rm -v "$PWD":/p -w /p espidf:s31 idf.py --preview set-target esp32s31 build`
- [x] 0.2 Run the build-gate once on the current scaffold to establish a known-clean baseline
- [x] 0.3 Read `legacy-pico/src/<module>` + its header before porting each subsystem; keep the existing module headers (`bt.h`, `usb.h`, etc.) as the stable contract

## 1. Boot baseline (firmware-boot)

- [x] 1.1 Reconcile `sdkconfig.defaults` against the actual S31 Kconfig: CPU-freq symbol, `CONFIG_BT_CLASSIC_ENABLED` (or its S31 name), USB-OTG, NVS, and task-WDT symbols; record any renamed/missing symbols
- [x] 1.2 Rewrite `main/main.cpp` into an `app_main()` that initializes subsystems in order (config → bt → usb → bridge → audio) and starts FreeRTOS tasks instead of a super-loop
- [x] 1.3 Add `esp_log` startup logging and replace Pico watchdog usage with `esp_task_wdt`
- [x] 1.4 Add a shared report-buffer module guarded by `portMUX_TYPE` (replacing `critical_section_t`) for later use by the bridge
- [x] 1.5 Build-gate: clean `esp32s31` build; commit `feat: boot skeleton (app_main + tasks)`

## 2. USB device — HID only (usb-device)

- [x] 2.1 Create `components/usb/` (`CMakeLists.txt` + `idf_component_register`); add `esp_tinyusb` to `main/idf_component.yml`
- [x] 2.2 Port `usb_descriptors.cpp` / `tusb_config.h` verbatim and feed them to `esp_tinyusb` as custom descriptors (HID interface first; leave UAC audio interfaces stubbed for group 6)
- [x] 2.3 Port the `tud_hid_get_report_cb` / `tud_hid_set_report_cb` skeletons, routing command IDs (`0xf6`–`0xf9`) and feature IDs to stubs to be filled by groups 4–5
- [x] 2.4 Initialize USB via `esp_tinyusb` (replace `board_init` / `tusb_init`); expose `usb_init()`
- [x] 2.5 Wire `usb_init()` into `app_main()`
- [x] 2.6 Build-gate: clean `esp32s31` build; commit `feat: USB device enumeration (HID)`

## 3. Bluetooth BR/EDR HID host (bt-hid-host)

- [x] 3.1 Create `components/bt/` and bring up the S31 native controller in BR/EDR mode + Bluedroid as a HID host (`esp_hidh`), behind the existing `bt.h` API  [component named `bthost` to avoid overriding IDF's `bt`]
- [x] 3.2 Implement discovery filtered by class-of-device and connect to the DualSense; complete pairing and persist the bond in NVS
- [x] 3.3 Implement `bt_register_data_callback` delivery of HID interrupt input reports (channel/data/len) and `bt_get_signal_strength` (RSSI)
- [x] 3.4 Implement `bt_write` (output report send over the HID interrupt channel)
- [x] 3.5 If `esp_hidh` cannot pass raw DualSense reports or filter discovery, fall back to raw Bluedroid L2CAP (PSM `0x11`/`0x13`) inside the component without changing `bt.h`; document which path was used  [CHOSEN: esp_hidh — it exposes raw INPUT reports + CoD-filtered discovery, so the raw-L2CAP fallback was NOT needed]
- [x] 3.6 Wire `bt_init()` + callback registration into `app_main()`  [bt_init wired; on_bt_data registration lands in group 4 with the bridge]
- [x] 3.7 Build-gate: clean `esp32s31` build; commit `feat: BR/EDR HID host (Bluedroid)`

## 4. Report bridge + state + cmd (report-bridge)

- [x] 4.1 Create `components/state_mgr/` and port `state_mgr.cpp` (`state_init`/`state_set`/`state_update`) as-is
- [x] 4.2 Create `components/cmd/` and port `cmd.cpp` (`is_pico_cmd`/`pico_cmd_get`/`pico_cmd_set`), replacing `tud_disconnect/connect` with the `esp_tinyusb` equivalents and `sleep_ms` with `vTaskDelay`
- [x] 4.3 Implement the BT→USB input relay: copy `0x31` report (`data+3`, 63 bytes) into the guarded buffer, detect the headset bit, send as HID `0x01`  [components/bridge]
- [x] 4.4 Implement the polling-rate modes (0/1 direct forward; 2 real-time dirty-flag under `portMUX`, re-arm on queue-full)
- [x] 4.5 Implement the USB→BT output relay: build `0x31` output with sequence counter via `state_set`, suppress while speaker active, send via `bt_write`
- [x] 4.6 Fill the `tud_hid_set_report_cb` `0x02`/feature-ID branches and the `tud_hid_get_report_cb` feature branch to call into cmd/state  [callbacks relocated to components/bridge to avoid a usb<->bridge dependency cycle]
- [x] 4.7 Build-gate: clean `esp32s31` build; commit `feat: HID report bridge + state/cmd`

## 5. Device config in NVS (device-config)

- [x] 5.1 Create `components/config/` and port `config.cpp`; replace `hardware/flash`/`hardware/sync` with NVS blob get/set (namespace `ds5`, key `config`)  [ported early — cmd/bridge depend on it]
- [x] 5.2 Keep magic/version/size/CRC validation and per-field range checks → defaults (`config_valid`)
- [x] 5.3 Implement `config_load` (NVS or defaults) and `config_save` (write + read-back verify)
- [ ] 5.4 Route `disable_pico_led` to the battery-indicator/LED control instead of `cyw43_arch_gpio_put`  [deferred to group 7 — battery_led]
- [x] 5.5 Confirm `0xf6`–`0xf9` command paths (config get/set/save, firmware version, RSSI, USB reconnect) work end-to-end against the ported config/bt/usb  [wired + builds; runtime confirmation needs hardware]
- [x] 5.6 Wire `config_load()` into `app_main()` before bt/usb init
- [x] 5.7 Build-gate: clean `esp32s31` build; commit `feat: NVS config + command interface`

## 6. Audio bridge (audio-bridge)

- [ ] 6.1 Resolve the Opus ESP Component Registry package id/version in `main/idf_component.yml` (vendor Opus only if the registry entry is unusable)
- [ ] 6.2 Create `components/audio/`; vendor the WDL resampler used by `audio.cpp`
- [ ] 6.3 Add the UAC audio interfaces to the USB descriptors (from group 2) and the `tud_audio_*` control callbacks (mute/volume, speaker alt-setting tracking)
- [ ] 6.4 Port `audio_loop`: read 4-ch host audio (`tud_audio_read` → ESP audio API), split speaker/haptics with gains, resample haptics 48k→3k, build `0x36` report, `bt_write`
- [ ] 6.5 Port the Opus encoder onto a FreeRTOS task pinned to the app core (replacing `multicore_launch_core1`), with a FreeRTOS queue (replacing `queue_t`) and `portMUX` (replacing `opus_cs`)
- [ ] 6.6 Implement `set_headset` routing (speaker vs headset sub-mode) driven by the bridge's headset-bit detection
- [ ] 6.7 Wire `audio_init()` into `app_main()`
- [ ] 6.8 Build-gate: clean `esp32s31` build; commit `feat: audio bridge (Opus + resampler over BT)`

## 7. Battery indicator (battery-indicator)

- [ ] 7.1 Create `components/battery_led/`; port `battery_led.cpp` driving a board GPIO / `led_strip` instead of the CYW43 onboard LED
- [ ] 7.2 Derive battery percent/state from the parsed input report; honour `disable_pico_led`
- [ ] 7.3 Implement disconnect/staleness handling (`battery_led_note_report`, `battery_led_on_disconnect`, timeout) wired to the bridge + BT disconnect event
- [ ] 7.4 Wire `battery_led_init/tick` into `app_main()`/bridge
- [ ] 7.5 Build-gate: clean `esp32s31` build; commit `feat: low-battery LED indicator`

## 8. CI, cleanup & report

- [ ] 8.1 Port `.github/workflows/` from the Pico `.uf2` build to `idf.py --preview set-target esp32s31 build` on the `espidf:s31` image
- [ ] 8.2 Update `main/CMakeLists.txt` REQUIRES and `components/README.md` / `docs/MIGRATION.md` to reflect the ported components and the chosen BT path
- [ ] 8.3 Confirm no Pico SDK / BTstack / `cyw43` / `bsp/board_api` references remain in ported code (grep)
- [ ] 8.4 Final build-gate: full clean `esp32s31` build from scratch
- [ ] 8.5 Write the completion report: what was ported, the BT path chosen (esp_hidh vs raw L2CAP), resolved/unresolved open questions, and the explicit list of runtime paths (BR/EDR pairing, USB enumeration, audio) that could not be verified without physical S31 hardware

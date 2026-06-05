● I've hit a genuine decision point on Group 6 (audio). The core firmware (boot →
  USB → BT → bridge → config) is complete and building clean — that's the
  controller's primary function. But audio is a large, high-risk effort with hard
  external dependencies, and it's the one area the proposal itself frames as
  hardware-deferred. Specifically it needs all three of:

  1. WDL resampler — vendored (submodule is unpopulated; network works so it's
  fetchable).
  2. Opus codec — espressif/opus does not exist in the registry; the only source is
  the unpopulated xiph/opus submodule, which means vendoring tens of thousands of
  lines of third-party C into the branch with a hand-written ESP-IDF CMakeLists
  (fixed/float config, xtensa) — error-prone and iterated over multi-minute builds.
  3. UAC USB-audio — esp_tinyusb has zero audio-class support, so I'd inject
  CFG_TUD_AUDIO + the 4-channel config into TinyUSB's own compilation and
  restructure the descriptor to the full HID+UAC composite.

  All three are entirely runtime-untestable here (no S31 hardware). This is a
  consequential scope/effort call (notably the heavy third-party vendoring), so I'd
  like your decision before proceeding.
───────────────────────────────────────────────────────────────────────────────────
 ☐ Audio (Group 6)

How should I handle Group 6 (audio bridge), given it needs vendoring xiph/opus +
WDL and injecting UAC into esp_tinyusb, all runtime-untestable?

❯ 1. Port logic, stub Opus/UAC
     Create components/audio with the fully ported bridge logic (FreeRTOS
     task/queue/portMUX, 0x36 packetization, set_headset) + vendored WDL, but keep
     Opus-encode and UAC-capture behind clear stubs/TODOs. Build stays clean; no
     huge third-party dump. Document Opus pkg + UAC + hardware as the remaining
     human step.
  2. Attempt full port
     Vendor xiph/opus (large) + WDL, inject UAC into TinyUSB, port audio.cpp fully,
     and iterate builds until clean. Largest effort, many multi-minute build
     cycles, high risk it needs several fix rounds — all still untestable without
     hardware.
  3. Skip audio, do 7-8
     Leave Group 6 unstarted for now, proceed to Group 7 (battery LED) + Group 8
     (CI, cleanup, completion report). Audio documented as a scoped follow-up.
  4. Type something.
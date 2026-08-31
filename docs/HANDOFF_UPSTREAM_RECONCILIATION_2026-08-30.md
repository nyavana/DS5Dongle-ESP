# Fresh-Session Handoff: ESP32-S31 Canonical Reconciliation

Prepared: 2026-08-30

Workspace: `/home/nyavana/git/DS5Dongle-ESP`

Implementation worktree: `/home/nyavana/git/DS5Dongle-ESP/esp32-s31`

## Recommended continuation

Start a fresh, narrowly scoped Trellis task for the current version-5
configuration and host-tool contract. Do not resume the superseded July parent
or child, and do not attempt the complete canonical reconciliation in one
implementation pass.

The recommended order is:

1. Decide whether to publish the two local maintenance/documentation commits so
   the clean workflow baseline is shared before firmware work begins.
2. In the new session, obtain consent and create one fresh Trellis task for the
   version-5 config/NVS/host-tool slice.
3. Write and review its `prd.md`, `design.md`, and `implement.md`; use those
   artifacts as the executable contract.
4. Activate only that task, dispatch one bounded implementation agent, and keep
   diff review, integration fixes, verification, and commit ownership with the
   primary session.
5. Create later Trellis tasks only after this first slice passes its host,
   static, and ESP-IDF build gates.

The first slice should adopt the current 22-byte canonical schema directly,
including the two status-GPIO bytes for wire and storage compatibility, while
leaving GPIO behavior disabled until a board is selected. This avoids landing
a 20-byte intermediate schema and migrating it again later.

## Clean baseline at handoff

| Item | State |
|---|---|
| ESP32-S31 branch | `esp32-s31` at `37b82994ada1e8bd623d94e871dd7aad801c2f3a` before this handoff commit |
| Branch relationship | clean and one commit ahead of `origin/esp32-s31` before this handoff commit |
| Published `origin/esp32-s31` | `61b8b3f5888612d3f58b24cc5f41200302cc59d8` |
| Frozen Pico reference | `8760ee3f4fa9335e3c5e1a0d0aead92b55f23abb` |
| Local `master` worktree | clean at `fadcf2f2424f4724393b196966fbbc9558c16aae` |
| Canonical repository | `https://github.com/awalol/DS5Dongle` |
| Canonical `master` | `17385f8beeef17129f0b39d9e5fc2195ea89b322`, rechecked live on 2026-08-30 |
| Trellis current task | none |
| Hardware | no physical ESP32-S31 available |

Canonical `master` is 208 commits beyond the frozen reference: 42 changed
files, 4,229 additions, and 528 deletions. It is 19 commits beyond the previous
July comparison target `a66f5cf9eebb1938019eac9ec92f3f1406c99376`:
16 changed files, 295 additions, and 99 deletions.

The retired spec workflow, repository-local helpers, partial worker artifacts,
and earlier audit/handoff changes were removed. Commit `37b8299` is the durable
workflow-removal baseline. No system-wide CLI removal is required for this
project. Use Trellis plus ordinary project documentation; do not recreate the
retired workflow.

The earlier Trellis reconciliation tasks are retained only as superseded
history under:

- `.trellis/tasks/archive/2026-08/07-20-update-port-from-upstream/`;
- `.trellis/tasks/archive/2026-08/07-20-config-host-contract/`.

They were not completed and must not be resumed or cited as implementation
evidence. The unrelated `00-bootstrap-guidelines` task remains active but is not
the firmware reconciliation task.

## Read first in the new session

From the bare-repository root:

1. Read `AGENTS.md`, `LAYOUT.md`, and the root `CLAUDE.md`.
2. Read `esp32-s31/AGENTS.md`, this handoff, `docs/MIGRATION.md`,
   `docs/COMPLETION.md`, and `decision.md`.
3. Load Trellis session context and confirm there is no reconciliation task
   selected.
4. Recheck both worktrees and the live canonical SHA before writing planning
   artifacts.
5. Read the `esp32-s31` Trellis spec indexes before implementation.

All shell commands must be prefixed with `rtk`. Run package Git commands from
the actual worktree or with `rtk git -C esp32-s31 ...`; the checkout root is a
bare repository with linked worktrees.

`master/` and `esp32-s31/legacy-pico/` are frozen references. Do not modify
either one, and do not treat fork-local `master` as live canonical upstream.

## Why configuration should be first

The ESP port still implements the frozen version-1 contract:

- an outer `Config.version` field and a 27-byte persisted record;
- `speaker_volume` as a float;
- `disable_inactive_disconnect`;
- one-byte `0xf9` status;
- direct TinyUSB reconnect logic in `cmd`;
- no portable config tool or dependency-free host contract tests.

The current canonical target uses a packed version-5 body. Three post-July
config commits matter:

- `70f508f1`: valid zero-valued fields must not be rejected;
- `86da98c6`: adds `status_gpio_pin` and `status_gpio_mode` to the wire/storage
  schema;
- `dbf2a71c`: moves Pico raw-flash storage and also changes the canonical audio
  buffer default. The raw-flash migration is Pico-only; portable schema/default
  behavior must be reviewed separately.

Configuration is therefore the smallest useful boundary that establishes a
tested cross-language contract for later controller, USB/wake, and audio work.

## First-task contract

### Scope

Own only:

- `components/config/`;
- bounded config/status helpers under `components/cmd/`;
- the minimal explicit reconnect/apply seams in `components/usb/` and
  `components/battery_led/` required by the command contract;
- `tools/config_tool.py`;
- `tools/wireshark_dualsense_setstate.lua`;
- dependency-free host tests under `tests/host/`;
- directly relevant user documentation.

Do not modify controller lifecycle/BT/DSE behavior, USB descriptors or wake,
audio runtime, board GPIO output, `master/`, or `legacy-pico/` in this task.

### Canonical version-5 schema

Use one packed 22-byte `Config_body` with these offsets and defaults:

| Offset | Field | Type | Valid range | Default |
|---:|---|---|---|---:|
| 0 | `config_version` | `uint8_t` | exactly 5 | 5 |
| 1 | `haptics_gain` | `float` | finite 1.0-2.0 | 1.0 |
| 5 | `speaker_volume` | `uint8_t` | 0-127 | 100 |
| 6 | `headset_volume` | `uint8_t` | 0-127 | 100 |
| 7 | `speaker_gain` | `uint8_t` | 0-7 | 2 |
| 8 | `inactive_time` | `uint8_t` | 0-60 | 30 |
| 9 | `disable_pico_led` | `uint8_t` | 0 or 1 | 0 |
| 10 | `polling_rate_mode` | `uint8_t` | 0-2 | 1 |
| 11 | `audio_buffer_length` | `uint8_t` | 16-128 | 48 |
| 12 | `controller_mode` | `uint8_t` | 0-2 | 2 |
| 13 | `enable_usb_sn` | `uint8_t` | 0 or 1 | 0 |
| 14 | `ps_shortcut_enabled` | `uint8_t` | 0 or 1 | 0 |
| 15 | `mic_select` | `uint8_t` | 0-3 | 0 |
| 16 | `speaker_select` | `uint8_t` | 0-3 | 0 |
| 17 | `enable_wake` | `uint8_t` | 0 or 1 | 0 |
| 18 | `trigger_reduce` | `uint8_t` | 0-10 | 0 |
| 19 | `lock_volume` | `uint8_t` | 0 or 1 | 0 |
| 20 | `status_gpio_pin` | `uint8_t` | board-valid pin or `0xff` | `0xff` |
| 21 | `status_gpio_mode` | `uint8_t` | 0 or 1 | 0 |

Use a packed 32-byte NVS envelope:
`{uint32_t magic, uint32_t crc32, uint16_t size, Config_body body}` with magic
`0x66ccff00`. Compile-time guards should fix all offsets, body/envelope sizes,
and `sizeof(SetStateData) == 47`.

Status-GPIO fields are part of the canonical schema, but their runtime output
remains disabled. Store, validate, and report them without selecting a real pin
or claiming GPIO behavior.

### Validation and NVS

- Use pure ESP-IDF-independent helpers for defaults, normalization, CRC, and
  record acceptance so production logic is host-testable.
- Treat zero as valid wherever the declared range includes zero; do not use
  truthiness as validation.
- Accept only an exact 32-byte record with valid magic, body size, schema
  version, and CRC.
- Reset incompatible version-1 data to complete version-5 defaults; do not
  reinterpret or migrate its bytes.
- Keep persistence NVS-only. Do not copy Pico flash offsets, sector migration,
  erase/program operations, or Pico flash locking.
- On save, normalize, calculate CRC, write, commit, read back, fully validate,
  and compare the body before reporting success.

### Feature reports and tools

- `0xf7`: bounded reads from the 22-byte body.
- `0xf8`: bounded firmware-version reads.
- `0xf9`: RSSI in byte 0; when two bytes are requested, byte 1 is `0x80` with
  audio activity bits clear while runtime audio remains disabled.
- Only `0xf6` accepts SET subcommands.
- `0xf6/0x01`: require one full body, normalize it, ignore report padding, and
  leave current state unchanged on short input.
- `0xf6/0x02`: save and verify through NVS.
- `0xf6/0x03`: request re-enumeration through a USB-component boundary rather
  than calling TinyUSB directly from `cmd`.
- Adapt the canonical Python tool with one ordered field table, lazy `hidapi`,
  gamepad-interface filtering, version checks, 64-byte writes, read-back, and
  NVS-neutral wording.
- Add the portable SetState Wireshark post-dissector without Pico build
  assumptions.

### Required host coverage

Cover exact layout/defaults, every range boundary, valid zeros, NaN/infinity,
schema reset, length/magic/size/version/CRC rejection, deterministic round trip,
short and padded command buffers, Python field order and pack/unpack, HID report
prefix variants, interface filtering, update preservation, version mismatch,
and 64-byte write padding.

## Verification boundary

Run from `esp32-s31/` unless noted:

```bash
rtk python3 -m unittest discover -s tests/host -p 'test_*.py'
rtk cmake -S tests/host -B /tmp/ds5-s31-host-tests
rtk cmake --build /tmp/ds5-s31-host-tests
rtk ctest --test-dir /tmp/ds5-s31-host-tests --output-on-failure
rtk git diff --check
rtk docker run --rm -v "$PWD":/p -w /p espidf:s31 \
  idf.py --preview set-target esp32s31 build
rtk git status --short --untracked-files=all
```

From the checkout root, prove the reference worktrees were not modified:

```bash
rtk git -C master status --short
rtk git -C esp32-s31 status --short --untracked-files=all
```

Keep generated builds, managed components, caches, credentials, and raw logs
untracked. If the Docker image, registry, or network is unavailable, report the
build gate as blocked rather than weakening requirements.

No device tests, flashing, serial monitoring, Bluetooth pairing, USB host
enumeration, audio testing, wake testing, or GPIO testing are possible without
hardware. Do not claim runtime success.

## Later task order

After the config task passes and is reviewed:

1. Controller discovery/reconnect, report validation, and DSE firmware-report
   behavior.
2. Exact `esp_tinyusb` v2 qualification, descriptors, suspend/resume, wake, and
   shortcuts.
3. Audio scaffold contract refresh only; keep UAC, codecs, and live audio
   disabled.
4. Optional status-GPIO runtime behavior after an actual board/pin is selected.
5. Integration, coverage-ledger closure, documentation reconciliation, Docker
   build, and a static-evidence completion report.

## Completion language

Supported conclusion:

> The current canonical config/host contract was re-ported and passes
> host/static/build gates. Physical ESP32-S31 Bluetooth, USB, wake, audio, and
> GPIO behavior remains unverified because no hardware is available.

Do not use “fully working,” “runtime verified,” “hardware validated,” or
“shipping ready.”

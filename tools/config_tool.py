#!/usr/bin/env python3
"""Read and modify ds5dongle configuration over USB HID.

Protocol (see components/cmd and components/config):
  GET feature report 0xF7 -> packed Config_body bytes
  GET feature report 0xF8 -> firmware version string
  SET feature report 0xF6:
      funcid 0x01 + body   -> update the current configuration
      funcid 0x02          -> persist the current configuration to device NVS
      funcid 0x03          -> reconnect the USB device

The HID dependency is loaded only when a command opens a device, so the binary
contract helpers remain usable with the Python standard library alone.

Requires for device access: pip install hidapi

Examples:
  python config_tool.py get
  python config_tool.py set speaker_volume=90 enable_wake=1
  python config_tool.py set haptics_gain=1.5 --no-save
  python config_tool.py fields
"""

import argparse
import math
import struct
import sys


def _load_hid():
    try:
        import hid
    except ImportError:
        sys.exit("Missing dependency. Install with:  pip install hidapi")
    return hid


VID = 0x054C
PIDS = (0x0CE6, 0x0DF2)  # DualSense, DualSense Edge
HID_USAGE_PAGE_GENERIC_DESKTOP = 0x01
HID_USAGE_GAMEPAD = 0x05

REPORT_SET = 0xF6
REPORT_GET_CONFIG = 0xF7
REPORT_GET_VERSION = 0xF8

FUNC_UPDATE = 0x01
FUNC_SAVE = 0x02
FUNC_RECONNECT = 0x03

SET_DATA_LEN = 63
FEATURE_REPORT_LEN = SET_DATA_LEN + 1
CONFIG_VERSION = 5

KIND_TO_CODE = {"u8": "B", "float": "f"}

# This ordered table owns the Python view of Config_body. Pack, unpack,
# defaults, validation, display, and assignment updates all derive from it.
# name, kind, default, validator(value), help. Order must match config.h.
FIELDS = (
    ("config_version", "u8", 5, lambda value: value == CONFIG_VERSION,
     "schema version 5 (managed by firmware)"),
    ("haptics_gain", "float", 1.0,
     lambda value: math.isfinite(value) and 1.0 <= value <= 2.0,
     "finite value in [1.0, 2.0]"),
    ("speaker_volume", "u8", 100, lambda value: 0 <= value <= 127,
     "[0, 127]"),
    ("headset_volume", "u8", 100, lambda value: 0 <= value <= 127,
     "[0, 127]"),
    ("speaker_gain", "u8", 2, lambda value: 0 <= value <= 7,
     "[0, 7]"),
    ("inactive_time", "u8", 30, lambda value: 0 <= value <= 60,
     "[0, 60] minutes (0 disables)"),
    ("disable_pico_led", "u8", 0, lambda value: value in (0, 1),
     "0/1"),
    ("polling_rate_mode", "u8", 1, lambda value: value in (0, 1, 2),
     "0:250Hz 1:500Hz 2:real-time"),
    ("audio_buffer_length", "u8", 48, lambda value: 16 <= value <= 128,
     "[16, 128]"),
    ("controller_mode", "u8", 2, lambda value: value in (0, 1, 2),
     "0:DS5 1:DSE 2:Auto"),
    ("enable_usb_sn", "u8", 0, lambda value: value in (0, 1),
     "0/1 (USB serial number)"),
    ("ps_shortcut_enabled", "u8", 0, lambda value: value in (0, 1),
     "0/1 (Xbox Game Bar via HID keyboard)"),
    ("mic_select", "u8", 0, lambda value: value in (0, 1, 2, 3),
     "0:auto 1:builtin 2:headphone 3:disable"),
    ("speaker_select", "u8", 0, lambda value: value in (0, 1, 2, 3),
     "0:auto 1:builtin 2:headphone 3:disable"),
    ("enable_wake", "u8", 0, lambda value: value in (0, 1),
     "0/1 (wake host on PS press)"),
    ("trigger_reduce", "u8", 0, lambda value: 0 <= value <= 10,
     "[0, 10] (0: auto)"),
    ("lock_volume", "u8", 0, lambda value: value in (0, 1),
     "0/1 (ignore SetStateData volume changes)"),
    ("status_gpio_pin", "u8", 0xFF, lambda value: 0 <= value <= 0xFF,
     "GPIO number (255 disables; stored only, status output disabled)"),
    ("status_gpio_mode", "u8", 0, lambda value: value in (0, 1),
     "0:pull high 1:200ms button pulse (stored only, status output disabled)"),
)

FIELD_NAMES = tuple(field[0] for field in FIELDS)
FIELD_BY_NAME = {field[0]: field for field in FIELDS}
DEFAULT_CONFIG = {name: default for name, _kind, default, _validator, _help in FIELDS}
STRUCT_FMT = "<" + "".join(KIND_TO_CODE[kind] for _name, kind, *_rest in FIELDS)
BODY_SIZE = struct.calcsize(STRUCT_FMT)

if BODY_SIZE != 22:
    raise RuntimeError(f"Config_body layout drifted: expected 22 bytes, got {BODY_SIZE}")


def validate_config(config):
    """Raise ValueError unless config is one complete valid version-5 body."""
    missing = [name for name in FIELD_NAMES if name not in config]
    if missing:
        raise ValueError(f"Missing config field: {missing[0]}")

    extras = [name for name in config if name not in FIELD_BY_NAME]
    if extras:
        raise ValueError(f"Unknown config field: {extras[0]}")

    for name, kind, _default, validator, helptext in FIELDS:
        value = config[name]
        if kind == "u8" and not isinstance(value, int):
            raise ValueError(f"Invalid {name}: expected integer {helptext}")
        try:
            valid = validator(value)
        except (TypeError, ValueError, OverflowError):
            valid = False
        if not valid:
            raise ValueError(f"Invalid {name}: {value!r}; expected {helptext}")


def pack_config(config):
    """Pack one complete validated Config_body in canonical field order."""
    validate_config(config)
    return struct.pack(STRUCT_FMT, *(config[name] for name in FIELD_NAMES))


def unpack_config(body):
    """Unpack and validate one exact Config_body."""
    body = bytes(body)
    if len(body) != BODY_SIZE:
        raise ValueError(f"Config body must be exactly {BODY_SIZE} bytes, got {len(body)}")
    config = dict(zip(FIELD_NAMES, struct.unpack(STRUCT_FMT, body)))
    validate_config(config)
    return config


def is_gamepad_hid(devinfo):
    return (
        devinfo.get("usage_page") == HID_USAGE_PAGE_GENERIC_DESKTOP
        and devinfo.get("usage") == HID_USAGE_GAMEPAD
    )


def fmt_hex(value):
    if value is None:
        return "?"
    return f"0x{int(value):04X}"


def describe_hid(devinfo):
    return (
        f"pid={fmt_hex(devinfo.get('product_id'))}, "
        f"interface={devinfo.get('interface_number', '?')}, "
        f"usage_page={fmt_hex(devinfo.get('usage_page'))}, "
        f"usage={fmt_hex(devinfo.get('usage'))}, "
        f"product={devinfo.get('product_string') or '?'}"
    )


def open_device():
    hid = _load_hid()
    candidates = [
        device
        for device in hid.enumerate(VID)
        if device.get("product_id") in PIDS
    ]
    if not candidates:
        sys.exit(
            "No DualSense / ds5dongle found (VID 054C, PID 0CE6/0DF2). "
            "Close Steam/DSX if they are holding the device."
        )

    gamepads = [device for device in candidates if is_gamepad_hid(device)]
    if not gamepads:
        detail = "\n".join(f"  {describe_hid(device)}" for device in candidates)
        sys.exit(
            "Found DualSense / ds5dongle HID device(s), but none were the Game Pad "
            "interface (usage_page=0x0001, usage=0x0005). Wake adds a keyboard HID; "
            "this tool only opens the gamepad.\n" + detail
        )

    device = hid.device()
    device.open_path(gamepads[0]["path"])
    return device


def _feature_payload(data, report_id):
    raw = bytes(data or b"")
    if raw and raw[0] == report_id:
        return raw[1:]
    return raw


def read_config(device):
    """Read, decode, and schema-check the current version-5 configuration."""
    try:
        data = device.get_feature_report(REPORT_GET_CONFIG, FEATURE_REPORT_LEN)
    except OSError as exc:
        sys.exit(f"Failed reading config report 0x{REPORT_GET_CONFIG:02X}: {exc}")
    if not data:
        sys.exit("Empty response reading config (report 0xF7). Is the firmware current?")

    payload = _feature_payload(data, REPORT_GET_CONFIG)
    if len(payload) < BODY_SIZE:
        sys.exit(f"Short config read: got {len(payload)} bytes, expected {BODY_SIZE}.")
    try:
        return unpack_config(payload[:BODY_SIZE])
    except ValueError as exc:
        sys.exit(f"Invalid config report: {exc}")


def read_version(device):
    try:
        data = device.get_feature_report(REPORT_GET_VERSION, FEATURE_REPORT_LEN)
    except OSError:
        return ""
    raw = _feature_payload(data, REPORT_GET_VERSION)
    return raw.split(b"\x00", 1)[0].decode("ascii", "replace").strip()


def build_set_report(funcid, payload=b""):
    """Build one descriptor-sized SET feature report including report ID."""
    data = bytes([funcid]) + bytes(payload)
    if len(data) > SET_DATA_LEN:
        raise ValueError(
            f"SET payload is too large: {len(data)} bytes, maximum is {SET_DATA_LEN}"
        )
    report = bytes([REPORT_SET]) + data.ljust(SET_DATA_LEN, b"\x00")
    if len(report) != FEATURE_REPORT_LEN:
        raise RuntimeError("SET feature report must be exactly 64 bytes")
    return report


def write_config(device, config, save):
    device.send_feature_report(build_set_report(FUNC_UPDATE, pack_config(config)))
    if save:
        device.send_feature_report(build_set_report(FUNC_SAVE))


def fmt_value(name, value):
    if name == "haptics_gain":
        return f"{value:.3f}"
    return str(value)


def print_config(config):
    width = max(len(name) for name in FIELD_NAMES)
    for name, _kind, _default, _validator, helptext in FIELDS:
        print(f"  {name:<{width}} = {fmt_value(name, config[name]):<8}  # {helptext}")


def parse_assignment(token):
    if "=" not in token:
        sys.exit(f"Bad assignment '{token}', expected name=value.")
    name, raw = token.split("=", 1)
    name = name.strip()
    if name not in FIELD_BY_NAME:
        sys.exit(f"Unknown field '{name}'. Run 'config_tool.py fields' to list them.")
    if name == "config_version":
        sys.exit("config_version is managed by the firmware and cannot be set.")

    _name, kind, _default, validator, helptext = FIELD_BY_NAME[name]
    try:
        value = float(raw) if kind == "float" else int(raw, 0)
    except ValueError:
        sys.exit(f"Bad value '{raw}' for {name}.")
    try:
        valid = validator(value)
    except (TypeError, ValueError, OverflowError):
        valid = False
    if not valid:
        sys.exit(f"Value {raw} out of range for {name} (expected {helptext}).")
    return name, value


def cmd_fields(_args):
    width = max(len(name) for name in FIELD_NAMES)
    print(f"Config_body ({BODY_SIZE} bytes, schema version {CONFIG_VERSION}):")
    for name, kind, _default, _validator, helptext in FIELDS:
        read_only = " (read-only)" if name == "config_version" else ""
        print(f"  {name:<{width}} {kind:<6} {helptext}{read_only}")


def cmd_get(_args):
    device = open_device()
    try:
        version = read_version(device)
        config = read_config(device)
    finally:
        device.close()
    if version:
        print(f"Firmware: {version}")
    print("Config:")
    print_config(config)


def cmd_set(args):
    updates = dict(parse_assignment(token) for token in args.assignments)
    if not updates:
        sys.exit("Nothing to set. Pass one or more name=value pairs.")

    device = open_device()
    try:
        config = read_config(device)
        updated = dict(config)
        updated.update(updates)
        write_config(device, updated, save=not args.no_save)
        read_back = read_config(device)
    finally:
        device.close()

    print("Updated:" + ("" if args.no_save else " (saved to device)"))
    for name in updates:
        print(f"  {name} -> {fmt_value(name, read_back[name])}")

    for name, requested in updates.items():
        actual = read_back[name]
        adjusted = (
            abs(actual - requested) > 1e-6
            if isinstance(requested, float)
            else actual != requested
        )
        if adjusted:
            print(
                f"  note: {name} was normalized by firmware to "
                f"{fmt_value(name, actual)}"
            )


def main():
    parser = argparse.ArgumentParser(
        description="Read and modify ds5dongle config over USB HID."
    )
    subcommands = parser.add_subparsers(dest="command", required=True)

    subcommands.add_parser(
        "get", help="read and print the current config"
    ).set_defaults(func=cmd_get)
    subcommands.add_parser(
        "fields", help="list configurable fields and ranges"
    ).set_defaults(func=cmd_fields)

    set_parser = subcommands.add_parser(
        "set", help="set one or more fields (name=value ...)"
    )
    set_parser.add_argument("assignments", nargs="+", metavar="name=value")
    set_parser.add_argument(
        "--no-save",
        action="store_true",
        help="update the current value only; do not persist it to device NVS",
    )
    set_parser.set_defaults(func=cmd_set)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

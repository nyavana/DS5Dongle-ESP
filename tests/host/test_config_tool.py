import builtins
import contextlib
import importlib.util
import io
import math
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = PROJECT_ROOT / "tools" / "config_tool.py"


def load_tool(module_name="ds5_config_tool_under_test"):
    spec = importlib.util.spec_from_file_location(module_name, TOOL_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


tool = load_tool()


EXPECTED_FIELD_NAMES = (
    "config_version",
    "haptics_gain",
    "speaker_volume",
    "headset_volume",
    "speaker_gain",
    "inactive_time",
    "disable_pico_led",
    "polling_rate_mode",
    "audio_buffer_length",
    "controller_mode",
    "enable_usb_sn",
    "ps_shortcut_enabled",
    "mic_select",
    "speaker_select",
    "enable_wake",
    "trigger_reduce",
    "lock_volume",
    "status_gpio_pin",
    "status_gpio_mode",
)

EXPECTED_DEFAULT_BYTES = bytes.fromhex(
    "05 00 00 80 3f 64 64 02 1e 00 01 30 02 00 00 00 00 00 00 00 ff 00"
)


class FakeDevice:
    def __init__(self, config_reports=(), version_reports=()):
        self.config_reports = list(config_reports)
        self.version_reports = list(version_reports)
        self.reads = []
        self.writes = []
        self.opened_path = None
        self.closed = False

    def open_path(self, path):
        self.opened_path = path

    def get_feature_report(self, report_id, length):
        self.reads.append((report_id, length))
        reports = (
            self.config_reports
            if report_id == tool.REPORT_GET_CONFIG
            else self.version_reports
        )
        if not reports:
            return []
        report = reports.pop(0)
        if isinstance(report, BaseException):
            raise report
        return report

    def send_feature_report(self, report):
        self.writes.append(bytes(report))
        return len(report)

    def close(self):
        self.closed = True


class FakeHidModule:
    def __init__(self, devices, opened_device):
        self.devices = devices
        self.opened_device = opened_device
        self.enumerated_vid = None

    def enumerate(self, vid):
        self.enumerated_vid = vid
        return list(self.devices)

    def device(self):
        return self.opened_device


class ConfigLayoutTests(unittest.TestCase):
    def test_field_order_size_and_default_bytes_match_v5_contract(self):
        self.assertEqual(tuple(tool.FIELD_NAMES), EXPECTED_FIELD_NAMES)
        self.assertEqual(tool.STRUCT_FMT, "<Bf" + "B" * 17)
        self.assertEqual(tool.BODY_SIZE, 22)
        self.assertEqual(tool.pack_config(tool.DEFAULT_CONFIG), EXPECTED_DEFAULT_BYTES)

    def test_pack_unpack_round_trip_uses_the_ordered_field_table(self):
        config = dict(tool.DEFAULT_CONFIG)
        config.update(
            haptics_gain=1.75,
            speaker_volume=0,
            headset_volume=127,
            controller_mode=1,
            status_gpio_pin=61,
            status_gpio_mode=1,
        )

        packed = tool.pack_config(config)
        unpacked = tool.unpack_config(packed)

        self.assertEqual(len(packed), tool.BODY_SIZE)
        self.assertEqual(unpacked, config)

    def test_every_field_accepts_its_declared_boundaries(self):
        valid_values = {
            "config_version": (5,),
            "haptics_gain": (1.0, 2.0),
            "speaker_volume": (0, 127),
            "headset_volume": (0, 127),
            "speaker_gain": (0, 7),
            "inactive_time": (0, 60),
            "disable_pico_led": (0, 1),
            "polling_rate_mode": (0, 2),
            "audio_buffer_length": (16, 128),
            "controller_mode": (0, 2),
            "enable_usb_sn": (0, 1),
            "ps_shortcut_enabled": (0, 1),
            "mic_select": (0, 3),
            "speaker_select": (0, 3),
            "enable_wake": (0, 1),
            "trigger_reduce": (0, 10),
            "lock_volume": (0, 1),
            "status_gpio_pin": (0, 255),
            "status_gpio_mode": (0, 1),
        }
        for name, values in valid_values.items():
            for value in values:
                with self.subTest(name=name, value=value):
                    config = dict(tool.DEFAULT_CONFIG)
                    config[name] = value
                    tool.validate_config(config)

    def test_every_field_rejects_out_of_range_values(self):
        invalid_values = {
            "config_version": (4, 6),
            "haptics_gain": (math.nan, math.inf, -math.inf, 0.99, 2.01),
            "speaker_volume": (-1, 128),
            "headset_volume": (-1, 128),
            "speaker_gain": (-1, 8),
            "inactive_time": (-1, 61),
            "disable_pico_led": (-1, 2),
            "polling_rate_mode": (-1, 3),
            "audio_buffer_length": (15, 129),
            "controller_mode": (-1, 3),
            "enable_usb_sn": (-1, 2),
            "ps_shortcut_enabled": (-1, 2),
            "mic_select": (-1, 4),
            "speaker_select": (-1, 4),
            "enable_wake": (-1, 2),
            "trigger_reduce": (-1, 11),
            "lock_volume": (-1, 2),
            "status_gpio_pin": (-1, 256),
            "status_gpio_mode": (-1, 2),
        }
        for name, values in invalid_values.items():
            for value in values:
                with self.subTest(name=name, value=value):
                    config = dict(tool.DEFAULT_CONFIG)
                    config[name] = value
                    with self.assertRaisesRegex(ValueError, name):
                        tool.validate_config(config)

    def test_unpack_rejects_short_body_and_schema_mismatch(self):
        with self.assertRaisesRegex(ValueError, "22 bytes"):
            tool.unpack_config(EXPECTED_DEFAULT_BYTES[:-1])

        wrong_version = bytes([4]) + EXPECTED_DEFAULT_BYTES[1:]
        with self.assertRaisesRegex(ValueError, "schema version 5"):
            tool.unpack_config(wrong_version)

    def test_fields_marks_status_gpio_as_stored_but_runtime_disabled(self):
        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            tool.cmd_fields(None)

        output = stdout.getvalue()
        self.assertIn("status_gpio_pin", output)
        self.assertIn("status_gpio_mode", output)
        self.assertGreaterEqual(output.count("stored only, status output disabled"), 2)


class HidReportTests(unittest.TestCase):
    def test_read_config_accepts_reports_with_or_without_report_id_prefix(self):
        for report in (
            EXPECTED_DEFAULT_BYTES + bytes(41),
            bytes([tool.REPORT_GET_CONFIG]) + EXPECTED_DEFAULT_BYTES + bytes(41),
        ):
            with self.subTest(first_byte=report[0], length=len(report)):
                device = FakeDevice(config_reports=[report])
                self.assertEqual(tool.read_config(device), tool.DEFAULT_CONFIG)
                self.assertEqual(
                    device.reads,
                    [(tool.REPORT_GET_CONFIG, tool.FEATURE_REPORT_LEN)],
                )

    def test_read_config_rejects_empty_short_and_wrong_version_reports(self):
        wrong_version = bytes([4]) + EXPECTED_DEFAULT_BYTES[1:]
        cases = (
            (b"", "Empty response"),
            (EXPECTED_DEFAULT_BYTES[:-1], "Short config read"),
            (wrong_version, "schema version 5"),
        )
        for report, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(SystemExit, message):
                    tool.read_config(FakeDevice(config_reports=[report]))

    def test_read_version_accepts_reports_with_or_without_report_id_prefix(self):
        for report in (
            b"v5.2.1\x00ignored",
            bytes([tool.REPORT_GET_VERSION]) + b"v5.2.1\x00ignored",
        ):
            with self.subTest(report=report):
                device = FakeDevice(version_reports=[report])
                self.assertEqual(tool.read_version(device), "v5.2.1")

    def test_gamepad_filter_selects_only_generic_desktop_gamepad(self):
        self.assertTrue(
            tool.is_gamepad_hid(
                {"usage_page": tool.HID_USAGE_PAGE_GENERIC_DESKTOP,
                 "usage": tool.HID_USAGE_GAMEPAD}
            )
        )
        self.assertFalse(
            tool.is_gamepad_hid(
                {"usage_page": tool.HID_USAGE_PAGE_GENERIC_DESKTOP, "usage": 0x06}
            )
        )
        self.assertFalse(tool.is_gamepad_hid({}))

    def test_open_device_filters_pid_and_interface_before_opening(self):
        opened = FakeDevice()
        hid = FakeHidModule(
            [
                {"product_id": 0x9999, "path": b"wrong-product",
                 "usage_page": 0x01, "usage": 0x05},
                {"product_id": tool.PIDS[0], "path": b"keyboard",
                 "usage_page": 0x01, "usage": 0x06},
                {"product_id": tool.PIDS[1], "path": b"gamepad",
                 "usage_page": 0x01, "usage": 0x05},
            ],
            opened,
        )

        with mock.patch.object(tool, "_load_hid", return_value=hid):
            self.assertIs(tool.open_device(), opened)

        self.assertEqual(hid.enumerated_vid, tool.VID)
        self.assertEqual(opened.opened_path, b"gamepad")

    def test_open_device_rejects_matching_non_gamepad_interfaces(self):
        hid = FakeHidModule(
            [{"product_id": tool.PIDS[0], "path": b"keyboard",
              "interface_number": 2, "usage_page": 0x01, "usage": 0x06,
              "product_string": "Wireless Controller"}],
            FakeDevice(),
        )
        with mock.patch.object(tool, "_load_hid", return_value=hid):
            with self.assertRaisesRegex(SystemExit, "none were the Game Pad interface"):
                tool.open_device()

    def test_import_does_not_load_hidapi(self):
        original_import = builtins.__import__

        def guarded_import(name, *args, **kwargs):
            if name == "hid":
                raise AssertionError("hidapi was imported eagerly")
            return original_import(name, *args, **kwargs)

        with mock.patch("builtins.__import__", side_effect=guarded_import):
            load_tool("ds5_config_tool_lazy_import_test")


class UpdateFlowTests(unittest.TestCase):
    def test_set_preserves_unspecified_fields_reads_back_and_writes_64_bytes(self):
        initial = dict(tool.DEFAULT_CONFIG)
        initial.update(haptics_gain=1.5, headset_volume=17, status_gpio_pin=61)
        expected = dict(initial)
        expected["speaker_volume"] = 90
        device = FakeDevice(
            config_reports=[
                bytes([tool.REPORT_GET_CONFIG]) + tool.pack_config(initial),
                tool.pack_config(expected),
            ]
        )

        stdout = io.StringIO()
        args = SimpleNamespace(assignments=["speaker_volume=90"], no_save=False)
        with mock.patch.object(tool, "open_device", return_value=device):
            with contextlib.redirect_stdout(stdout):
                tool.cmd_set(args)

        self.assertTrue(device.closed)
        self.assertEqual(len(device.reads), 2)
        self.assertEqual(len(device.writes), 2)
        self.assertTrue(all(len(report) == 64 for report in device.writes))
        self.assertEqual(device.writes[0][:2], bytes([tool.REPORT_SET, tool.FUNC_UPDATE]))
        self.assertEqual(
            tool.unpack_config(device.writes[0][2:2 + tool.BODY_SIZE]),
            expected,
        )
        self.assertEqual(device.writes[0][2 + tool.BODY_SIZE:], bytes(40))
        self.assertEqual(device.writes[1][:2], bytes([tool.REPORT_SET, tool.FUNC_SAVE]))
        self.assertEqual(device.writes[1][2:], bytes(62))
        self.assertIn("saved to device", stdout.getvalue())
        self.assertNotIn("flash", stdout.getvalue().lower())

    def test_no_save_still_reads_back_but_sends_only_update(self):
        expected = dict(tool.DEFAULT_CONFIG)
        expected["inactive_time"] = 0
        device = FakeDevice(
            config_reports=[
                tool.pack_config(tool.DEFAULT_CONFIG),
                tool.pack_config(expected),
            ]
        )
        args = SimpleNamespace(assignments=["inactive_time=0"], no_save=True)

        with mock.patch.object(tool, "open_device", return_value=device):
            with contextlib.redirect_stdout(io.StringIO()):
                tool.cmd_set(args)

        self.assertEqual(len(device.writes), 1)
        self.assertEqual(len(device.writes[0]), 64)

    def test_assignment_validation_rejects_schema_and_non_finite_float(self):
        with self.assertRaisesRegex(SystemExit, "cannot be set"):
            tool.parse_assignment("config_version=5")
        for token in ("haptics_gain=nan", "haptics_gain=inf"):
            with self.subTest(token=token):
                with self.assertRaisesRegex(SystemExit, "out of range"):
                    tool.parse_assignment(token)


if __name__ == "__main__":
    unittest.main()

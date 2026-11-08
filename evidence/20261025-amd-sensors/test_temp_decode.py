#!/usr/bin/python3
"""Golden temperatures, unavailable samples and process-level failure checks."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest

from zen5_temp_decode import InvalidSample, decode_snapshot

CPU = {"vendor": "AuthenticAMD", "family": 26, "model": 68}


def snapshot(raw, kind="tctl", **kwargs):
    return {"cpu": CPU.copy(), "samples": [dict(kind=kind, raw=raw,
                                              transport_ok=True, **kwargs)]}


class TemperatureTests(unittest.TestCase):
    def value(self, raw, kind="tctl", **kwargs):
        return decode_snapshot(snapshot(raw, kind, **kwargs))["samples"][0]

    def test_known_tctl_and_flags(self):
        # Literal snapshot vectors: 50 C without offset, 100 - 49 = 51 C.
        for raw, expected in ((0x32000000, 50000), (0x64080000, 51000),
                              (0x64030000, 51000), (0x64010000, 100000),
                              (0x64020000, 100000), (0x640B0000, 51000),
                              (0xFFE00000, 255875)):
            self.assertEqual(self.value(raw)["millidegrees_c"], expected)

    def test_tctl_negative_is_clamped_but_retained(self):
        item = self.value(0x00080000)
        self.assertEqual(item["millidegrees_c"], 0)
        self.assertEqual(item["unclamped_millidegrees_c"], -49000)
        self.assertTrue(item["linux_clamped"])

    def test_known_ccd_and_slots(self):
        # 0xB18: valid + 792 steps, i.e. 50 C; 0x987: -0.125 C.
        self.assertEqual(self.value(0xB18, "tccd", index=0)["millidegrees_c"], 50000)
        self.assertEqual(self.value(0x987, "tccd", index=0)["millidegrees_c"], -125)
        self.assertEqual(self.value(0x800, "tccd", index=0)["millidegrees_c"], -49000)
        self.assertEqual(self.value(0xFFF, "tccd", index=7)["smn_address"], "0x00059b24")
        self.assertEqual(self.value(0xB18, "tccd", index=0)["smn_address"], "0x00059b08")

    def test_all_code_steps_and_flag_equivalence(self):
        previous = None
        for code in range(1, 2048):
            plain = self.value(code << 21)
            if previous is not None:
                self.assertEqual(plain["millidegrees_c"] - previous, 125)
            previous = plain["millidegrees_c"]
            adjusted = self.value((code << 21) | 0x80000)
            self.assertEqual(adjusted["unclamped_millidegrees_c"], previous - 49000)
            self.assertEqual(self.value((code << 21) | 0x30000)["millidegrees_c"],
                             adjusted["millidegrees_c"])
        # CCD preserves negative values and spans exactly 2048 codes.
        values = [self.value(code | 0x800, "tccd", index=0)["millidegrees_c"]
                  for code in range(2048)]
        self.assertEqual((values[0], values[-1]), (-49000, 206875))
        self.assertTrue(all(b - a == 125 for a, b in zip(values, values[1:])))

    def test_missing_and_failed_transport(self):
        for flag in (False, None, 1, "true"):
            data = snapshot(0x32000000)
            data["samples"][0]["transport_ok"] = flag
            with self.assertRaises(InvalidSample):
                decode_snapshot(data)

    def test_invalid_registers_never_become_temperatures(self):
        for raw in (0, 0xFFFFFFFF, -1, 0x100000000, True, "0x32000000", None):
            with self.assertRaises(InvalidSample):
                self.value(raw)
        with self.assertRaises(InvalidSample):
            self.value(0x318, "tccd", index=0)  # Plausible bits, valid bit absent.
        for index in (-1, 8, True, None):
            with self.assertRaises(InvalidSample):
                self.value(0xB18, "tccd", index=index)

    def test_identity_and_shape(self):
        for field, value in (("vendor", "GenuineIntel"), ("family", 25),
                             ("model", 36), ("model", True), ("family", "26")):
            data = copy.deepcopy(snapshot(0x32000000))
            data["cpu"][field] = value
            with self.assertRaises(InvalidSample):
                decode_snapshot(data)
        for data in (None, [], {}, {"cpu": CPU, "samples": []},
                     {"cpu": CPU, "samples": [None]}):
            with self.assertRaises(InvalidSample):
                decode_snapshot(data)
        with self.assertRaises(InvalidSample):
            self.value(0x32000000, "tdie")

    def test_cli_and_explicit_offline_label(self):
        script = str(Path(__file__).with_name("zen5_temp_decode.py"))
        result = subprocess.run([sys.executable, script, "-"], input=json.dumps(
            snapshot(0x32000000)), text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        decoded = json.loads(result.stdout)
        self.assertFalse(decoded["hardware_access"])
        self.assertFalse(decoded["measurement_verified"])
        bad = subprocess.run([sys.executable, script, "-"], input='{',
                             text=True, capture_output=True)
        self.assertEqual(bad.returncode, 2)
        self.assertEqual(json.loads(bad.stderr)["status"], "INVALID")
        failed = snapshot(0x32000000)
        failed["samples"][0]["transport_ok"] = False
        bad = subprocess.run([sys.executable, script, "-"], input=json.dumps(failed),
                             text=True, capture_output=True)
        self.assertEqual(bad.returncode, 2)
        self.assertEqual(bad.stdout, "")


if __name__ == "__main__":
    unittest.main(verbosity=2)

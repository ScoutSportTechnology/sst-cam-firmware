#!/usr/bin/env python3
"""Fail-closed checks for the 90-minute stress evidence (synthetic, no hardware)."""
import importlib.util
import json
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('stress_evaluate', Path(__file__).with_name('stress-evaluate.py'))
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class StressEvaluateTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.mode = self.root / 'veryfast'
        self.mode.mkdir()
        start = datetime(2026, 11, 19, tzinfo=timezone.utc)
        (self.mode / 'e2e.log').write_text(''.join(
            f'minute={minute} elapsed_s={minute * 60 + 5} capture_fps=30,30 paired_pct=100 '
            'render_fps=30 render_ms_mean=20 infer_hz=15 infer_batches=900 analysis_skipped=0 '
            'infer_p99_ms=30 mp4_fps=30 rtmp_fps=30 no_drop=PASS\n'
            for minute in (1, 2)) + 'final acceptance=PASS\n')
        (self.mode / 'cores.log').write_text(''.join(
            f'{(start + timedelta(seconds=second)).isoformat()}\n'
            f'cpu0 {40 * second} 0 0 {60 * second} 0 0 0 0 0 0\n'
            f'cpu1 {30 * second} 0 0 {70 * second} 0 0 0 0 0 0\n'
            for second in range(121)))
        (self.mode / 'monitor.log').write_text(''.join(
            f'{(start + timedelta(seconds=second)).isoformat()} MemAvailable_kib=3000000 pid\n'
            for second in range(121)))
        (self.mode / 'tegrastats.log').write_text(''.join(
            f'11-19-2026 12:{second // 60:02d}:{second % 60:02d} RAM 2000/7485MB '
            f'GR3D_FREQ 55% tj@{55 + second / 120:.2f}C/60C\n'
            for second in range(121)))
        (self.mode / 'kernel.log').write_text('normal kernel message\n')
        (self.mode / 'thermal-trips.log').write_text(
            'zone=thermal_zone0 type=CPU-therm\ntrip_point_0_type=passive temperature_millic=99000\n'
            'zone=thermal_zone1 type=GPU-therm\ntrip_point_0_type=passive temperature_millic=99000\n')
        (self.mode / 'power-mode.log').write_text('NV Power Mode: MAXN\n')
        (self.root / 'veryfast-viewer.log').write_text(json.dumps({
            'decoded_fps': 30, 'pts_fps': 30, 'decoded_frames': 3600,
            'elapsed_s': 120, 'full_window': True, 'missed_segments': 0,
            'pts_discontinuities': 0}))
        (self.root / 'veryfast-mp4.json').write_text(json.dumps({
            'width': 1920, 'height': 1080, 'nb_read_frames': 3600,
            'duration': 120, 'measured_fps': 30}))

    def evaluate(self):
        return module.summarize(self.root, 'veryfast', minutes=2)

    def test_pass_requires_all_independent_evidence(self):
        result = self.evaluate()
        self.assertEqual(result['status'], 'PASS', result)
        self.assertEqual(result['cpu_per_core']['cpu0']['mean_pct'], 40)
        self.assertEqual(result['gpu']['p95_pct'], 55)
        self.assertTrue(result['headroom'])

    def test_first_window_cannot_be_startup_period(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('minute=1 elapsed_s=65', 'minute=1 elapsed_s=60'))
        self.assertFalse(self.evaluate()['limits']['all_minutes'])

    def test_4k_container_is_not_the_rendered_1080p_recording(self):
        mp4 = self.root / 'veryfast-mp4.json'
        mp4.write_text(mp4.read_text().replace('"width": 1920, "height": 1080',
                                                '"width": 3840, "height": 2160'))
        self.assertFalse(self.evaluate()['limits']['mp4'])

    def test_invalid_extra_minute_cannot_be_smuggled_past_final_acceptance(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('final acceptance=PASS\n',
            'minute=3 elapsed_s=185 no_drop=FAIL\nfinal acceptance=PASS\n'))
        self.assertFalse(self.evaluate()['limits']['final_acceptance'])

    def test_90th_minute_is_still_required_when_tail_exists(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('minute=2 ', 'minute=3 '))
        self.assertFalse(self.evaluate()['limits']['all_minutes'])

    def test_terminal_window_must_reach_its_end_even_with_extra_evidence(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('minute=2 elapsed_s=125',
                                                'minute=2 elapsed_s=122'))
        self.assertFalse(self.evaluate()['limits']['all_minutes'])

    def test_extra_trailing_minute_is_allowed_after_full_window(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('final acceptance=PASS\n',
            'minute=3 elapsed_s=185 capture_fps=30,30 paired_pct=100 render_fps=30 '
            'render_ms_mean=20 infer_hz=15 infer_batches=900 analysis_skipped=0 '
            'infer_p99_ms=30 mp4_fps=30 rtmp_fps=30 no_drop=PASS\nfinal acceptance=PASS\n'))
        self.assertTrue(self.evaluate()['limits']['all_minutes'])
        self.assertEqual(self.evaluate()['status'], 'PASS')

    def test_missing_or_short_minute_cannot_pass(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('minute=2 ', 'minute=3 '))
        result = self.evaluate()
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['limits']['all_minutes'])

    def test_one_bad_minute_cannot_hide_in_average(self):
        log = self.mode / 'e2e.log'
        log.write_text(log.read_text().replace('infer_batches=900 analysis_skipped=0',
                                               'infer_batches=899 analysis_skipped=1', 1))
        result = self.evaluate()
        self.assertFalse(result['limits']['all_minutes'])
        self.assertEqual(result['bad_minutes'], [1])

    def test_missing_telemetry_fails_closed(self):
        (self.mode / 'tegrastats.log').unlink()
        result = self.evaluate()
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['limits']['gpu'])
        self.assertFalse(result['headroom'])

    def test_thermal_throttle_and_resource_saturation_are_rejected(self):
        (self.mode / 'kernel.log').write_text('thermal throttling triggered\n')
        tegra = self.mode / 'tegrastats.log'
        tegra.write_text(tegra.read_text().replace('GR3D_FREQ 55%', 'GR3D_FREQ 99%'))
        result = self.evaluate()
        self.assertFalse(result['limits']['no_throttling'])
        self.assertFalse(result['limits']['gpu'])
        self.assertFalse(result['headroom'])

    def test_viewer_and_mp4_must_cover_full_window(self):
        viewer = self.root / 'veryfast-viewer.log'
        viewer.write_text(viewer.read_text().replace('"missed_segments": 0', '"missed_segments": 1'))
        mp4 = self.root / 'veryfast-mp4.json'
        mp4.write_text(mp4.read_text().replace('"duration": 120', '"duration": 110'))
        result = self.evaluate()
        self.assertFalse(result['limits']['viewer'])
        self.assertFalse(result['limits']['mp4'])

    def test_board_trip_determines_thermal_headroom(self):
        trip = self.mode / 'thermal-trips.log'
        trip.write_text(trip.read_text().replace('99000', '56000'))
        result = self.evaluate()
        self.assertEqual(result['thermal']['trip_c'], 56)
        self.assertFalse(result['limits']['thermal'])
        self.assertFalse(result['headroom'])

    def test_missing_board_trip_is_unknown_not_safe(self):
        (self.mode / 'thermal-trips.log').unlink()
        result = self.evaluate()
        self.assertEqual(result['thermal']['trip_c'], None)
        self.assertFalse(result['limits']['thermal'])

    def test_hot_gpu_with_only_cpu_trip_is_unknown(self):
        trip = self.mode / 'thermal-trips.log'
        trip.write_text('zone=thermal_zone0 type=CPU-therm\n'
                        'trip_point_0_type=passive temperature_millic=99000\n')
        result = self.evaluate()
        self.assertIsNone(result['thermal']['trip_c'])
        self.assertFalse(result['limits']['thermal'])

    def test_missing_power_mode_is_not_complete_telemetry(self):
        (self.mode / 'power-mode.log').unlink()
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])

    def test_missing_kernel_log_is_not_no_throttling(self):
        (self.mode / 'kernel.log').unlink()
        self.assertFalse(self.evaluate()['limits']['no_throttling'])


if __name__ == '__main__':
    unittest.main()

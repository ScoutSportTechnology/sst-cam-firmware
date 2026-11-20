#!/usr/bin/env python3
"""Offline v2 stress evidence checks; synthetic full-duration logs, no device."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('stress_evaluate_v2', Path(__file__).with_name('stress-evaluate-v2.py'))
evaluator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(evaluator)


class StressEvaluateV2Test(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.make_trial('run-a', minutes=2, overlay='off', encoders='shared')

    def make_trial(self, trial, minutes=1, overlay='on', encoders='independent'):
        directory = self.root / trial
        directory.mkdir(parents=True, exist_ok=True)
        start = datetime(2026, 11, 19, 12, tzinfo=timezone.utc)
        seconds = minutes * 60
        preset = 'veryfast' if overlay == 'off' else trial.rsplit('/', 1)[-1]
        (directory / 'e2e.log').write_text(
            f'encoders={encoders} preset=veryfast overlay={overlay} overlay_preset={preset if overlay == "on" else "none"}\n'
            + ''.join(
                f'minute={minute} elapsed_s={minute * 60 + 5} capture_fps=30,30 '
                'paired_pct=100 render_fps=30 infer_hz=15 infer_batches=900 '
                'mp4_fps=30 rtmp_fps=30 no_drop=PASS timestamp_gaps=0 '
                'capture_id_gaps=0 capture_drops=0,0 analysis_skipped=0\n'
                for minute in range(1, minutes + 1))
            + 'window=steady timestamp_gaps=0 capture_id_gaps=0 internal_gaps=0 '
              'reordered=0 queue_drops=0 unmatched=0 render_misses=0 analysis_skipped=0 no_drop=PASS\n'
            + f'final elapsed_s={seconds + 5} encoded_frames={seconds * 30} '
              f'rtmp_input_frames={seconds * 30} rtmp_frames={seconds * 30} '
              f'render={seconds * 30} encoder_queue_or_branch_gap=0 '
              f'mp4_frames={seconds * 30} mp4_branch_frames={seconds * 30} '
              f'rtmp_branch_frames={seconds * 30} clean_encoder_in={seconds * 30} '
              f'overlay_encoder_in={seconds * 30 if overlay == "on" else 0} '
              'encoder_queue_drops=0 mp4_branch_drops=0 rtmp_branch_drops=0 acceptance=PASS\n')
        (directory / 'cores.log').write_text(''.join(
            f'{(start + timedelta(seconds=s)).isoformat()}\n'
            f'cpu0 {90 * s} 0 0 {10 * s} 0 0 0 0 0 0\n'
            f'cpu1 {30 * s} 0 0 {70 * s} 0 0 0 0 0 0\n'
            + ''.join(f'cpu{core} {20 * s} 0 0 {80 * s} 0 0 0 0 0 0\n'
                      for core in range(2, 6))
            for s in range(seconds + 6)))
        (directory / 'monitor.log').write_text(''.join(
            f'{(start + timedelta(seconds=s)).isoformat()} MemAvailable_kib=2000000 pid\n'
            for s in range(seconds + 6)))
        (directory / 'tegrastats.log').write_text(''.join(
            f'{(start + timedelta(seconds=s)).strftime("%m-%d-%Y %H:%M:%S")} '
            f'GR3D_FREQ 55% tj@{69 if s < 20 else 71}C/75C\n'
            for s in range(seconds + 6)))
        (directory / 'thermal-trips.log').write_text(
            'zone=thermal_zone0 type=GPU-therm\n'
            'trip_point_0_type=passive temperature_millic=70000\n'
            'trip_point_1_type=critical temperature_millic=99000\n')
        (directory / 'kernel.log').write_text('normal kernel message\n')
        (directory / 'power-mode.log').write_text('NV Power Mode: MAXN\n')
        (directory / 'baseline.log').write_text('nvpmodel:\nNV Power Mode: MAXN\njetson_clocks:\nClocks: disabled\nfan:\n/sys/class/hwmon/hwmon0/fan1_input=2400\nambient=unknown (no calibrated ambient probe supplied)\nonline_cores=cpu0,cpu1,cpu2,cpu3,cpu4,cpu5\n')
        prefix = trial.replace('/', '-')
        (self.root / f'{prefix}-mp4.json').write_text(json.dumps({
            'width': 1920, 'height': 1080, 'nb_read_frames': seconds * 30,
            'duration': seconds + 5, 'measured_fps': 30}))
        (self.root / f'{prefix}-viewer.log').write_text(json.dumps({
            'decoded_fps': 30, 'pts_fps': 30, 'decoded_frames': seconds * 30,
            'elapsed_s': seconds, 'full_window': True, 'missed_segments': 0,
            'pts_discontinuities': 0}))

    def evaluate(self, trial='run-a', minutes=2):
        return evaluator.summarize(self.root, trial, minutes=minutes)

    def test_clean_one_encode_passes_even_if_passive_trip_crossed_and_cpu_is_90pct(self):
        result = self.evaluate()
        self.assertEqual(result['status'], 'PASS', result)
        self.assertEqual(result['cpu_per_core']['cpu0']['mean_pct'], 90)
        self.assertEqual(result['cpu_per_core']['cpu0']['p95_margin_pct'], 10)
        self.assertEqual(result['gpu']['p95_pct'], 55)
        self.assertEqual(result['gpu']['p95_margin_pct'], 45)
        self.assertEqual(result['thermal']['passive_trip_c'], 70)
        self.assertGreater(result['thermal']['passive_crossings'], 0)
        self.assertEqual(result['lost_frames'], {'mp4': 0, 'rtmp': 0})
        self.assertIn('fan', result['baseline'])
        self.assertIn('ambient=unknown', result['baseline'])

    def test_eos_branch_count_and_independent_decode_must_match_render(self):
        log = self.root / 'run-a/e2e.log'
        text = log.read_text()
        for old, new in (('rtmp_frames=3600', 'rtmp_frames=3599'),
                         ('encoded_frames=3600', 'encoded_frames=3599'),
                         ('rtmp_input_frames=3600', 'rtmp_input_frames=3599')):
            with self.subTest(counter=old):
                log.write_text(text.replace(old, new))
                result = self.evaluate()
                self.assertEqual(result['status'], 'FAIL')
                self.assertFalse(result['limits']['eos_parity'])
        log.write_text(text)
        mp4 = self.root / 'run-a-mp4.json'
        mp4.write_text(mp4.read_text().replace('"nb_read_frames": 3600', '"nb_read_frames": 3599'))
        self.assertFalse(self.evaluate()['limits']['mp4_decode_parity'])

    def test_missing_or_duplicate_minute_fails_closed(self):
        log = self.root / 'run-a/e2e.log'
        text = log.read_text()
        log.write_text(text.replace('minute=2 ', 'minute=1 '))
        result = self.evaluate()
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['limits']['all_minutes'])

    def test_each_minute_checks_both_outputs_capture_and_detection_batch_tolerance(self):
        log = self.root / 'run-a/e2e.log'
        text = log.read_text()
        for old, new in (('mp4_fps=30', 'mp4_fps=29.499'),
                         ('rtmp_fps=30', 'rtmp_fps=29.499'),
                         ('capture_fps=30,30', 'capture_fps=29.499,30'),
                         ('infer_batches=900', 'infer_batches=898'),
                         ('timestamp_gaps=0', 'timestamp_gaps=1'),
                         ('capture_id_gaps=0', 'capture_id_gaps=1')):
            with self.subTest(field=old):
                log.write_text(text.replace(old, new, 1))
                self.assertFalse(self.evaluate()['limits']['all_minutes'])
        log.write_text(text.replace('infer_batches=900', 'infer_batches=899', 1))
        self.assertEqual(self.evaluate()['status'], 'PASS')

    def test_tee_branch_frames_and_clean_encoder_input_must_match_at_eos(self):
        log = self.root / 'run-a/e2e.log'
        original = log.read_text()
        for key in ('mp4_branch_frames', 'rtmp_branch_frames', 'clean_encoder_in'):
            with self.subTest(counter=key):
                log.write_text(original.replace(f'{key}=3600', f'{key}=3599'))
                self.assertFalse(self.evaluate()['limits']['eos_parity'])
        log.write_text(original.replace('mp4_branch_drops=0', 'mp4_branch_drops=1'))
        self.assertFalse(self.evaluate()['limits']['eos_parity'])
        log.write_text(original.replace('rtmp_branch_frames=3600 ', ''))
        self.assertFalse(self.evaluate()['limits']['eos_parity'])

    def test_missing_per_minute_gap_evidence_cannot_pass_via_no_drop_flag(self):
        log = self.root / 'run-a/e2e.log'
        log.write_text(log.read_text().replace('timestamp_gaps=0 ', '', 1))
        self.assertFalse(self.evaluate()['limits']['all_minutes'])

    def test_missing_telemetry_middle_minute_fails_despite_large_total_samples(self):
        path = self.root / 'run-a/tegrastats.log'
        path.write_text(''.join(line for line in path.read_text().splitlines(keepends=True)
                                if not line.startswith('11-19-2026 12:01:')))
        result = self.evaluate()
        self.assertFalse(result['limits']['telemetry_coverage'])
        self.assertEqual(result['status'], 'FAIL')

    def test_full_ninety_minute_evidence_and_middle_gap(self):
        self.make_trial('run-a', minutes=90, overlay='off', encoders='shared')
        self.assertEqual(self.evaluate(minutes=90)['status'], 'PASS')
        path = self.root / 'run-a/monitor.log'
        path.write_text(''.join(line for line in path.read_text().splitlines(keepends=True)
                                if not line.startswith('2026-11-19T12:25:')))
        self.assertFalse(self.evaluate(minutes=90)['limits']['telemetry_coverage'])

    def test_missing_cpu_core_feed_does_not_look_like_full_per_core_evidence(self):
        path = self.root / 'run-a/cores.log'
        text = path.read_text()
        path.write_text(''.join(line for line in text.splitlines(keepends=True)
                                if not line.startswith('cpu1 ')))
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])
        path.write_text(''.join(line for line in text.splitlines(keepends=True)
                                if not line.startswith('cpu0 ')))
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])
        path.write_text(''.join(line for line in text.splitlines(keepends=True)
                                if not line.startswith('cpu5 ')))
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])

    def test_baseline_core_list_missing_middle_core_fails_closed(self):
        baseline = self.root / 'run-a/baseline.log'
        baseline.write_text(baseline.read_text().replace(
            'online_cores=cpu0,cpu1,cpu2,cpu3,cpu4,cpu5',
            'online_cores=cpu0,cpu1,cpu2,cpu4,cpu5'))
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])

    def test_failed_power_clock_or_fan_probe_fails_closed(self):
        baseline = self.root / 'run-a/baseline.log'
        original = baseline.read_text()
        for key in ('nvpmodel', 'jetson_clocks', 'fan'):
            with self.subTest(probe=key):
                baseline.write_text(original + f'{key}=unknown (probe failed)\n')
                self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])

    def test_missing_online_core_provenance_fails_closed(self):
        baseline = self.root / 'run-a/baseline.log'
        baseline.write_text(baseline.read_text().replace(
            'online_cores=cpu0,cpu1,cpu2,cpu3,cpu4,cpu5\n', ''))
        self.assertFalse(self.evaluate()['limits']['telemetry_coverage'])

    def test_minute_infer_batch_tolerance_uses_actual_elapsed_interval(self):
        log = self.root / 'run-a/e2e.log'
        text = log.read_text()
        log.write_text(text.replace('minute=1 elapsed_s=65', 'minute=1 elapsed_s=65.7')
                           .replace('infer_batches=900', 'infer_batches=909', 1))
        self.assertEqual(self.evaluate()['status'], 'PASS')
        log.write_text(log.read_text().replace('infer_batches=909', 'infer_batches=908', 1))
        self.assertFalse(self.evaluate()['limits']['all_minutes'])

    def test_missing_evidence_and_wrong_mode_fail_closed(self):
        for filename, limit in (('baseline.log', 'telemetry_coverage'),
                                ('power-mode.log', 'telemetry_coverage'),
                                ('kernel.log', 'no_throttling')):
            with self.subTest(file=filename):
                path = self.root / 'run-a' / filename
                original = path.read_text()
                path.unlink()
                self.assertFalse(self.evaluate()['limits'][limit])
                path.write_text(original)
        log = self.root / 'run-a/e2e.log'
        log.write_text(log.read_text().replace('encoders=shared', 'encoders=independent'))
        self.assertFalse(self.evaluate()['limits']['encoder_configuration'])

    def test_run_b_is_assessed_independently_when_run_a_fails(self):
        self.make_trial('run-b/superfast', minutes=1)
        log = self.root / 'run-a/e2e.log'
        log.write_text(log.read_text().replace('minute=1 elapsed_s=65', 'minute=1 elapsed_s=50'))
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'FAIL')
        self.assertEqual(report['trials']['run-a']['status'], 'FAIL')
        self.assertEqual(report['trials']['run-b/superfast']['status'], 'PASS')
        self.assertEqual(report['holding_preset'], 'superfast')

    def test_overlay_two_encode_trial_and_fallback_report(self):
        self.make_trial('run-b/superfast', minutes=1)
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'PASS', report)
        self.assertEqual(report['holding_preset'], 'superfast')
        log = self.root / 'run-b/superfast/e2e.log'
        log.write_text(log.read_text().replace('rtmp_fps=30', 'rtmp_fps=28', 1))
        self.make_trial('run-b/ultrafast', minutes=1)
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'PASS', report)
        self.assertEqual(report['holding_preset'], 'ultrafast')
        self.assertEqual(report['trials']['run-b/superfast']['status'], 'FAIL')
        self.assertEqual(report['trials']['run-b/ultrafast']['lost_frames']['rtmp'], 0)

    def test_missing_superfast_minute_field_cannot_be_called_measured_fallback(self):
        self.make_trial('run-b/superfast', minutes=1)
        self.make_trial('run-b/ultrafast', minutes=1)
        path = self.root / 'run-b/superfast/e2e.log'
        path.write_text(path.read_text().replace('timestamp_gaps=0 ', '', 1))
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'NO_COMPLETE_EVIDENCE')

    def test_truncated_superfast_run_does_not_qualify_as_fallback_comparison(self):
        self.make_trial('run-b/superfast', minutes=1)
        self.make_trial('run-b/ultrafast', minutes=2)
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=2)
        self.assertEqual(report['status'], 'NO_COMPLETE_EVIDENCE')
        self.assertIsNone(report['holding_preset'])

    def test_no_superfast_evidence_does_not_allow_only_ultrafast_to_pass(self):
        self.make_trial('run-b/ultrafast', minutes=1)
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'NO_COMPLETE_EVIDENCE')

    def test_incomplete_superfast_telemetry_cannot_be_hidden_by_ultrafast_fallback(self):
        self.make_trial('run-b/superfast', minutes=1)
        self.make_trial('run-b/ultrafast', minutes=1)
        (self.root / 'run-b/superfast/tegrastats.log').unlink()
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'NO_COMPLETE_EVIDENCE')
        self.assertIsNone(report['holding_preset'])

    def test_measured_superfast_lost_frame_permits_fallback_with_frame_count_report(self):
        self.make_trial('run-b/superfast', minutes=1)
        self.make_trial('run-b/ultrafast', minutes=1)
        path = self.root / 'run-b/superfast/e2e.log'
        path.write_text(path.read_text().replace('rtmp_frames=1800', 'rtmp_frames=1799'))
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'PASS', report)
        self.assertEqual(report['holding_preset'], 'ultrafast')
        self.assertEqual(report['trials']['run-b/superfast']['lost_frames']['rtmp'], 1)

    def test_measured_mp4_loss_permits_fallback_when_decode_confirms_the_loss(self):
        self.make_trial('run-b/superfast', minutes=1)
        self.make_trial('run-b/ultrafast', minutes=1)
        path = self.root / 'run-b/superfast/e2e.log'
        path.write_text(path.read_text().replace('encoded_frames=1800', 'encoded_frames=1799'))
        mp4 = self.root / 'run-b-superfast-mp4.json'
        mp4.write_text(mp4.read_text().replace('"nb_read_frames": 1800', '"nb_read_frames": 1799'))
        report = evaluator.evaluate_runs(self.root, minutes_a=2, minutes_b=1)
        self.assertEqual(report['status'], 'PASS', report)
        self.assertEqual(report['holding_preset'], 'ultrafast')
        self.assertEqual(report['trials']['run-b/superfast']['lost_frames']['mp4'], 1)

    def test_cli_per_trial_exits_nonzero_on_missing_full_duration_evidence(self):
        result = subprocess.run([sys.executable, str(SPEC.origin), str(self.root), 'run-a'],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(json.loads(result.stdout)['status'], 'FAIL')


if __name__ == '__main__':
    unittest.main()

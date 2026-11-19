#!/usr/bin/env python3
"""Offline acceptance checks against the committed first-session evidence."""
import json
import subprocess
import sys
import unittest
from pathlib import Path

from evaluate import summarize

ROOT = Path(__file__).parent


class EvaluateTest(unittest.TestCase):
    def test_first_session_does_not_invent_stream_or_full_window(self):
        for mode in ('4k', '1080'):
            with self.subTest(mode=mode):
                result = summarize(ROOT / 'logs', mode)
                self.assertEqual(result['status'], 'NOT_VERIFIED')
                self.assertFalse(result['limits']['ten_minutes'])
                self.assertFalse(result['limits']['stream_29_5fps'])
                self.assertGreater(result['record_fps'], 29.5)
                self.assertFalse(result['limits']['record_ten_minutes'])
                self.assertGreater(float(result['infer_p99_ms']), 30)
                self.assertIsNone(result['viewer_fps'])

    def test_cli_reads_actual_mode_directories(self):
        output = subprocess.check_output([sys.executable, str(ROOT / 'evaluate.py')], text=True)
        results = [json.loads(line) for line in output.splitlines()]
        self.assertEqual([r['mode'] for r in results], ['4k', '1080'])
        for result in results:
            self.assertIn('limits', result)
            self.assertIn('capture_fps', result)


if __name__ == '__main__':
    unittest.main()

"""Offline startup handling regression: missing live playlist must be retried, not abandoned."""
import importlib.util
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('viewer_hls', Path(__file__).with_name('viewer-hls.py'))
viewer_hls = importlib.util.module_from_spec(spec)
spec.loader.exec_module(viewer_hls)


class ViewerStartupTest(unittest.TestCase):
    def test_retries_missing_playlist_before_decoding(self):
        calls = []
        def open_playlist(url, options):
            calls.append(url)
            if len(calls) == 1:
                raise OSError('playlist not ready')
            return object()
        with patch.object(viewer_hls.av, 'open', side_effect=open_playlist), patch.object(viewer_hls.time, 'sleep'):
            self.assertIsNotNone(viewer_hls.open_live('https://example.test/hls/spike.m3u8', 30))
        self.assertEqual(len(calls), 2)

    def test_exhausted_startup_fails(self):
        with patch.object(viewer_hls.av, 'open', side_effect=OSError('offline')), patch.object(viewer_hls.time, 'sleep'), patch.object(viewer_hls.time, 'monotonic', side_effect=[0, 31]):
            with self.assertRaises(OSError):
                viewer_hls.open_live('https://example.test/hls/spike.m3u8', 30)


if __name__ == '__main__':
    unittest.main()

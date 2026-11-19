"""Offline viewer regressions: startup retry and segment-sequence accounting."""
import importlib.util
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('viewer_hls', Path(__file__).with_name('viewer-hls.py'))
viewer_hls = importlib.util.module_from_spec(spec)
spec.loader.exec_module(viewer_hls)

BASE = 'https://example.test/hls/spike.m3u8'
PLAYLIST = '#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:7\n#EXTINF:1.000,\nspike-7.ts\n#EXTINF:1.000,\nspike-8.ts\n'


class ViewerStartupTest(unittest.TestCase):
    def test_retries_missing_playlist_before_decoding(self):
        calls = []
        def fetch(url):
            calls.append(url)
            if len(calls) == 1:
                raise OSError('playlist not ready')
            return b'#EXTM3U\n'
        with patch.object(viewer_hls, 'fetch', side_effect=fetch), patch.object(viewer_hls.time, 'sleep'):
            self.assertEqual(viewer_hls.fetch_playlist(BASE, 30), '#EXTM3U\n')
        self.assertEqual(len(calls), 2)

    def test_exhausted_startup_fails(self):
        with patch.object(viewer_hls, 'fetch', side_effect=OSError('offline')), patch.object(viewer_hls.time, 'sleep'), patch.object(viewer_hls.time, 'monotonic', side_effect=[0, 31]):
            with self.assertRaises(OSError):
                viewer_hls.fetch_playlist(BASE, 30)


class SegmentSequenceTest(unittest.TestCase):
    def test_first_poll_takes_all_with_absolute_urls(self):
        self.assertEqual(viewer_hls.new_segments(PLAYLIST, BASE, None),
                         ([(7, 'https://example.test/hls/spike-7.ts'), (8, 'https://example.test/hls/spike-8.ts')], 0, False))

    def test_already_fetched_segments_are_skipped(self):
        self.assertEqual(viewer_hls.new_segments(PLAYLIST, BASE, 8),
                         ([(8, 'https://example.test/hls/spike-8.ts')], 0, False))

    def test_segments_rotated_out_before_fetch_count_as_missed(self):
        # The viewer fell behind: segments 5 and 6 left the window before it fetched them.
        self.assertEqual(viewer_hls.new_segments(PLAYLIST, BASE, 5)[1], 2)

    def test_endlist_is_reported(self):
        self.assertTrue(viewer_hls.new_segments(PLAYLIST + '#EXT-X-ENDLIST\n', BASE, None)[2])


if __name__ == '__main__':
    unittest.main()

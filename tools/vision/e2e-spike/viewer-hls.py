#!/usr/bin/env python3
"""Measure decoded frames fetched from the viewer's HLS endpoint, not a browser display."""
import argparse
import io
import json
import time
import urllib.error
import urllib.request
from urllib.parse import urljoin

import av

# PyAV's bundled GnuTLS has no usable CA store and FFmpeg's HLS demuxer does not forward
# ca_file to segment requests, so HTTP is done here with Python's system trust store.


def fetch(url):
    with urllib.request.urlopen(url, timeout=15) as response:
        return response.read()


def fetch_playlist(playlist, startup_seconds):
    deadline = time.monotonic() + startup_seconds
    while True:
        try:
            return fetch(playlist).decode()
        except (OSError, urllib.error.URLError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(1)


def new_segments(text, base, next_seq):
    """Return ([(seq, url)], missed_count, ended) for segments at or after next_seq."""
    sequence, entries, ended = 0, [], False
    for line in text.splitlines():
        line = line.strip()
        if line.startswith('#EXT-X-MEDIA-SEQUENCE:'):
            sequence = int(line.split(':', 1)[1])
        elif line == '#EXT-X-ENDLIST':
            ended = True
        elif line and not line.startswith('#'):
            entries.append((sequence + len(entries), urljoin(base, line)))
    fresh = [(seq, url) for seq, url in entries if next_seq is None or seq >= next_seq]
    missed = fresh[0][0] - next_seq if fresh and next_seq is not None else 0
    return fresh, missed, ended


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('playlist', help='Full HTTPS URL to the stream-specific .m3u8')
    parser.add_argument('--seconds', type=float, default=600)
    args = parser.parse_args()
    if not args.playlist.startswith('https://') or not args.playlist.endswith('.m3u8') or args.seconds <= 0:
        parser.error('expected HTTPS .m3u8 playlist and a positive duration')

    start = time.monotonic()
    first = None
    count = 0
    first_pts = last_pts = None
    discontinuities = 0
    missed_segments = 0
    next_seq = None
    last_progress = time.monotonic()
    text = fetch_playlist(args.playlist, 30)
    while True:
        segments, missed, ended = new_segments(text, args.playlist, next_seq)
        missed_segments += missed
        for seq, url in segments:
            with av.open(io.BytesIO(fetch(url)), format='mpegts') as container:
                for frame in container.decode(video=0):
                    if first is None:
                        first = time.monotonic()
                    pts = frame.time
                    if last_pts is not None and pts is not None and (pts <= last_pts or pts - last_pts > 0.1):
                        discontinuities += 1
                    if pts is not None:
                        first_pts = pts if first_pts is None else first_pts
                        last_pts = pts
                    count += 1
            next_seq = seq + 1
            last_progress = time.monotonic()
        if first is not None and time.monotonic() - first >= args.seconds:
            break
        # Fail if the stream stalls or the publisher ends it.
        if ended or time.monotonic() - last_progress > 15:
            break
        time.sleep(0.5)
        text = fetch(args.playlist).decode()
    elapsed = time.monotonic() - (first or start)
    result = dict(playlist=args.playlist, elapsed_s=elapsed, decoded_frames=count,
                  decoded_fps=count / elapsed if elapsed else 0, pts_discontinuities=discontinuities,
                  # Content rate; wall-clock decoded_fps is biased up by the initial playlist backlog.
                  pts_fps=(count - 1) / (last_pts - first_pts) if last_pts and last_pts > first_pts else 0,
                  missed_segments=missed_segments, full_window=elapsed >= args.seconds)
    print(json.dumps(result, sort_keys=True), flush=True)
    if not result['full_window'] or discontinuities or missed_segments:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

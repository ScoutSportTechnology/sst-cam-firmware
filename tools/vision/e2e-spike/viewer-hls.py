#!/usr/bin/env python3
"""Measure decoded frames fetched from the viewer's HLS endpoint, not a browser display."""
import argparse
import json
import time

import av


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
    last_pts = None
    discontinuities = 0
    # Do not pass credentials or secrets to the demuxer. Fail if the stream stalls/exits.
    with av.open(args.playlist, options={'rw_timeout': '15000000'}) as container:
        for frame in container.decode(video=0):
            if first is None:
                first = time.monotonic()
            pts = frame.time
            if last_pts is not None and pts is not None and (pts <= last_pts or pts - last_pts > 0.1):
                discontinuities += 1
            if pts is not None:
                last_pts = pts
            count += 1
            if time.monotonic() - first >= args.seconds:
                break
    elapsed = time.monotonic() - (first or start)
    result = dict(playlist=args.playlist, elapsed_s=elapsed, decoded_frames=count,
                  decoded_fps=count / elapsed if elapsed else 0, pts_discontinuities=discontinuities,
                  full_window=elapsed >= args.seconds)
    print(json.dumps(result, sort_keys=True), flush=True)
    if not result['full_window'] or discontinuities:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

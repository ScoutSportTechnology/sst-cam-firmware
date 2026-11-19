#!/usr/bin/env python3
"""Summarize independent acceptance evidence from one completed HIL session."""
import json
import re
import sys
from pathlib import Path


def fields(line):
    return dict(re.findall(r'([a-z_][a-z_0-9]*)=([^\s]+)', line))


def summarize(root, mode):
    source = root / mode
    lines = (source / 'e2e.log').read_text().splitlines()
    steady = next((fields(line) for line in lines if line.startswith('window=steady ')), {})
    final = next((fields(line) for line in lines if line.startswith('final ')), {})
    monitor = (source / 'monitor.log').read_text().splitlines()
    memory = [int(value) for line in monitor for value in re.findall(r'MemAvailable_kib=(\d+)', line)]
    measurement_file = root / 'recorded-mp4.json'
    if measurement_file.exists():
        measurement = json.loads(measurement_file.read_text())[mode]
        frame_count = int(measurement['nb_read_frames'])
        duration = float(measurement['duration_seconds'])
    else:
        measurement = json.loads((root / f'{mode}-ffprobe.json').read_text())['streams'][0]
        frame_count = int(measurement['nb_read_frames'])
        duration = float(measurement['duration'])
    record_fps = frame_count / duration if duration else 0
    viewer_file = root / f'{mode}-viewer.log'
    viewer = json.loads(viewer_file.read_text()) if viewer_file.exists() and viewer_file.stat().st_size else {}
    # Conservative viewer rate: wall clock is biased up by the startup backlog, PTS hides delivery stalls.
    viewer_fps = min(viewer.get('decoded_fps', 0), viewer.get('pts_fps', 0))
    limits = {
        'ten_minutes': float(steady.get('elapsed_s', 0)) >= 600,
        'capture_no_drops': steady.get('no_drop') == 'PASS',
        # Detection is scheduled at exactly 15 Hz: allow one batch of window-edge quantization, no skipped slot.
        'inference_15hz': steady.get('analysis_skipped') == '0'
            and int(steady.get('infer_batches', 0)) + 1 >= 15 * float(steady.get('elapsed_s', 0)),
        'record_29_5fps': record_fps >= 29.5,
        'record_ten_minutes': duration >= 600,
        'stream_29_5fps': viewer_fps >= 29.5 and viewer.get('full_window', False)
            and not viewer.get('missed_segments', 1) and not viewer.get('pts_discontinuities', 1),
        'memory_1gib': bool(memory) and min(memory) >= 1048576,
    }
    return {'mode': mode, 'limits': limits, 'status': 'NOT_VERIFIED' if not all(limits.values()) else 'PENDING_THERMAL_REVIEW',
            'capture_fps': steady.get('capture_fps'), 'paired_pct': steady.get('paired_pct'),
            'infer_hz': steady.get('infer_hz'), 'infer_p99_ms': steady.get('inference_latency_ms_p99'),
            'render_fps': steady.get('render_fps'), 'encode_fps': final.get('steady_encoded_fps'),
            'record_fps': record_fps, 'viewer_fps': viewer_fps if viewer else None,
            'mem_available_min_kib': min(memory) if memory else None,
            'tegrastats_log': str(source / 'tegrastats.log'),
            'throttling': 'UNREVIEWED', 'cpu_per_core': str(source / 'cores.log')}


def main():
    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).parent / 'logs'
    for mode in ('4k', '1080'):
        try:
            print(json.dumps(summarize(root, mode), sort_keys=True))
        except (OSError, ValueError, KeyError, StopIteration) as exc:
            print(json.dumps({'mode': mode, 'status': 'NO_COMPLETE_EVIDENCE', 'error': str(exc)}))


if __name__ == '__main__':
    main()

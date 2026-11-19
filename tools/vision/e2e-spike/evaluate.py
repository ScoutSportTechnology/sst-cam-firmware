#!/usr/bin/env python3
"""Summarize independent acceptance evidence from one completed HIL session."""
import json
import re
import sys
from pathlib import Path


def fields(line):
    return dict(re.findall(r'([a-z_]+)=([^\s]+)', line))


def summarize(root, mode):
    source = root / f'vision-e2e-{mode}'
    lines = (source / 'e2e.log').read_text().splitlines()
    steady = next((fields(line) for line in lines if line.startswith('window=steady ')), {})
    final = next((fields(line) for line in lines if line.startswith('final ')), {})
    monitor = (source / 'monitor.log').read_text().splitlines()
    memory = [int(value) for line in monitor for value in re.findall(r'MemAvailable_kib=(\d+)', line)]
    probe = json.loads((root / f'{mode}-ffprobe.json').read_text())
    streams = probe.get('streams', [])
    stream = streams[0] if streams else {}
    frame_count = int(stream.get('nb_read_frames') or 0)
    duration = float(stream.get('duration') or 0)
    record_fps = frame_count / duration if duration else 0
    viewer = json.loads((root / f'{mode}-viewer.log').read_text())
    limits = {
        'ten_minutes': float(final.get('elapsed_s', 0)) >= 600,
        'capture_no_drops': steady.get('no_drop') == 'PASS',
        'inference_15hz': float(steady.get('infer_hz', 0)) >= 15,
        'record_29_5fps': record_fps >= 29.5,
        'stream_29_5fps': viewer.get('decoded_fps', 0) >= 29.5 and viewer.get('full_window', False),
        'memory_1gib': bool(memory) and min(memory) >= 1048576,
    }
    return {'mode': mode, 'limits': limits, 'status': 'NOT_VERIFIED' if not all(limits.values()) else 'PENDING_THERMAL_REVIEW',
            'capture_fps': steady.get('capture_fps'), 'paired_pct': steady.get('paired_pct'),
            'infer_hz': steady.get('infer_hz'), 'infer_p99_ms': steady.get('inference_latency_ms_p99'),
            'render_fps': steady.get('render_fps'), 'encode_fps': final.get('steady_encoded_fps'),
            'record_fps': record_fps, 'viewer_fps': viewer.get('decoded_fps'),
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

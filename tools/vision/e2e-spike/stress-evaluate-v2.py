#!/usr/bin/env python3
"""Offline, fail-closed review of v2 90-minute clean / 20-minute overlay evidence.

Usage: python3 stress-evaluate-v2.py [logs-stress/v2] [run-a|run-b/superfast|run-b/ultrafast]
All measurements come from transferred trial logs; MP4 JSON must come from a
separate full decode of the transferred MP4, not its encoder's own counters.
"""
import json
import math
import re
import statistics
import sys
from datetime import datetime, timedelta
from pathlib import Path


def fields(line):
    return dict(re.findall(r'([a-z_][a-z_0-9]*)=([^\s]+)', line))


def number(data, key):
    try:
        value = float(data[key])
        return value if math.isfinite(value) else float('-inf')
    except (KeyError, ValueError, TypeError):
        return float('-inf')


def integer(data, key):
    value = number(data, key)
    return int(value) if math.isfinite(value) and value >= 0 and value.is_integer() else None


def pair(data, key):
    try:
        values = [float(value) for value in data[key].split(',')]
        return values if len(values) == 2 and all(math.isfinite(v) for v in values) else []
    except (KeyError, ValueError, TypeError):
        return []


def percentile(values, fraction):
    values = sorted(values)
    return values[math.ceil(len(values) * fraction) - 1]


def parse_time(text, tegra=False):
    try:
        return (datetime.strptime(text[:19], '%m-%d-%Y %H:%M:%S') if tegra else
                datetime.fromisoformat(text.split()[0]).replace(tzinfo=None))
    except (ValueError, IndexError):
        return None


def read(path):
    return path.read_text() if path.exists() else ''


def cpu_stats(text):
    previous, usage, times = {}, {}, {}
    stamp = None
    for line in text.splitlines():
        parts = line.split()
        if parts and re.fullmatch(r'cpu\d+', parts[0]) and len(parts) >= 9:
            try:
                ticks = [int(value) for value in parts[1:9]]
            except ValueError:
                continue
            busy = sum(ticks[:3]) + sum(ticks[5:8])
            total = sum(ticks)
            old = previous.get(parts[0])
            if old and stamp is not None and total > old[1] and 0 <= busy - old[0] <= total - old[1]:
                usage.setdefault(parts[0], []).append(100 * (busy - old[0]) / (total - old[1]))
                times.setdefault(parts[0], []).append(stamp)
            previous[parts[0]] = busy, total
        else:
            stamp = parse_time(line)
    return ({core: {'mean_pct': round(statistics.mean(values), 2),
                    'p95_pct': round(percentile(values, .95), 2),
                    'p95_margin_pct': round(100 - percentile(values, .95), 2),
                    'samples': len(values)}
             for core, values in usage.items()}, times)


def coverage(times, start, minutes):
    stamps = [time for time in times if time is not None]
    if start is None or not stamps or (max(stamps) - min(stamps)).total_seconds() < minutes * 60 - 3:
        return False
    counts = [0] * minutes
    for stamp in stamps:
        index = int((stamp - start).total_seconds() // 60)
        if 0 <= index < minutes:
            counts[index] += 1
    return all(count >= 54 for count in counts)


def summarize(root, trial='run-a', minutes=None):
    """Return verdict and evidence for a trial; never infer missing counters as zero."""
    if trial not in ('run-a', 'run-b/superfast', 'run-b/ultrafast'):
        raise ValueError('unknown trial')
    if minutes is None:
        minutes = 90 if trial == 'run-a' else 20
    if not isinstance(minutes, int) or minutes < 1:
        raise ValueError('minutes must be positive')
    root = Path(root)
    source = root / trial
    prefix = trial.replace('/', '-')
    lines = read(source / 'e2e.log').splitlines()
    config = [fields(line) for line in lines if line.startswith('encoders=')]
    config = config[-1] if len(config) == 1 else {}
    overlay = trial != 'run-a'
    expected_preset = trial.rsplit('/', 1)[-1] if overlay else 'none'
    configuration_ok = (config.get('encoders') == ('independent' if overlay else 'shared') and
                        config.get('preset') == 'veryfast' and
                        config.get('overlay') == ('on' if overlay else 'off') and
                        config.get('overlay_preset') == expected_preset)
    windows = [fields(line) for line in lines if line.startswith('minute=')]
    bad_minutes, seen = [], set()
    for window in windows:
        minute = integer(window, 'minute')
        if minute is None or minute > minutes:
            # The runner may emit a partial trailing interval; it cannot override a required interval.
            if minute is None or window.get('no_drop') != 'PASS':
                bad_minutes.append(window.get('minute', '?'))
            continue
        good = (minute not in seen and minute * 60 + 4 <= number(window, 'elapsed_s') <= minute * 60 + 10 and
                all(value >= 29.5 for value in pair(window, 'capture_fps')) and
                len(pair(window, 'capture_fps')) == 2 and
                number(window, 'render_fps') >= 29.5 and
                number(window, 'mp4_fps') >= 29.5 and number(window, 'rtmp_fps') >= 29.5 and
                integer(window, 'infer_batches') is not None and
                integer(window, 'infer_batches') >= math.floor(15 * (
                    number(window, 'elapsed_s') - (5 if minute == 1 else
                    number(next((w for w in windows if integer(w, 'minute') == minute - 1), {}),
                           'elapsed_s')))) - 1 and
                number(window, 'infer_hz') >= 14.9 and
                integer(window, 'timestamp_gaps') == 0 and
                integer(window, 'capture_id_gaps') == 0 and
                pair(window, 'capture_drops') == [0, 0] and
                integer(window, 'analysis_skipped') == 0 and
                window.get('no_drop') == 'PASS')
        if not good:
            bad_minutes.append(minute)
        seen.add(minute)
    missing_minutes = sorted(set(range(1, minutes + 1)) - seen)
    window_evidence_complete = (not missing_minutes and len(seen) == minutes and
        all(integer(w, 'minute') is not None and
            integer(w, 'minute') * 60 + 4 <= number(w, 'elapsed_s') <= integer(w, 'minute') * 60 + 10 and
            len(pair(w, 'capture_fps')) == 2 and
            all(math.isfinite(number(w, key)) for key in
                ('render_fps', 'mp4_fps', 'rtmp_fps', 'infer_hz', 'infer_batches', 'analysis_skipped')) and
            integer(w, 'timestamp_gaps') is not None and
            integer(w, 'capture_id_gaps') is not None and
            len(pair(w, 'capture_drops')) == 2 and w.get('no_drop') in ('PASS', 'FAIL')
            for w in windows if integer(w, 'minute') is not None and integer(w, 'minute') <= minutes))
    final_lines = [fields(line) for line in lines if line.startswith('final ')]
    final = final_lines[-1] if len(final_lines) == 1 else {}
    steady = [fields(line) for line in lines if line.startswith('window=steady ')]
    steady = steady[-1] if len(steady) == 1 else {}
    no_gaps = (all(integer(steady, name) == 0 for name in (
        'timestamp_gaps', 'capture_id_gaps', 'internal_gaps', 'reordered',
        'queue_drops', 'unmatched', 'render_misses', 'analysis_skipped')) and
        steady.get('no_drop') == 'PASS')
    rendered = integer(final, 'render')
    mp4_frames = integer(final, 'encoded_frames')
    rtmp_input = integer(final, 'rtmp_input_frames')
    rtmp_frames = integer(final, 'rtmp_frames')
    branch_counters = ('mp4_frames', 'mp4_branch_frames', 'rtmp_branch_frames',
                       'clean_encoder_in', 'encoder_queue_drops', 'mp4_branch_drops',
                       'rtmp_branch_drops')
    branch_ok = (rendered is not None and rendered > 0 and
                 all(integer(final, key) is not None for key in branch_counters) and
                 integer(final, 'clean_encoder_in') == rendered and
                 integer(final, 'encoder_queue_drops') == 0 and
                 integer(final, 'mp4_branch_drops') == 0 and
                 integer(final, 'rtmp_branch_drops') == 0 and
                 integer(final, 'mp4_frames') == integer(final, 'mp4_branch_frames') == mp4_frames and
                 integer(final, 'rtmp_branch_frames') == rtmp_frames and
                 (integer(final, 'overlay_encoder_in') == rendered if overlay else
                  integer(final, 'overlay_encoder_in') == 0))
    lost = {'mp4': rendered - mp4_frames if rendered is not None and mp4_frames is not None else None,
            'rtmp': rendered - rtmp_frames if rendered is not None and rtmp_frames is not None else None}
    eos_ok = (branch_ok and rendered == mp4_frames == rtmp_input == rtmp_frames and
              integer(final, 'encoder_queue_or_branch_gap') == 0 and
              number(final, 'elapsed_s') >= minutes * 60 + 5 and
              final.get('acceptance') == 'PASS')

    monitor = [(parse_time(line), int(match[1])) for line in read(source / 'monitor.log').splitlines()
               if (match := re.search(r'\bMemAvailable_kib=(\d+)', line))]
    cores, core_times = cpu_stats(read(source / 'cores.log'))
    tegra_lines = read(source / 'tegrastats.log').splitlines()
    gpu_samples = [(parse_time(line, True), int(match[1])) for line in tegra_lines
                   if (match := re.search(r'\bGR3D_FREQ (\d+)%', line))]
    tj_samples = [(parse_time(line, True), float(match[1])) for line in tegra_lines
                  if (match := re.search(r'\btj@([\d.]+)C', line))]
    gpu_values = [v for _, v in gpu_samples]
    tj_values = [v for _, v in tj_samples]
    start = min((stamp for stamp, _ in monitor if stamp is not None), default=None)
    # /proc/stat reports offline cores too; the board's online list fixes expected per-core coverage.
    online_raw = read(source / 'baseline.log')
    online_match = re.search(r'(?m)^online_cores=(cpu\d+(?:,cpu\d+)*)$', online_raw)
    expected_cores = set(online_match[1].split(',')) if online_match else set()
    online_ok = (bool(expected_cores) and
                 len(expected_cores) == len(online_match[1].split(',')) and
                 set(cores) == expected_cores) if online_match else False
    baseline = read(source / 'baseline.log').strip()
    power = read(source / 'power-mode.log').strip()
    baseline_ok = (bool(baseline) and bool(power) and
                   all(name in baseline.lower() for name in ('nvpmodel', 'jetson_clocks', 'fan', 'ambient')) and
                   not re.search(r'(?im)^\s*(?:nvpmodel|jetson_clocks|fan)\s*=\s*(?:unknown|unavailable|missing|n/a)\b', baseline))
    telemetry_ok = (online_ok and expected_cores.issubset(cores) and
                    bool(gpu_values) and bool(tj_values) and
                    coverage([stamp for stamp, _ in monitor], start, minutes) and
                    all(coverage(core_times[core], start, minutes) for core in expected_cores) and
                    coverage([stamp for stamp, _ in gpu_samples], start, minutes) and
                    coverage([stamp for stamp, _ in tj_samples], start, minutes) and
                    baseline_ok)
    thermal_trips = []
    sensor = ''
    for line in read(source / 'thermal-trips.log').splitlines():
        if line.startswith('zone='):
            sensor = fields(line).get('type', '').lower()
        match = re.search(r'trip_point_\d+_type=(\S+) temperature_millic=(\d+)', line)
        if match and match[1] == 'passive' and any(word in sensor for word in ('gpu', 'tj', 'junction')):
            thermal_trips.append(int(match[2]) / 1000)
    passive = min(thermal_trips) if thermal_trips else None
    crossed = sum(a < passive <= b for a, b in zip(tj_values, tj_values[1:])) if passive is not None else None
    slice_seconds = min(900, minutes * 30)
    early = [v for stamp, v in tj_samples if stamp is not None and start is not None and
             start + timedelta(seconds=minutes * 60 - 2 * slice_seconds) <= stamp <
             start + timedelta(seconds=minutes * 60 - slice_seconds)]
    late = [v for stamp, v in tj_samples if stamp is not None and start is not None and
            start + timedelta(seconds=minutes * 60 - slice_seconds) <= stamp <
            start + timedelta(seconds=minutes * 60)]
    thermal = {'passive_trip_c': passive, 'passive_crossings': crossed,
               'max_c': max(tj_values) if tj_values else None,
               'last_15m_delta_c': round(statistics.mean(late) - statistics.mean(early), 2)
               if early and late else None}
    mp4 = json.loads(read(root / f'{prefix}-mp4.json') or '{}')
    viewer = json.loads(read(root / f'{prefix}-viewer.log') or '{}')
    decoded = integer(mp4, 'nb_read_frames')
    decode_ok = (decoded is not None and mp4_frames is not None and decoded == mp4_frames and
                 number(mp4, 'width') == 1920 and number(mp4, 'height') == 1080 and
                 number(mp4, 'duration') >= minutes * 60 - 1 and
                 number(mp4, 'measured_fps') >= 29.5)
    kernel = read(source / 'kernel.log')
    limits = {
        'encoder_configuration': configuration_ok,
        'all_minutes': len(seen) == minutes and not bad_minutes and not missing_minutes,
        'zero_gaps': no_gaps,
        'eos_parity': eos_ok,
        'mp4_decode_parity': decode_ok and decoded == rendered,
        'viewer': viewer.get('full_window') is True and
            number(viewer, 'elapsed_s') >= minutes * 60 and
            number(viewer, 'decoded_fps') >= 29.5 and number(viewer, 'pts_fps') >= 29.5 and
            number(viewer, 'decoded_frames') >= 29.5 * minutes * 60 and
            integer(viewer, 'missed_segments') == 0 and integer(viewer, 'pts_discontinuities') == 0,
        'memory': bool(monitor) and min(value for _, value in monitor) >= 1048576,
        'telemetry_coverage': telemetry_ok and passive is not None and bool(read(source / 'thermal-trips.log').strip()),
        'no_throttling': bool(kernel.strip()) and not re.search(
            r'(?i)(?:thermal[^\n]*throttl|throttl[^\n]*thermal|overheat|soctherm[^\n]*trip)', kernel),
    }
    return {'trial': trial, 'status': 'PASS' if all(limits.values()) else 'FAIL', 'limits': limits,
            'bad_minutes': bad_minutes, 'missing_minutes': missing_minutes, 'lost_frames': lost,
            'cpu_per_core': cores,
            'gpu': {'mean_pct': round(statistics.mean(gpu_values), 2),
                    'p95_pct': percentile(gpu_values, .95),
                    'p95_margin_pct': 100 - percentile(gpu_values, .95)} if gpu_values else {},
            'memory_min_kib': min((value for _, value in monitor), default=None),
            'thermal': thermal, 'baseline': baseline, 'power_mode': power,
            'telemetry_samples': {'memory': len(monitor), 'gpu': len(gpu_values), 'tj': len(tj_values)},
            'mp4': mp4, 'viewer': viewer, 'independent_decode_valid': decode_ok,
            'window_evidence_complete': window_evidence_complete}


def evaluate_runs(root, minutes_a=90, minutes_b=20):
    """Compare superfast first; ultrafast is valid only after a measured superfast failure."""
    trials = {'run-a': summarize(root, 'run-a', minutes_a)}
    fast = Path(root) / 'run-b/superfast/e2e.log'
    if not fast.exists():
        return {'status': 'NO_COMPLETE_EVIDENCE' if trials['run-a']['status'] == 'PASS' else 'FAIL',
                'holding_preset': None, 'trials': trials}
    trials['run-b/superfast'] = summarize(root, 'run-b/superfast', minutes_b)
    if trials['run-b/superfast']['status'] == 'PASS':
        return {'status': trials['run-a']['status'], 'holding_preset': 'superfast', 'trials': trials}
    slow = Path(root) / 'run-b/ultrafast/e2e.log'
    if slow.exists():
        trials['run-b/ultrafast'] = summarize(root, 'run-b/ultrafast', minutes_b)
    # A failed, measured trial permits fallback; missing provenance is not a failed preset.
    if not (trials['run-b/superfast']['limits']['telemetry_coverage'] and
            trials['run-b/superfast']['window_evidence_complete'] and
            trials['run-b/superfast']['limits']['encoder_configuration'] and
            trials['run-b/superfast']['limits']['memory'] and
            trials['run-b/superfast']['limits']['no_throttling'] and
            trials['run-b/superfast']['limits']['viewer'] and
            trials['run-b/superfast']['independent_decode_valid']):
        return {'status': 'NO_COMPLETE_EVIDENCE', 'holding_preset': None, 'trials': trials}
    if not slow.exists():
        return {'status': 'FAIL', 'holding_preset': None, 'trials': trials}
    return {'status': ('PASS' if trials['run-a']['status'] == 'PASS' and
                        trials['run-b/ultrafast']['status'] == 'PASS' else 'FAIL'),
            'holding_preset': 'ultrafast' if trials['run-b/ultrafast']['status'] == 'PASS' else None,
            'trials': trials}


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / 'logs-stress/v2'
    try:
        if len(sys.argv) > 3:
            raise ValueError('usage: stress-evaluate-v2.py [root] [trial]')
        result = summarize(root, sys.argv[2]) if len(sys.argv) == 3 else evaluate_runs(root)
    except (OSError, ValueError, TypeError, ZeroDivisionError) as exc:
        result = {'status': 'NO_COMPLETE_EVIDENCE', 'error': str(exc)}
    print(json.dumps(result, sort_keys=True))
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())

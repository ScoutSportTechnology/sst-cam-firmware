#!/usr/bin/env bash
# Run on the Jetson beside the e2e prototype; all paths are caller-supplied and durable.
set -euo pipefail
if (($# != 3)) || [[ ! $2 =~ ^[0-9]+$ ]] || (( $2 < 1 )) || [[ ! -d $3 ]]; then
  echo 'usage: monitor.sh <process-pid> <seconds> <existing-log-dir>' >&2
  exit 2
fi
pid=$1
seconds=$2
logdir=$3
command -v tegrastats >/dev/null
tegrastats --interval 1000 --logfile "$logdir/tegrastats.log" &
tegrapid=$!
cleanup() { kill "$tegrapid" 2>/dev/null || true; wait "$tegrapid" 2>/dev/null || true; }
trap cleanup EXIT
for ((second=0; second<seconds; second++)); do
  if ! kill -0 "$pid" 2>/dev/null; then
    printf '%s prototype_exited pid=%s\n' "$(date -Ins)" "$pid" >>"$logdir/monitor.log"
    break
  fi
  printf '%s ' "$(date -Ins)" >>"$logdir/monitor.log"
  awk '/MemAvailable:/ {printf "MemAvailable_kib=%s ", $2}' /proc/meminfo >>"$logdir/monitor.log"
  ps -p "$pid" -o 'pid=,pcpu=,rss=,nlwp=,stat=' >>"$logdir/monitor.log" || true
  ps -L -p "$pid" -o 'tid=,pcpu=,comm=' >>"$logdir/threads.log" || true
  # /proc/stat preserves per-core busy and idle ticks for interval-by-interval CPU accounting.
  printf '%s\n' "$(date -Ins)" >>"$logdir/cores.log"
  grep '^cpu[0-9]' /proc/stat >>"$logdir/cores.log"
  sleep 1
done
# Existing kernel logs are read only. Keep timestamps for correlations to frame counters.
sudo -n journalctl -k -b -o short-monotonic --no-pager >"$logdir/kernel.log" || true

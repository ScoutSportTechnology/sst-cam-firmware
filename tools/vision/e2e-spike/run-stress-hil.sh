#!/usr/bin/env bash
# Run ONLY from the VM inside jetson-hil.sh session; no footage/MP4 is committed.
# CC BY 3.0 clip: Utrecht v Ajax Dec 2018 (Wikimedia Commons); release rights unreviewed.
set -euo pipefail
[[ ${VISION_E2E_HIL_APPROVED:-} == yes ]] || {
  echo 'HIL not approved: set VISION_E2E_HIL_APPROVED=yes after review' >&2; exit 2;
}
[[ $(readlink /proc/$$/fd/9 2>/dev/null) == "${JETSON_LOCK:-/run/lock/jetson-hil.lock}" ]] || {
  echo 'Invoke only inside jetson-hil.sh session; refusing direct hardware access' >&2; exit 2;
}
root="$(dirname "$(realpath "$0")")"
SSH=(ssh -n -o BatchMode=yes -o ConnectTimeout=6 -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes -i /root/.ssh/jetson_ed25519)
SCP=(scp -o BatchMode=yes -o ConnectTimeout=6 -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes -i /root/.ssh/jetson_ed25519)
remote=sst@10.10.1.30
run="/home/sst/vision-e2e-stress-$(date -u +%Y%m%dT%H%M%SZ)"
stream="${run##*/}"
out="$root/logs-stress/session2"
clip=/root/setup/vision-e2e-footage.webm
python=/root/setup/vision/.venv/bin/python
[[ ! -e "$out" ]] || { echo "Existing logs: $out" >&2; exit 1; }
[[ -s "$clip" && -s "$root/e2e-aarch64" && -s "$root/engine-build-aarch64" ]] || {
  echo 'Missing licensed source clip or cross-built binaries' >&2; exit 1;
}
[[ $(sha256sum "$clip" | cut -d' ' -f1) == 64282779e7258654bc8dc227eafb64f8d4e6c98ff5b9eb994dcd177e24d30c96 ]] || {
  echo 'Soccer clip does not match documented licensed source' >&2; exit 1;
}
mkdir -p "$out"
stopped=0
restore() {
  local rc=$?
  trap - EXIT
  if ((stopped)); then
    # Never restart firmware until every stress child and monitor is gone.
    if ! "${SSH[@]}" "$remote" "pkill -f '^$run/e2e-aarch64 ' 2>/dev/null || true; pkill -f '^$run/monitor.sh ' 2>/dev/null || true; while pgrep -f '^$run/e2e-aarch64 ' >/dev/null; do sleep 1; done; sudo -n systemctl start sst-cam-firmware; systemctl is-active sst-cam-firmware" >"$out/service-restored.log" 2>&1; then
      echo 'Firmware restoration unverified; investigate before reusing rig' >&2
      rc=1
    fi
  fi
  exit "$rc"
}
trap restore EXIT
"${SSH[@]}" "$remote" "mkdir $run"
stopped=1
"${SSH[@]}" "$remote" 'sudo -n systemctl stop sst-cam-firmware; systemctl is-active sst-cam-firmware || test "$?" -eq 3' >"$out/service-stopped.log" 2>&1
"${SCP[@]}" "$root/e2e-aarch64" "$root/engine-build-aarch64" "$root/monitor.sh" /root/setup/vision/rfdetr-export/rfdetr-small.onnx "$clip" "$remote:$run/"
"${SSH[@]}" "$remote" "chmod +x $run/monitor.sh; cd $run; timeout 420 ./engine-build-aarch64 ./rfdetr-small.onnx ./rfdetr-small.engine" >"$out/engine-small.log" 2>&1

trial() {
  local preset=$1 seconds=$2 viewer_seconds=$3 viewer failed=0
  "${SSH[@]}" "$remote" "mkdir $run/$preset"
  (sleep 2; timeout "$((seconds + 120))" "$python" "$root/viewer-hls.py" \
    "https://rtmp.internal.rsantamaria.xyz/hls/$stream-$preset.m3u8" \
    --seconds "$viewer_seconds" >"$out/$preset-viewer.log" 2>"$out/$preset-viewer.err") &
  viewer=$!
  if ! "${SSH[@]}" "$remote" "bash -lc 'cd $run/$preset; set -uo pipefail; stdbuf -oL -eL $run/e2e-aarch64 --clip $run/vision-e2e-footage.webm --preset $preset --seconds $seconds --engine $run/rfdetr-small.engine --record $run/$preset/output.mp4 --rtmp rtmp://10.10.1.96/live/$stream-$preset >e2e.log 2>e2e.err & pid=\$!; (sleep $((seconds + 90)); kill -TERM \$pid 2>/dev/null || true) & watchdog=\$!; $run/monitor.sh \$pid $((seconds + 60)) . & monitor=\$!; wait \$pid; result=\$?; kill \$watchdog \$monitor 2>/dev/null || true; wait \$monitor 2>/dev/null || true; exit \$result'" >"$out/$preset-run.log" 2>&1; then failed=1; fi
  if ! wait "$viewer"; then failed=1; fi
  if ! "${SSH[@]}" "$remote" "sudo -n journalctl -k -b -o short-monotonic --no-pager >$run/$preset/kernel.log"; then failed=1; fi
  if ! "${SCP[@]}" -r "$remote:$run/$preset/" "$out/"; then failed=1; fi
  if [[ -s "$out/$preset/output.mp4" ]]; then
    (cd "$out"; sha256sum "$preset/output.mp4" >>mp4-sha256.txt)
    if ! "$python" - "$out/$preset/output.mp4" >"$out/$preset-mp4.json" 2>"$out/$preset-mp4.err" <<'PY'
import av, json, sys
with av.open(sys.argv[1]) as container:
    stream = container.streams.video[0]
    frames = sum(1 for _ in container.decode(video=0))
    duration = float(stream.duration * stream.time_base)
    print(json.dumps({'width': stream.width, 'height': stream.height,
                      'nb_read_frames': frames, 'duration': duration,
                      'measured_fps': frames / duration if duration else 0}))
PY
    then failed=1; fi
  else
    failed=1
  fi
  return "$failed"
}
if ! trial veryfast 5490 5400; then
  echo 'Stress run failed; evidence retained, skipping faster trial' >&2
  exit 1
fi
if ! "$python" "$root/stress-evaluate.py" "$out" veryfast >"$out/veryfast-evaluation.json"; then
  echo 'Veryfast endurance evidence did not pass; skipping faster trial' >&2
  exit 1
fi
# Only run faster if every resource and endurance gate passed with measured headroom.
if [[ ${VISION_E2E_TRY_FASTER:-no} == yes ]] && "$python" - "$out/veryfast-evaluation.json" <<'PY'
import json, sys
result = json.load(open(sys.argv[1]))
raise SystemExit(0 if result['status'] == 'PASS' and result['headroom'] else 1)
PY
then
  trial faster 690 600
  "$python" "$root/stress-evaluate.py" "$out" faster >"$out/faster-evaluation.json"
fi

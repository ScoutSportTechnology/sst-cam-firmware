#!/usr/bin/env bash
# Run only inside one jetson-hil.sh session. Raw evidence stays local; never commit footage or MP4s.
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
run="/home/sst/vision-e2e-stress-v2-$(date -u +%Y%m%dT%H%M%SZ)-$$"
stream="${run##*/}"
out="$root/logs-stress/v2"
media=/root/setup/vision-e2e-stress-v2-mp4
clip=/root/setup/vision-e2e-footage.webm
python=/root/setup/vision/.venv/bin/python
[[ ! -e $out && ! -e $media ]] || { echo "Existing logs/media: $out or $media" >&2; exit 1; }
[[ -s $clip && -s $root/e2e-aarch64 && -s $root/engine-build-aarch64 && -s /root/setup/vision/rfdetr-export/rfdetr-small.onnx && -x $python && -f $root/stress-evaluate-v2.py ]] || {
  echo 'Missing licensed clip, cross-built binaries, ONNX model, Python or v2 evaluator' >&2; exit 1;
}
[[ $(sha256sum "$clip" | cut -d' ' -f1) == 64282779e7258654bc8dc227eafb64f8d4e6c98ff5b9eb994dcd177e24d30c96 ]] || {
  echo 'Soccer clip does not match documented licensed source' >&2; exit 1;
}
mkdir -p "$root/logs-stress"
mkdir "$out" # exclusive; no prior evidence is overwritten
mkdir "$media" # MP4s stay outside Git; never commit video.
stopped=0
viewer=''
cleanup_remote() {
  "${SSH[@]}" "$remote" "pkill -TERM -f '^$run/e2e-aarch64 ' 2>/dev/null || true; pkill -TERM -f '^bash $run/monitor.sh ' 2>/dev/null || true; pkill -TERM -f '^$run/monitor.sh ' 2>/dev/null || true; pkill -TERM -f '^tegrastats --interval 1000 --logfile $run/' 2>/dev/null || true; pkill -TERM -f '^$run/engine-build-aarch64 ' 2>/dev/null || true; for i in {1..10}; do pgrep -f '^(bash )?$run/(e2e-aarch64|monitor.sh|engine-build-aarch64) |^tegrastats --interval 1000 --logfile $run/' >/dev/null || exit 0; sleep 1; done; pkill -KILL -f '^(bash )?$run/(e2e-aarch64|monitor.sh|engine-build-aarch64) |^tegrastats --interval 1000 --logfile $run/' 2>/dev/null || true; sleep 1; ! pgrep -f '^(bash )?$run/(e2e-aarch64|monitor.sh|engine-build-aarch64) |^tegrastats --interval 1000 --logfile $run/' >/dev/null"
}
# shellcheck disable=SC2317 # EXIT trap invokes this function indirectly.
restore() {
  local rc=$?
  trap - EXIT INT TERM
  if [[ -n $viewer ]]; then
    pkill -TERM -P "$viewer" 2>/dev/null || true
    kill "$viewer" 2>/dev/null || true
    wait "$viewer" 2>/dev/null || true
  fi
  if ((stopped)); then
    # Never restart firmware while an orphan prototype, engine builder or monitor may own the rig.
    if ! cleanup_remote >"$out/cleanup.log" 2>&1; then
      echo 'Orphan cleanup unverified; firmware left stopped for safety' >&2
      rc=1
    elif ! "${SSH[@]}" "$remote" 'sudo -n systemctl start sst-cam-firmware && systemctl is-active sst-cam-firmware' >"$out/service-restored.log" 2>&1; then
      echo 'Firmware restoration unverified; investigate before reusing rig' >&2
      rc=1
    fi
  fi
  exit "$rc"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"${SSH[@]}" "$remote" "mkdir $run"
stopped=1
"${SSH[@]}" "$remote" 'sudo -n systemctl stop sst-cam-firmware; systemctl is-active sst-cam-firmware || test "$?" -eq 3' >"$out/service-stopped.log" 2>&1
"${SCP[@]}" "$root/e2e-aarch64" "$root/engine-build-aarch64" "$root/monitor.sh" /root/setup/vision/rfdetr-export/rfdetr-small.onnx "$clip" "$remote:$run/"
"${SSH[@]}" "$remote" "chmod +x $run/monitor.sh; cd $run; timeout 420 ./engine-build-aarch64 ./rfdetr-small.onnx ./rfdetr-small.engine" >"$out/engine-small.log" 2>&1

trial() {
  local variant=$1 seconds=$2 viewer_seconds=$3 overlay=$4 overlay_preset=$5 failed=0 run_rc=0 viewer_rc=0
  local subdir=$variant
  if [[ $variant == run-b-* ]]; then subdir="run-b/${variant#run-b-}"; fi
  "${SSH[@]}" "$remote" "mkdir -p $run/$(dirname "$subdir"); mkdir $run/$subdir"
  # Read only: do not set a power mode, clocks, fan speed or thermal target.
  "${SSH[@]}" "$remote" "bash -lc 'printf \"nvpmodel:\\n\"; sudo -n nvpmodel -q || printf \"nvpmodel=unknown (query failed)\\n\"; printf \"jetson_clocks:\\n\"; sudo -n jetson_clocks --show || printf \"jetson_clocks=unknown (show unsupported or query failed)\\n\"; printf \"fan:\\n\"; found=0; for f in /sys/class/hwmon/hwmon*/fan*_input /sys/class/hwmon/hwmon*/pwm* /sys/devices/platform/pwm-fan/{target_pwm,cur_pwm,rpm_measured}; do if [[ -r \$f ]]; then printf \"%s=%s\\n\" \"\$f\" \"\$(cat \"\$f\")\"; found=1; fi; done; if (( ! found )); then printf \"fan=unknown (no readable PWM/RPM sensor)\\n\"; fi; printf \"ambient=unknown (no calibrated ambient probe supplied)\\n\"; cores=(); for cpu in /sys/devices/system/cpu/cpu[0-9]*; do [[ -d \$cpu ]] || continue; if [[ \${cpu##*/} == cpu0 && ! -e \$cpu/online ]] || [[ -r \$cpu/online && \$(cat \"\$cpu/online\") == 1 ]]; then cores+=(\"\${cpu##*/}\"); fi; done; IFS=,; printf \"online_cores=%s\\n\" \"\${cores[*]}\"' >$run/$subdir/baseline.log 2>&1" >"$out/$variant-board-before-run.log" 2>&1 || failed=1
  timeout --signal=TERM --kill-after=10 "$((seconds + 120))" bash -c 'sleep 2; exec "$@"' _ \
    "$python" "$root/viewer-hls.py" "https://rtmp.internal.rsantamaria.xyz/hls/$stream-$variant.m3u8" \
    --seconds "$viewer_seconds" >"$out/$variant-viewer.log" 2>"$out/$variant-viewer.err" &
  viewer=$!
  local remote_command
  remote_command="cd $run/$subdir; set -uo pipefail; stdbuf -oL -eL $run/e2e-aarch64 --clip $run/vision-e2e-footage.webm --overlay $overlay --preset veryfast"
  if [[ -n $overlay_preset ]]; then remote_command+=" --overlay-preset $overlay_preset"; fi
  remote_command+=" --seconds $seconds --engine $run/rfdetr-small.engine --record $run/$subdir/output.mp4 --rtmp rtmp://10.10.1.96/live/$stream-$variant >e2e.log 2>e2e.err & pid=\$!; (sleep $((seconds + 90)); kill -TERM \$pid 2>/dev/null || true) & watchdog=\$!; $run/monitor.sh \$pid $((seconds + 60)) $run/$subdir & monitor=\$!; trap 'kill -TERM \$pid \$watchdog \$monitor 2>/dev/null || true; wait \$pid \$watchdog \$monitor 2>/dev/null || true' EXIT; wait \$pid; result=\$?; kill \$watchdog 2>/dev/null || true; wait \$monitor 2>/dev/null || true; exit \$result"
  local quoted_command
  printf -v quoted_command '%q' "$remote_command"
  timeout --signal=TERM --kill-after=10 "$((seconds + 120))" "${SSH[@]}" "$remote" "bash -c $quoted_command" >"$out/$variant-run.log" 2>&1 || run_rc=$?
  printf '%s\n' "$run_rc" >"$out/$variant-run.exit"
  if ((run_rc != 0)); then
    failed=1
    pkill -TERM -P "$viewer" 2>/dev/null || true
    kill "$viewer" 2>/dev/null || true
  fi
  wait "$viewer" || viewer_rc=$?
  viewer=''
  printf '%s\n' "$viewer_rc" >"$out/$variant-viewer.exit"
  ((viewer_rc == 0)) || failed=1
  cleanup_remote >"$out/$variant-cleanup.log" 2>&1 || failed=1
  "${SSH[@]}" "$remote" "sudo -n journalctl -k -b -o short-monotonic --no-pager >$run/$subdir/kernel-after.log" >"$out/$variant-kernel-run.log" 2>&1 || failed=1
  mkdir -p "$out/$subdir" "$media/$subdir"
  "${SCP[@]}" "$remote:$run/$subdir/*.log" "$remote:$run/$subdir/*.err" "$out/$subdir/" >"$out/$variant-copy.log" 2>&1 || failed=1
  "${SCP[@]}" "$remote:$run/$subdir/output.mp4" "$media/$subdir/output.mp4" >"$out/$variant-media-copy.log" 2>&1 || failed=1
  if [[ -s $media/$subdir/output.mp4 ]]; then
    (cd "$media"; sha256sum "$subdir/output.mp4") >>"$out/mp4-sha256.txt"
    "$python" - "$media/$subdir/output.mp4" >"$out/$variant-mp4.json" 2>"$out/$variant-mp4.err" <<'PY' || failed=1
import av, json, sys
with av.open(sys.argv[1]) as container:
    stream = container.streams.video[0]
    frames = sum(1 for _ in container.decode(video=0))
    duration = float(stream.duration * stream.time_base)
    print(json.dumps({'width': stream.width, 'height': stream.height,
                      'nb_read_frames': frames, 'duration': duration,
                      'measured_fps': frames / duration if duration else 0}))
PY
  else
    echo 'MP4 missing or empty' >"$out/$variant-mp4.err"
    failed=1
  fi
  # Evaluate each trial separately; aggregate short-circuits on A and must not suppress B.
  local evaluation_rc=0
  "$python" "$root/stress-evaluate-v2.py" "$out" "$subdir" >"$out/$variant-evaluation.json" 2>"$out/$variant-evaluation.err" || evaluation_rc=$?
  printf '%s\n' "$evaluation_rc" >"$out/$variant-evaluation.exit"
  ((evaluation_rc == 0)) || failed=1
  printf '%s\n' "$failed" >"$out/$variant.exit"
  return "$failed"
}
failed=0
trial run-a 5490 5400 off '' || failed=1
if ! trial run-b-superfast 1290 1200 on superfast; then
  echo 'B superfast did not pass; retaining evidence and trying ultrafast' >&2
  trial run-b-ultrafast 1290 1200 on ultrafast || failed=1
fi
"$python" "$root/stress-evaluate-v2.py" "$out" >"$out/evaluation.json" 2>"$out/evaluation.err" || failed=1
exit "$failed"

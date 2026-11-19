#!/usr/bin/env bash
# Invoke only after review through `jetson-hil.sh session -- ...`.
set -euo pipefail
[[ ${VISION_E2E_HIL_APPROVED:-} == yes ]] || {
  echo 'HIL not approved: set VISION_E2E_HIL_APPROVED=yes after review' >&2
  exit 2
}
# jetson-hil.sh session holds fd 9 for its rig lock across this child process.
[[ $(readlink /proc/$$/fd/9 2>/dev/null) == "${JETSON_LOCK:-/run/lock/jetson-hil.lock}" ]] || {
  echo 'Invoke only inside jetson-hil.sh session; refusing direct hardware access' >&2
  exit 2
}
root="$(dirname "$(realpath "$0")")"
SSH=(ssh -n -o BatchMode=yes -o ConnectTimeout=6 -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes -i /root/.ssh/jetson_ed25519)
SCP=(scp -o BatchMode=yes -o ConnectTimeout=6 -o StrictHostKeyChecking=accept-new -o IdentitiesOnly=yes -i /root/.ssh/jetson_ed25519)
remote=sst@10.10.1.30
run=/home/sst/vision-e2e-spike-20261119
out="$root/logs"
[[ ! -e "$out" ]] || { echo "Existing logs: $out" >&2; exit 1; }
mkdir "$out"
stopped=0
restore() {
  local rc=$?
  trap - EXIT
  if ((stopped)); then
    # Never restart firmware while an orphan trial might still own Argus.
    if ! "${SSH[@]}" "$remote" "pkill -f '^$run/e2e-aarch64 ' 2>/dev/null || true; pkill -f '^timeout 690 stdbuf -oL -eL $run/e2e-aarch64 ' 2>/dev/null || true; while pgrep -f '^$run/e2e-aarch64 ' >/dev/null; do sleep 1; done; sudo -n systemctl start sst-cam-firmware; systemctl is-active sst-cam-firmware" >"$out/service-restored.log" 2>&1; then
      echo 'Firmware restoration unverified; investigate before reusing the rig' >&2
      rc=1
    fi
  fi
  exit "$rc"
}
trap restore EXIT
"${SSH[@]}" "$remote" "mkdir $run"   # exclusive: refuses to overwrite prior artifacts
stopped=1
"${SSH[@]}" "$remote" 'sudo -n systemctl stop sst-cam-firmware; systemctl is-active sst-cam-firmware || test "$?" -eq 3' >"$out/service-stopped.log" 2>&1
"${SCP[@]}" "$root/e2e-aarch64" "$root/engine-build-aarch64" "$root/monitor.sh" /root/setup/vision/rfdetr-export/rfdetr-small.onnx "$remote:$run/"
"${SSH[@]}" "$remote" "cd $run; chmod +x monitor.sh; timeout 420 ./engine-build-aarch64 ./rfdetr-small.onnx ./rfdetr-small.engine" >"$out/engine-small.log" 2>&1
failed=0
for mode in 4k 1080; do
  "${SSH[@]}" "$remote" "mkdir $run/$mode"
  (sleep 5; timeout 690 /root/setup/vision/.venv/bin/python "$root/viewer-hls.py" "https://rtmp.internal.rsantamaria.xyz/hls/vision-e2e-$mode.m3u8" --seconds 580 >"$out/$mode-viewer.log" 2>"$out/$mode-viewer.err") &
  viewer=$!
  if ! "${SSH[@]}" "$remote" "bash -lc 'cd $run/$mode; set -uo pipefail; timeout 690 stdbuf -oL -eL $run/e2e-aarch64 --mode $mode --seconds 600 --engine $run/rfdetr-small.engine --record $run/$mode/output.mp4 --rtmp rtmp://10.10.1.96/live/vision-e2e-$mode >e2e.log 2>e2e.err & pid=\$!; $run/monitor.sh \$pid 660 . & monitor=\$!; wait \$pid; result=\$?; kill \$monitor 2>/dev/null || true; wait \$monitor 2>/dev/null || true; exit \$result'" >"$out/$mode-run.log" 2>&1; then failed=1; fi
  if ! wait "$viewer"; then failed=1; fi
  if ! "${SSH[@]}" "$remote" "cd $run/$mode; ffprobe -v error -select_streams v:0 -count_frames -show_entries stream=width,height,avg_frame_rate,nb_read_frames,duration -of json output.mp4" >"$out/$mode-ffprobe.json" 2>"$out/$mode-ffprobe.err"; then failed=1; fi
  if ! "${SCP[@]}" -r "$remote:$run/$mode/" "$out/"; then failed=1; fi
  if ! grep -q '^final .*acceptance=PASS' "$out/$mode/e2e.log" 2>/dev/null; then failed=1; fi
done
exit "$failed"

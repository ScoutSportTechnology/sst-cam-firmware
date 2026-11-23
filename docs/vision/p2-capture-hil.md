# Vision P2 capture HIL report — 2026-11-20
Branch `feat/vision-p2-capture` from WIP `9fa9edb`; worktree `/root/.paseo/worktrees/3qlepqsk/feat-vision-p2-capture`.
Local: `just ci changed` green (102/102 CTest suites); `just build` green, binary + Argus `.so` packaged. Installer tests 84/84.
Jetson HIL: two `jetson-hil.sh session` invocations, both powered off; second ran gate-ON direct Argus for 600.019 s.
Both sensors 29.969 fps; 17,981 pairs (99.9278% coverage); pair skew p99 3.725 ms; ring drops 0, ring depth ~43; minimum MemAvailable 4,356,828 KiB (~4.15 GiB).
Failed strict acceptance: cam0 incurred 12 inferred frames lost in one startup gap; pair coverage <100%. No additional gaps appeared in sustained interval.
Manual controls requested identical exposure 8,333,333 ns, analog 16, ISP 4 and WB red/blue 0.5/0.5; applied exposure 8,333,332 ns, analog 16, ISP 4; applied WB red 2.017/2.094 and blue 1.582/1.723 — not requested or shared. No forced-wrong-start convergence time was measured.
Gate OFF check returned exit 2; normal GStreamer service restarted and reported both cameras healthy and pipeline running. Jetson final `status`: off.
Independent review blockers: future GPU-reader fence/lifetime, sparse corner-only AE/AWB sampling, incomplete restart/health and per-frame acceptance instrumentation. Sidecar release-digest selection fix applied after local CI and needs full re-verification.
PR: not opened; CI: not requested. STATUS: BLOCKED — WB readback and strict no-drop/100%-pair acceptance failed; 1 of 3 permitted powered sessions remains.

## r2 — 2026-11-22
1. Branch feat/vision-p2-capture @85a10d5: manual AWB, sensor gain limits, 18×32 full-frame meter, per-frame Argus request-ID/metadata audit; release sidecar installer/publisher edits reverted (digest deployment is separate follow-up).
2. `just ci changed` green: 102/102 suites plus installer 84/84; release binary + diagnostic library built and pushed in milestone commits.
3. HIL #3 gate ON 600.006 s: 29.968 fps each; 56.2349% steady pairing, 15,184/15,186 steady metadata mismatches, 11 cam0 startup drops; failed.
4. Offline replay reproduced premature pairing eviction; Argus WB metadata normalizes green to 1. Fixed pairing and normalized requested WB; local tests red→green.
5. HIL #4 gate ON 600.009 s: steady fps 29.9877/29.9894, 0 steady sensor/ID gaps, 0 steady drops, 0 metadata mismatches on both, skew p99 2.256 ms, ring drops 0, minimum MemAvailable 4,378,440 KiB.
6. 17,842 steady pairs / 17,843 max steady frames = 99.9944%; one terminal cam1 frame had no cam0 counterpart in the measured capture window. Cam0 had 12 startup frames lost; no steady gaps.
7. Forced 1 ms initial exposure applied; AE target 0.45±0.05 did not converge within 600 s (both sensors at 8.333 ms exposure, 16× analog and 4× ISP gain caps). No convergence time can be claimed.
8. Gate OFF refused diagnostic; default GStreamer service restarted active, both cameras healthy; Jetson powered off and status checked off after #4.
9. PR not opened, GitHub CI not run: strict 100% steady pairing and measured AE convergence unmet. Follow-ups: brighter calibration scene or agreed target, terminal-pair accounting, GPU reader fence/lifetime, separate sidecar digest rollout.
10. STATUS: BLOCKED — no fifth powered session authorized; do not merge or call HIL accepted.

## r3 — 2026-11-23
1. Branch feat/vision-p2-capture @7f05c22: gated test-only luma target 0.30±0.05; production default 0.45 unchanged; terminal frames excluded and reported; separate artifact-ignore commit 2e4f4f3.
2. `just ci changed` green: 102/102 suites, installer 84/84; release binary and Argus sidecar cross-built. Jetson two powered sessions total, both confirmed off.
3. First lit-rig 45 s dark start at production 0.45: both reached 8.333 ms / 16× analog / 4× ISP caps; luma medians 0.383/0.318 (<0.40), so stopped by rule.
4. Final session, dark forced 1 ms / min gains at diagnostic target 0.30: first in band cam0 frame 23 (~1.20 s), cam1 frame 16 (~1.06 s); simultaneous first entry 1.6299 s / frame 37.
5. Neither stayed within 0.25–0.35 for the 90 s run: cam0 last-30 median 0.2413 (range 0.1951–0.3357), cam1 0.3728 (0.2636–0.4697); overshoot below/above band 0.3415/0.3343. No settled AE exposure/gain/WB exists; final per-camera exposure 8,333,332 ns, analog 10.0148/14.241, ISP 2.61048/2.61048, WB R/B 1.07555/1 on both.
6. 90.0283 s dark-start capture: steady fps 29.9183/29.9300, 0 steady gaps/drops and 0 applied-vs-requested mismatches; 2 cam1 terminal frames unpaired, 99.9607% reported steady pairing, skew p99 3.73 ms, MemAvailable min 4,307,900 KiB; cam0 startup loss 11 reported separately.
7. Bright start and 600 s steady run were not attempted after failed stay-in-band gate; strict 10-minute and 100% steady-pair acceptance are unproven. Production 0.45 still needs daylight verification. PR pending Raul's Q2 decision; do not merge or claim AE acceptance.
8. STATUS: BLOCKED — 0.30 AE did not stay in band; both hardware sessions exhausted, Jetson off.

Archived pre-Argus NVMM ring code: [`archive/vision-p2-nvmm-ring`](https://github.com/ScoutSportTechnology/sst-cam-firmware/tree/archive/vision-p2-nvmm-ring). The current gated Argus work and diagnostic summary live on `feat/vision-p2-capture`; raw HIL logs are not committed.

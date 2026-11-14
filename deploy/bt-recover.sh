#!/usr/bin/env bash
set -uo pipefail

sysfs="${SST_BT_SYSFS:-/sys}"
log() { printf '[sst-bt-recover] %s\n' "$*"; }

controller_ready() {
  local info
  if [ -n "$hciconfig" ]; then
    info="$(timeout 2 "$hciconfig" hci0 </dev/null 2>/dev/null)"
    if [[ "$info" == *'UP RUNNING'* ]] &&
       [[ "$info" =~ BD[[:space:]]Address:[[:space:]]*([[:xdigit:]:]{17}) ]] &&
       [ "${BASH_REMATCH[1]}" != 00:00:00:00:00:00 ]; then
      return 0
    fi
  fi
  if [ -n "$btmgmt" ]; then
    info="$(timeout 2 "$btmgmt" info </dev/null 2>/dev/null)"
    if [[ "$info" =~ addr[[:space:]]+([[:xdigit:]:]{17}) ]] &&
       [ "${BASH_REMATCH[1]}" != 00:00:00:00:00:00 ]; then
      return 0
    fi
  fi
  return 1
}

btmgmt="$(command -v btmgmt || true)"
hciconfig="$(command -v hciconfig || true)"
if ! command -v timeout >/dev/null 2>&1; then
  log 'timeout unavailable; skipping reset'
  exit 0
fi
if [ -z "$btmgmt" ] && [ -z "$hciconfig" ]; then
  log 'no controller diagnostic tool (btmgmt/hciconfig); skipping reset'
  exit 0
fi

wait_for_controller() {
  local deadline=$((SECONDS + 20))
  while [ "$SECONDS" -lt "$deadline" ]; do
    controller_ready && return 0
    [ "$SECONDS" -lt "$deadline" ] && sleep 1
  done
  return 1
}

log 'checking controller readiness'
if controller_ready; then
  log 'controller ready; no reset needed'
  exit 0
fi
if wait_for_controller; then
  log 'controller ready; no reset needed'
  exit 0
fi

for attempt in 1 2; do
  found=no
  fallback=''
  matches=0
  selected=''
  for iface in "$sysfs"/bus/usb/devices/*:1.*; do
    [ -L "$iface/driver" ] || continue
    driver="$(basename "$(readlink -f "$iface/driver")")"
    [ "$driver" = rtk_btusb ] || [ "$driver" = btusb ] || continue
    [ -r "$iface/bInterfaceClass" ] || continue
    [ "$(< "$iface/bInterfaceClass")" = e0 ] || continue
    device="${iface%:1.*}"
    if [ ! -r "$device/idVendor" ] || [ ! -r "$device/idProduct" ] ||
       [ ! -w "$sysfs/bus/usb/drivers/$driver/unbind" ] ||
       [ ! -w "$sysfs/bus/usb/drivers/$driver/bind" ]; then
      continue
    fi
    if [ "$(< "$device/idVendor")" = 13d3 ] && [ "$(< "$device/idProduct")" = 3549 ]; then
      selected="$iface"
      break
    fi
    fallback="$iface"
    matches=$((matches + 1))
  done
  if [ -z "$selected" ] && [ "$matches" -eq 1 ]; then
    selected="$fallback"
  fi
  if [ -n "$selected" ]; then
    iface="$selected"
    driver="$(basename "$(readlink -f "$iface/driver")")"
    device="${iface%:1.*}"
    found=yes
    log "reset $attempt/2: rebind $(basename "$iface") ($(< "$device/idVendor"):$(< "$device/idProduct")) via $driver"
    if ! printf '%s' "${iface##*/}" > "$sysfs/bus/usb/drivers/$driver/unbind"; then
      log "reset $attempt/2: unbind failed"
      continue
    fi
    if ! printf '%s' "${iface##*/}" > "$sysfs/bus/usb/drivers/$driver/bind"; then
      log "reset $attempt/2: bind failed; retrying same interface"
      if ! printf '%s' "${iface##*/}" > "$sysfs/bus/usb/drivers/$driver/bind"; then
        log 'bind failed twice; controller may be detached'
        exit 0
      fi
    fi
  fi
  if [ "$found" = no ]; then
    log 'no bound Bluetooth USB interface found; leaving boot degraded'
    exit 0
  fi
  if wait_for_controller; then
    log "controller ready after reset $attempt/2"
    exit 0
  fi
done
log 'controller still unavailable after 2 resets; firmware will retry BLE'
exit 0

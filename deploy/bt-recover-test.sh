#!/usr/bin/env bash
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
sandbox="$(mktemp -d)"
trap 'rm -rf "$sandbox"' EXIT
mkdir -p "$sandbox/sys/bus/usb/devices/1-3:1.0" "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/sys/class/bluetooth/hci0" "$sandbox/bin"
ln -s "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/sys/bus/usb/devices/1-3:1.0/driver"
printf 'e0\n' > "$sandbox/sys/bus/usb/devices/1-3:1.0/bInterfaceClass"
printf '00\n' > "$sandbox/sys/bus/usb/devices/1-3:1.0/bInterfaceNumber"
mkdir -p "$sandbox/sys/bus/usb/devices/1-3"
printf '13d3\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
printf '3549\n' > "$sandbox/sys/bus/usb/devices/1-3/idProduct"
: > "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind"
: > "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
# shellcheck disable=SC2016
printf '#!/bin/sh\nif [ -s "$SST_BT_SYSFS/bus/usb/drivers/rtk_btusb/bind" ] && [ "${SST_TEST_RECOVER:-no}" = yes ]; then printf "F8:3D:C6:1F:8C:70\\n" > "$SST_BT_SYSFS/class/bluetooth/hci0/address"; fi\n' > "$sandbox/bin/sleep"
chmod +x "$sandbox/bin/sleep"

run_case() {
  local name="$1" address="$2" recover="$3" expected="$4" output
  printf '%s\n' "$address" > "$sandbox/sys/class/bluetooth/hci0/address"
  : > "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind"
  : > "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
  if ! output="$(PATH="$sandbox/bin:$PATH" SST_BT_SYSFS="$sandbox/sys" SST_TEST_RECOVER="$recover" bash "$here/bt-recover.sh")"; then
    printf 'FAIL %s: script returned nonzero\n' "$name" >&2
    return 1
  fi
  if [ "$(< "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind")" != "$expected" ] ||
     [ "$(< "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind")" != "$expected" ]; then
    printf 'FAIL %s: wrong USB rebind\n' "$name" >&2
    return 1
  fi
  case "$name" in
    healthy) [[ "$output" == *'controller ready'* ]] ;;
    recovered) [[ "$output" == *'controller ready after reset'* ]] ;;
    exhausted) [[ "$output" == *'reset 1/2:'* && "$output" == *'reset 2/2:'* && "$output" == *'still unavailable after 2 resets'* ]] ;;
    fallback) [[ "$output" == *'no bound Bluetooth USB interface found'* ]] ;;
  esac || { printf 'FAIL %s: wrong outcome: %s\n' "$name" "$output" >&2; return 1; }
}

mkdir -p "$sandbox/sys/bus/usb/devices/0-1:1.0" "$sandbox/sys/bus/usb/devices/0-1"
ln -s "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/sys/bus/usb/devices/0-1:1.0/driver"
printf 'e0\n' > "$sandbox/sys/bus/usb/devices/0-1:1.0/bInterfaceClass"
printf '1234\n' > "$sandbox/sys/bus/usb/devices/0-1/idVendor"
printf '5678\n' > "$sandbox/sys/bus/usb/devices/0-1/idProduct"
run_case healthy F8:3D:C6:1F:8C:70 no '' || exit 1
run_case recovered 00:00:00:00:00:00 yes '1-3:1.0' || exit 1
run_case exhausted 00:00:00:00:00:00 no '1-3:1.0' || exit 1
printf 'abcd\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
run_case fallback 00:00:00:00:00:00 no '' || exit 1
printf '13d3\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
rm "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
ln -s /dev/full "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
printf '00:00:00:00:00:00\n' > "$sandbox/sys/class/bluetooth/hci0/address"
if output="$(PATH="$sandbox/bin:$PATH" SST_BT_SYSFS="$sandbox/sys" bash "$here/bt-recover.sh" 2>/dev/null)" &&
   [[ "$output" == *'bind failed twice; controller may be detached'* ]]; then
  :
else
  printf 'FAIL bind failure must stop after same-interface retry\n' >&2
  exit 1
fi
printf 'bt-recover-test.sh: 5 passed\n'

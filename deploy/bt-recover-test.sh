#!/usr/bin/env bash
set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
sandbox="$(mktemp -d)"
trap 'rm -rf "$sandbox"' EXIT
mkdir -p "$sandbox/sys/bus/usb/devices/1-3:1.0" "$sandbox/sys/bus/usb/devices/1-3" "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/bin"
ln -s "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/sys/bus/usb/devices/1-3:1.0/driver"
printf 'e0\n' > "$sandbox/sys/bus/usb/devices/1-3:1.0/bInterfaceClass"
printf '13d3\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
printf '3549\n' > "$sandbox/sys/bus/usb/devices/1-3/idProduct"
: > "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind"
: > "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
# shellcheck disable=SC2016
printf '#!/bin/sh\nif [ "${SST_TEST_CONTROLLER:-missing}" = healthy ] || { [ "${SST_TEST_CONTROLLER:-missing}" = recovered ] && [ -s "$SST_BT_SYSFS/bus/usb/drivers/rtk_btusb/bind" ]; }; then printf "Index list with 1 item\\nhci0: Primary controller\\n addr F8:3D:C6:1F:8C:70 version 10\\n"; else printf "Index list with 0 items\\n"; fi\n' > "$sandbox/bin/btmgmt"
# shellcheck disable=SC2016
printf '#!/bin/sh\nif [ "${SST_TEST_HCICONFIG:-missing}" = healthy ]; then printf "hci0: Type: Primary Bus: USB\\n BD Address: F8:3D:C6:1F:8C:70\\n UP RUNNING\\n"; else printf "hci0: Type: Primary Bus: USB\\n BD Address: 00:00:00:00:00:00\\n DOWN\\n"; fi\n' > "$sandbox/bin/hciconfig"
# shellcheck disable=SC2016
printf '#!/bin/sh\nexit 0\n' > "$sandbox/bin/sleep"
chmod +x "$sandbox/bin/"*

run_case() {
  local name="$1" state="$2" expected="$3" output
  : > "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind"
  : > "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
  if ! output="$(PATH="$sandbox/bin:$PATH" SST_BT_SYSFS="$sandbox/sys" SST_TEST_CONTROLLER="$state" bash "$here/bt-recover.sh")"; then
    printf 'FAIL %s: script returned nonzero\n' "$name" >&2
    return 1
  fi
  if [ "$(< "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind")" != "$expected" ] ||
     [ "$(< "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind")" != "$expected" ]; then
    printf 'FAIL %s: wrong USB rebind\n' "$name" >&2
    return 1
  fi
  case "$name" in
    healthy|hciconfig) [[ "$output" == *'controller ready; no reset needed'* ]] ;;
    recovered) [[ "$output" == *'controller ready after reset 1/2'* ]] ;;
    exhausted) [[ "$output" == *'reset 1/2:'* && "$output" == *'reset 2/2:'* && "$output" == *'still unavailable after 2 resets'* ]] ;;
    fallback) [[ "$output" == *'no bound Bluetooth USB interface found'* ]] ;;
  esac || { printf 'FAIL %s: wrong outcome: %s\n' "$name" "$output" >&2; return 1; }
}

mkdir -p "$sandbox/sys/bus/usb/devices/0-1:1.0" "$sandbox/sys/bus/usb/devices/0-1"
ln -s "$sandbox/sys/bus/usb/drivers/rtk_btusb" "$sandbox/sys/bus/usb/devices/0-1:1.0/driver"
printf 'e0\n' > "$sandbox/sys/bus/usb/devices/0-1:1.0/bInterfaceClass"
printf '1234\n' > "$sandbox/sys/bus/usb/devices/0-1/idVendor"
printf '5678\n' > "$sandbox/sys/bus/usb/devices/0-1/idProduct"
run_case healthy healthy '' || exit 1
run_case recovered recovered '1-3:1.0' || exit 1
run_case exhausted missing '1-3:1.0' || exit 1
printf 'abcd\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
run_case fallback missing '' || exit 1
printf '13d3\n' > "$sandbox/sys/bus/usb/devices/1-3/idVendor"
if output="$(PATH="$sandbox/bin:$PATH" SST_BT_SYSFS="$sandbox/sys" SST_TEST_CONTROLLER=missing SST_TEST_HCICONFIG=healthy bash "$here/bt-recover.sh")" &&
   [[ "$output" == *'controller ready; no reset needed'* ]] &&
   [ ! -s "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind" ]; then
  :
else
  printf 'FAIL hciconfig-ready controller must not be reset\n' >&2
  exit 1
fi
rm "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
ln -s /dev/full "$sandbox/sys/bus/usb/drivers/rtk_btusb/bind"
if ! output="$(PATH="$sandbox/bin:$PATH" SST_BT_SYSFS="$sandbox/sys" SST_TEST_CONTROLLER=missing bash "$here/bt-recover.sh" 2>/dev/null)" ||
   [[ "$output" != *'bind failed twice; controller may be detached'* ]] ||
   [ "$(< "$sandbox/sys/bus/usb/drivers/rtk_btusb/unbind")" != '1-3:1.0' ]; then
  printf 'FAIL bind failure must stop after same-interface retry\n' >&2
  exit 1
fi
printf 'bt-recover-test.sh: 6 passed\n'

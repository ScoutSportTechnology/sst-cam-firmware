#pragma once

#include "domain/config/models/device.hpp"

namespace sst::config::domain {

inline auto apply_patch(DeviceStaticIpData& modifiedData, const DeviceStaticIpData& defaultData)
    -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.ip_address) {
        modifiedData.ip_address = defaultData.ip_address;
    }
    if (defaultData.subnet_mask) {
        modifiedData.subnet_mask = defaultData.subnet_mask;
    }
    if (defaultData.gateway) {
        modifiedData.gateway = defaultData.gateway;
    }
}

inline auto apply_patch(DeviceConnectivityWifiClientData& modifiedData,
                        const DeviceConnectivityWifiClientData& defaultData) -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.wifi_ssid) {
        modifiedData.wifi_ssid = defaultData.wifi_ssid;
    }
    if (defaultData.wifi_password) {
        modifiedData.wifi_password = defaultData.wifi_password;
    }
    if (defaultData.static_ip) {
        if (!modifiedData.static_ip) {
            modifiedData.static_ip = DeviceStaticIpData{};
        }
        apply_patch(*modifiedData.static_ip, *defaultData.static_ip);
    }
}

inline auto apply_patch(DeviceConnectivityWifiAccessPointData& modifiedData,
                        const DeviceConnectivityWifiAccessPointData& defaultData) -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.ssid) {
        modifiedData.ssid = defaultData.ssid;
    }
    if (defaultData.password) {
        modifiedData.password = defaultData.password;
    }
}

inline auto apply_patch(DeviceConnectivityWifiData& modifiedData,
                        const DeviceConnectivityWifiData& defaultData) -> void {
    if (defaultData.client) {
        if (!modifiedData.client) {
            modifiedData.client = DeviceConnectivityWifiClientData{};
        }
        apply_patch(*modifiedData.client, *defaultData.client);
    }
    if (defaultData.access_point) {
        if (!modifiedData.access_point) {
            modifiedData.access_point = DeviceConnectivityWifiAccessPointData{};
        }
        apply_patch(*modifiedData.access_point, *defaultData.access_point);
    }
}

inline auto apply_patch(DeviceConnectivityEthernetData& modifiedData,
                        const DeviceConnectivityEthernetData& defaultData) -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.static_ip) {
        if (!modifiedData.static_ip) {
            modifiedData.static_ip = DeviceStaticIpData{};
        }
        apply_patch(*modifiedData.static_ip, *defaultData.static_ip);
    }
}

inline auto apply_patch(DeviceConnectivityBluetoothData& modifiedData,
                        const DeviceConnectivityBluetoothData& defaultData) -> void {
    if (defaultData.enabled) {
        modifiedData.enabled = defaultData.enabled;
    }
    if (defaultData.name) {
        modifiedData.name = defaultData.name;
    }
    if (defaultData.password) {
        modifiedData.password = defaultData.password;
    }
}

inline auto apply_patch(DeviceConnectivityData& modifiedData,
                        const DeviceConnectivityData& defaultData) -> void {
    if (defaultData.wifi) {
        if (!modifiedData.wifi) {
            modifiedData.wifi = DeviceConnectivityWifiData{};
        }
        apply_patch(*modifiedData.wifi, *defaultData.wifi);
    }
    if (defaultData.ethernet) {
        if (!modifiedData.ethernet) {
            modifiedData.ethernet = DeviceConnectivityEthernetData{};
        }
        apply_patch(*modifiedData.ethernet, *defaultData.ethernet);
    }
    if (defaultData.bluetooth) {
        if (!modifiedData.bluetooth) {
            modifiedData.bluetooth = DeviceConnectivityBluetoothData{};
        }
        apply_patch(*modifiedData.bluetooth, *defaultData.bluetooth);
    }
}

inline auto apply_patch(DeviceData& modifiedData, const DeviceData& defaultData) -> void {
    if (defaultData.name) {
        modifiedData.name = defaultData.name;
    }
    if (defaultData.model) {
        modifiedData.model = defaultData.model;
    }
    if (defaultData.version) {
        modifiedData.version = defaultData.version;
    }
    if (defaultData.serial_number) {
        modifiedData.serial_number = defaultData.serial_number;
    }
    if (defaultData.manufacturer) {
        modifiedData.manufacturer = defaultData.manufacturer;
    }
    if (defaultData.timezone) {
        modifiedData.timezone = defaultData.timezone;
    }
    if (defaultData.timestamp) {
        modifiedData.timestamp = defaultData.timestamp;
    }
    if (defaultData.connectivity) {
        if (!modifiedData.connectivity) {
            modifiedData.connectivity = DeviceConnectivityData{};
        }
        apply_patch(*modifiedData.connectivity, *defaultData.connectivity);
    }
}

}  // namespace sst::config::domain

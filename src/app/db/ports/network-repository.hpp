#pragma once

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/network.hpp"

namespace sst::db {

class INetworkRepository {
   public:
    virtual ~INetworkRepository() = default;
    virtual auto getWifiDirect(int64_t user_id) -> DbResult<NetworkWifiDirect> = 0;
    virtual auto saveWifiDirect(const NetworkWifiDirect& data) -> DbResult<NetworkWifiDirect> = 0;
    virtual auto getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> = 0;
    virtual auto saveBluetooth(const NetworkBluetooth& data) -> DbResult<NetworkBluetooth> = 0;
};

}  // namespace sst::db

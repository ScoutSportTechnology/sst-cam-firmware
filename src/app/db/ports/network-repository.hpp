#pragma once

#include "domain/db/models/db-result.hpp"
#include "domain/db/models/network.hpp"

namespace sst::db {

class INetworkRepository {
   public:
    virtual ~INetworkRepository() = default;
    virtual auto getClient(int64_t user_id) -> DbResult<NetworkClient> = 0;
    virtual auto saveClient(const NetworkClient& data) -> DbResult<NetworkClient> = 0;
    virtual auto getAccessPoint(int64_t user_id) -> DbResult<NetworkAccessPoint> = 0;
    virtual auto saveAccessPoint(const NetworkAccessPoint& data) -> DbResult<NetworkAccessPoint> = 0;
    virtual auto getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> = 0;
    virtual auto saveBluetooth(const NetworkBluetooth& data) -> DbResult<NetworkBluetooth> = 0;
};

}  // namespace sst::db

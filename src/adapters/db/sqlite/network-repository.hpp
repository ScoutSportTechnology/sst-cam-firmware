#pragma once

#include "adapters/db/sqlite/db-helpers.hpp"
#include "app/db/ports/network-repository.hpp"

namespace sst::db {

class SqliteNetworkRepository : public INetworkRepository, private SqliteRepositoryBase {
   public:
    explicit SqliteNetworkRepository(DbConnection& conn);

    auto getWifiDirect(int64_t user_id) -> DbResult<NetworkWifiDirect> override;
    auto saveWifiDirect(const NetworkWifiDirect& data) -> DbResult<NetworkWifiDirect> override;
    auto getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> override;
    auto saveBluetooth(const NetworkBluetooth& data) -> DbResult<NetworkBluetooth> override;
};

}  // namespace sst::db

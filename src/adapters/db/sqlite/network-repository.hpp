#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include "adapters/db/sqlite/db-connection.hpp"
#include "app/db/ports/network-repository.hpp"

namespace sst::db {

class SqliteNetworkRepository : public INetworkRepository {
   public:
    explicit SqliteNetworkRepository(DbConnection& conn);

    auto getClient(int64_t user_id) -> DbResult<NetworkClient> override;
    auto saveClient(const NetworkClient& data) -> DbResult<NetworkClient> override;
    auto getAccessPoint(int64_t user_id) -> DbResult<NetworkAccessPoint> override;
    auto saveAccessPoint(const NetworkAccessPoint& data) -> DbResult<NetworkAccessPoint> override;
    auto getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> override;
    auto saveBluetooth(const NetworkBluetooth& data) -> DbResult<NetworkBluetooth> override;

   private:
    SQLite::Database& db_;
};

}  // namespace sst::db

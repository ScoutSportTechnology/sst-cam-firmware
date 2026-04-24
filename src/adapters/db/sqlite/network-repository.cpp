#include "adapters/db/sqlite/network-repository.hpp"

namespace sst::db {

SqliteNetworkRepository::SqliteNetworkRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

namespace {

constexpr const char* kSelectNetClientCols =
    "SELECT user_id, enabled, medium, ssid, wifi_password, static_ip, "
    "ip_address, subnet_mask, gateway FROM config_client WHERE user_id = ?";

constexpr const char* kSelectNetApCols =
    "SELECT user_id, enabled, medium, ssid, wifi_password, static_ip, "
    "ip_address, subnet_mask, gateway FROM config_access_point WHERE user_id = ?";

template <typename T>
auto readWifiNode(SQLite::Statement& stmt) -> T {
    ColumnReader col(stmt);
    return {.user_id = col.nextI64(),
            .enabled = col.nextBool(),
            .medium = col.nextEnum<NetworkMedium>(),
            .ssid = col.nextOptText(),
            .wifi_password = col.nextOptText(),
            .static_ip = col.nextBool(),
            .ip_address = col.nextOptText(),
            .subnet_mask = col.nextOptText(),
            .gateway = col.nextOptText()};
}

template <typename T>
void bindWifiNode(SQLite::Statement& stmt, const T& data) {
    ParamBinder(stmt)
        .i64(data.user_id)
        .boolean(data.enabled)
        .asEnum(data.medium)
        .optText(data.ssid)
        .optText(data.wifi_password)
        .boolean(data.static_ip)
        .optText(data.ip_address)
        .optText(data.subnet_mask)
        .optText(data.gateway);
}

}  // namespace

auto SqliteNetworkRepository::getClient(int64_t user_id) -> DbResult<NetworkClient> {
    return dbExecute<NetworkClient>("NetworkRepository::getClient", [&] {
        SQLite::Statement stmt(db_, kSelectNetClientCols);
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkClient>::fail();
        }
        return DbResult<NetworkClient>::ok(readWifiNode<NetworkClient>(stmt));
    });
}

auto SqliteNetworkRepository::saveClient(const NetworkClient& data) -> DbResult<NetworkClient> {
    return dbExecute<NetworkClient>("NetworkRepository::saveClient", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_client "
            "(user_id, enabled, medium, ssid, wifi_password, static_ip, "
            "ip_address, subnet_mask, gateway) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        bindWifiNode(stmt, data);
        stmt.exec();
        return DbResult<NetworkClient>::ok(data);
    });
}

auto SqliteNetworkRepository::getAccessPoint(int64_t user_id) -> DbResult<NetworkAccessPoint> {
    return dbExecute<NetworkAccessPoint>("NetworkRepository::getAccessPoint", [&] {
        SQLite::Statement stmt(db_, kSelectNetApCols);
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkAccessPoint>::fail();
        }
        return DbResult<NetworkAccessPoint>::ok(readWifiNode<NetworkAccessPoint>(stmt));
    });
}

auto SqliteNetworkRepository::saveAccessPoint(const NetworkAccessPoint& data)
    -> DbResult<NetworkAccessPoint> {
    return dbExecute<NetworkAccessPoint>("NetworkRepository::saveAccessPoint", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_access_point "
            "(user_id, enabled, medium, ssid, wifi_password, static_ip, "
            "ip_address, subnet_mask, gateway) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        bindWifiNode(stmt, data);
        stmt.exec();
        return DbResult<NetworkAccessPoint>::ok(data);
    });
}

auto SqliteNetworkRepository::getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> {
    return dbExecute<NetworkBluetooth>("NetworkRepository::getBluetooth", [&] {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, enabled, name, password FROM config_bluetooth WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkBluetooth>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<NetworkBluetooth>::ok({.user_id = col.nextI64(),
                                              .enabled = col.nextBool(),
                                              .name = col.nextText(),
                                              .password = col.nextText()});
    });
}

auto SqliteNetworkRepository::saveBluetooth(const NetworkBluetooth& data)
    -> DbResult<NetworkBluetooth> {
    return dbExecute<NetworkBluetooth>("NetworkRepository::saveBluetooth", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_bluetooth (user_id, enabled, name, password) "
            "VALUES (?, ?, ?, ?)");
        ParamBinder(stmt).i64(data.user_id).boolean(data.enabled).text(data.name).text(
            data.password);
        stmt.exec();
        return DbResult<NetworkBluetooth>::ok(data);
    });
}

}  // namespace sst::db

#include "adapters/db/sqlite/network-repository.hpp"

namespace sst::db {

SqliteNetworkRepository::SqliteNetworkRepository(DbConnection& conn)
    : SqliteRepositoryBase(conn) {}

auto SqliteNetworkRepository::getWifiDirect(int64_t user_id) -> DbResult<NetworkWifiDirect> {
    return dbExecute<NetworkWifiDirect>("NetworkRepository::getWifiDirect", [&] {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, enabled, ssid, passphrase, channel, ip_address "
            "FROM config_wifi_direct WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkWifiDirect>::fail();
        }
        ColumnReader col(stmt);
        return DbResult<NetworkWifiDirect>::ok({.user_id = col.nextI64(),
                                                .enabled = col.nextBool(),
                                                .ssid = col.nextText(),
                                                .passphrase = col.nextText(),
                                                .channel = col.nextI32(),
                                                .ip_address = col.nextOptText()});
    });
}

auto SqliteNetworkRepository::saveWifiDirect(const NetworkWifiDirect& data)
    -> DbResult<NetworkWifiDirect> {
    return dbExecute<NetworkWifiDirect>("NetworkRepository::saveWifiDirect", [&] {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_wifi_direct "
            "(user_id, enabled, ssid, passphrase, channel, ip_address) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        ParamBinder(stmt)
            .i64(data.user_id)
            .boolean(data.enabled)
            .text(data.ssid)
            .text(data.passphrase)
            .i32(data.channel)
            .optText(data.ip_address);
        stmt.exec();
        return DbResult<NetworkWifiDirect>::ok(data);
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

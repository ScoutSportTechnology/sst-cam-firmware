#include "adapters/db/sqlite/network-repository.hpp"

#include <spdlog/spdlog.h>

namespace sst::db {

namespace {

auto readOptionalText(const SQLite::Column& col) -> std::optional<std::string> {
    if (col.isNull()) {
        return std::nullopt;
    }
    return std::string{col.getText()};
}

void bindOptionalText(SQLite::Statement& stmt, int idx, const std::optional<std::string>& val) {
    if (val) {
        stmt.bind(idx, *val);
    } else {
        stmt.bind(idx);
    }
}

constexpr const char* kSelectNetClientCols =
    "SELECT user_id, enabled, medium, ssid, wifi_password, static_ip, "
    "ip_address, subnet_mask, gateway FROM config_client WHERE user_id = ?";

constexpr const char* kSelectNetApCols =
    "SELECT user_id, enabled, medium, ssid, wifi_password, static_ip, "
    "ip_address, subnet_mask, gateway FROM config_access_point WHERE user_id = ?";

template <typename T>
auto readNetworkNode(SQLite::Statement& stmt) -> T {
    T out;
    int col = 0;
    out.user_id = stmt.getColumn(col++).getInt64();
    out.enabled = static_cast<bool>(stmt.getColumn(col++).getInt());
    out.medium = static_cast<NetworkMedium>(stmt.getColumn(col++).getInt());
    out.ssid = readOptionalText(stmt.getColumn(col++));
    out.wifi_password = readOptionalText(stmt.getColumn(col++));
    out.static_ip = static_cast<bool>(stmt.getColumn(col++).getInt());
    out.ip_address = readOptionalText(stmt.getColumn(col++));
    out.subnet_mask = readOptionalText(stmt.getColumn(col++));
    out.gateway = readOptionalText(stmt.getColumn(col));
    return out;
}

template <typename T>
void bindNetworkNode(SQLite::Statement& stmt, const T& data) {
    int param = 1;
    stmt.bind(param++, data.user_id);
    stmt.bind(param++, static_cast<int>(data.enabled));
    stmt.bind(param++, static_cast<int>(data.medium));
    bindOptionalText(stmt, param++, data.ssid);
    bindOptionalText(stmt, param++, data.wifi_password);
    stmt.bind(param++, static_cast<int>(data.static_ip));
    bindOptionalText(stmt, param++, data.ip_address);
    bindOptionalText(stmt, param++, data.subnet_mask);
    bindOptionalText(stmt, param, data.gateway);
}

}  // namespace

SqliteNetworkRepository::SqliteNetworkRepository(DbConnection& conn) : db_(conn.db()) {}

auto SqliteNetworkRepository::getClient(int64_t user_id) -> DbResult<NetworkClient> {
    try {
        SQLite::Statement stmt(db_, kSelectNetClientCols);
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkClient>::fail();
        }
        return DbResult<NetworkClient>::ok(readNetworkNode<NetworkClient>(stmt));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::getClient failed: {}", ex.what());
        return DbResult<NetworkClient>::fail();
    }
}

auto SqliteNetworkRepository::saveClient(const NetworkClient& data) -> DbResult<NetworkClient> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_client "
            "(user_id, enabled, medium, ssid, wifi_password, static_ip, "
            "ip_address, subnet_mask, gateway) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        bindNetworkNode(stmt, data);
        stmt.exec();
        return DbResult<NetworkClient>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::saveClient failed: {}", ex.what());
        return DbResult<NetworkClient>::fail();
    }
}

auto SqliteNetworkRepository::getAccessPoint(int64_t user_id) -> DbResult<NetworkAccessPoint> {
    try {
        SQLite::Statement stmt(db_, kSelectNetApCols);
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkAccessPoint>::fail();
        }
        return DbResult<NetworkAccessPoint>::ok(readNetworkNode<NetworkAccessPoint>(stmt));
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::getAccessPoint failed: {}", ex.what());
        return DbResult<NetworkAccessPoint>::fail();
    }
}

auto SqliteNetworkRepository::saveAccessPoint(const NetworkAccessPoint& data)
    -> DbResult<NetworkAccessPoint> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_access_point "
            "(user_id, enabled, medium, ssid, wifi_password, static_ip, "
            "ip_address, subnet_mask, gateway) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        bindNetworkNode(stmt, data);
        stmt.exec();
        return DbResult<NetworkAccessPoint>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::saveAccessPoint failed: {}", ex.what());
        return DbResult<NetworkAccessPoint>::fail();
    }
}

auto SqliteNetworkRepository::getBluetooth(int64_t user_id) -> DbResult<NetworkBluetooth> {
    try {
        SQLite::Statement stmt(
            db_,
            "SELECT user_id, enabled, name, password FROM config_bluetooth WHERE user_id = ?");
        stmt.bind(1, user_id);
        if (!stmt.executeStep()) {
            return DbResult<NetworkBluetooth>::fail();
        }
        int col = 0;
        return DbResult<NetworkBluetooth>::ok(
            {.user_id = stmt.getColumn(col++).getInt64(),
             .enabled = static_cast<bool>(stmt.getColumn(col++).getInt()),
             .name = stmt.getColumn(col++).getText(),
             .password = stmt.getColumn(col).getText()});
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::getBluetooth failed: {}", ex.what());
        return DbResult<NetworkBluetooth>::fail();
    }
}

auto SqliteNetworkRepository::saveBluetooth(const NetworkBluetooth& data)
    -> DbResult<NetworkBluetooth> {
    try {
        SQLite::Statement stmt(
            db_,
            "INSERT OR REPLACE INTO config_bluetooth (user_id, enabled, name, password) "
            "VALUES (?, ?, ?, ?)");
        int param = 1;
        stmt.bind(param++, data.user_id);
        stmt.bind(param++, static_cast<int>(data.enabled));
        stmt.bind(param++, data.name);
        stmt.bind(param, data.password);
        stmt.exec();
        return DbResult<NetworkBluetooth>::ok(data);
    } catch (const SQLite::Exception& ex) {
        spdlog::error("NetworkRepository::saveBluetooth failed: {}", ex.what());
        return DbResult<NetworkBluetooth>::fail();
    }
}

}  // namespace sst::db

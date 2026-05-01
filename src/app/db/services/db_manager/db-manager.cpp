#include "app/db/services/db_manager/db-manager.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "adapters/db/sqlite/camera-repository.hpp"
#include "adapters/db/sqlite/db-connection.hpp"
#include "adapters/db/sqlite/match-repository.hpp"
#include "adapters/db/sqlite/microphone-repository.hpp"
#include "adapters/db/sqlite/network-repository.hpp"
#include "adapters/db/sqlite/recording-repository.hpp"
#include "adapters/db/sqlite/sport-repository.hpp"
#include "adapters/db/sqlite/stream-config-repository.hpp"
#include "adapters/db/sqlite/team-repository.hpp"
#include "adapters/db/sqlite/user-repository.hpp"

namespace sst::db {

DbManager::DbManager(const Config& config)
    : conn_(std::make_unique<DbConnection>(config.db_path)) {
    if (!config.schema_path.empty()) {
        initSchema(config.schema_path);
    }
    users_ = std::make_unique<SqliteUserRepository>(*conn_);
    network_ = std::make_unique<SqliteNetworkRepository>(*conn_);
    streams_ = std::make_unique<SqliteStreamConfigRepository>(*conn_);
    cameras_ = std::make_unique<SqliteCameraRepository>(*conn_);
    microphones_ = std::make_unique<SqliteMicrophoneRepository>(*conn_);
    sports_ = std::make_unique<SqliteSportRepository>(*conn_);
    teams_ = std::make_unique<SqliteTeamRepository>(*conn_);
    matches_ = std::make_unique<SqliteMatchRepository>(*conn_);
    recordings_ = std::make_unique<SqliteRecordingRepository>(*conn_);
}

DbManager::~DbManager() = default;

void DbManager::initSchema(const std::string& schema_path) {
    std::ifstream file(schema_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open schema file: " + schema_path);
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    conn_->db().exec(buf.str());
    spdlog::info("DB schema applied from: {}", schema_path);
}

auto DbManager::users() -> IUserRepository& { return *users_; }
auto DbManager::network() -> INetworkRepository& { return *network_; }
auto DbManager::streams() -> IStreamConfigRepository& { return *streams_; }
auto DbManager::cameras() -> ICameraRepository& { return *cameras_; }
auto DbManager::microphones() -> IMicrophoneRepository& { return *microphones_; }
auto DbManager::sports() -> ISportRepository& { return *sports_; }
auto DbManager::teams() -> ITeamRepository& { return *teams_; }
auto DbManager::matches() -> IMatchRepository& { return *matches_; }
auto DbManager::recordings() -> IRecordingRepository& { return *recordings_; }

}  // namespace sst::db

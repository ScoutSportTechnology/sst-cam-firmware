#pragma once

#include <memory>
#include <string>

#include "app/db/ports/camera-repository.hpp"
#include "app/db/ports/microphone-repository.hpp"
#include "app/db/ports/network-repository.hpp"
#include "app/db/ports/storage-config-repository.hpp"
#include "app/db/ports/stream-config-repository.hpp"
#include "app/db/ports/user-repository.hpp"

namespace sst::db {

class DbConnection;

class DbManager {
   public:
    struct Config {
        std::string db_path;
        std::string schema_path;  // optional: run schema.sql on open if non-empty
    };

    explicit DbManager(const Config& config);
    ~DbManager();

    auto users() -> IUserRepository&;
    auto network() -> INetworkRepository&;
    auto streams() -> IStreamConfigRepository&;
    auto storage() -> IStorageConfigRepository&;
    auto cameras() -> ICameraRepository&;
    auto microphones() -> IMicrophoneRepository&;

   private:
    void initSchema(const std::string& schema_path);

    std::unique_ptr<DbConnection> conn_;
    std::unique_ptr<IUserRepository> users_;
    std::unique_ptr<INetworkRepository> network_;
    std::unique_ptr<IStreamConfigRepository> streams_;
    std::unique_ptr<IStorageConfigRepository> storage_;
    std::unique_ptr<ICameraRepository> cameras_;
    std::unique_ptr<IMicrophoneRepository> microphones_;
};

}  // namespace sst::db

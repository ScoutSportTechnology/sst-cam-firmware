#pragma once

#include "config/domain/users_config.hpp"

namespace sst::config::domain {

inline auto apply_patch(UsersData& modifiedData, const UsersData& defaultData) -> void {
    modifiedData.name = defaultData.name;
}

}  // namespace sst::config::domain

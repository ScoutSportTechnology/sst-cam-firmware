#pragma once
#include <cstdint>
#include <vector>

namespace sst::config {
template <typename UD>
struct UserArray {
    std::uint32_t id{0};
    UD user_data;
};

template <typename DD, typename UD>
struct ConfigFiles {
    DD default_data;
    std::vector<UserArray<UD>> users;
};
}  // namespace sst::config
#include <cstdint>

namespace sst::config::domain {
enum class DeviceModel : std::uint8_t { UNKNOWN, sst_cam_v1, sst_cam_v2, sst_cam_v3 };
}  // namespace sst::config::domain
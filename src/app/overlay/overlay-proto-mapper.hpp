#pragma once

#include "bluetooth.pb.h"
#include "domain/overlay/models/overlay-layout.hpp"

namespace sst::overlay {

// Map the proto OverlayLayout (framework type) into the domain value model at
// the adapter/app boundary (KTD2 — proto types never enter domain/).
auto MapLayoutFromProto(const sst_cam::OverlayLayout& proto) -> OverlayLayout;

}  // namespace sst::overlay

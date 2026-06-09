#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "domain/overlay/models/overlay-enums.hpp"
#include "domain/overlay/models/overlay-layout.hpp"
#include "domain/overlay/models/render-scene.hpp"

namespace sst::overlay {

// Live values that TEXT bindings resolve against. Updated from match events;
// read when a scene is built.
struct BindingData {
    std::uint32_t score_a{0};
    std::uint32_t score_b{0};
    std::uint32_t period{0};
    std::uint32_t clock_seconds{0};
    PeriodLabelState period_state{PeriodLabelState::kInPlay};
    std::string team_a_name;
    std::string team_b_name;
};

// Owns the active OverlayLayout, the current binding data, and any in-flight
// banner templates. Resolves bindings + {{param}} substitution + template
// activation/expiry into a flat, z-ordered RenderScene. No rendering, no GStreamer
// — pure scene logic, unit-testable on its own.
class OverlayScene {
   public:
    auto SetLayout(OverlayLayout layout) -> void;
    [[nodiscard]] auto HasLayout() const -> bool { return has_layout_; }

    auto SetBindingData(const BindingData& data) -> void;

    // Activate the template whose event_type == template_id, substituting params
    // into each element's static_text. duration_s_override > 0 overrides the
    // template's duration_ms (R19); otherwise the template duration is used.
    // Returns false if no template matches (a no-op).
    auto ActivateBanner(const std::string& template_id,
                        const std::map<std::string, std::string>& params,
                        std::uint32_t duration_s_override, std::uint64_t now_ms) -> bool;

    // Build the flattened, z-sorted scene at now_ms; expires lapsed banners.
    [[nodiscard]] auto Build(std::uint64_t now_ms) -> RenderScene;

   private:
    struct ActiveBanner {
        std::uint64_t expires_at_ms{0};
        std::vector<OverlayElement> elements;  // static_text already substituted
    };

    [[nodiscard]] auto ResolveText(const OverlayElement& element) const -> std::string;
    auto ExpireBanners(std::uint64_t now_ms) -> void;

    OverlayLayout layout_;
    bool has_layout_{false};
    BindingData data_;
    std::vector<ActiveBanner> banners_;
};

}  // namespace sst::overlay

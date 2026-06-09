#include "app/overlay/services/overlay_controller/overlay-controller.hpp"

#include <utility>

#include <fmt/format.h>

namespace sst::overlay {

OverlayController::OverlayController(IOverlayRenderer& renderer, IOverlaySink& sink,
                                    std::uint32_t out_width, std::uint32_t out_height)
    : renderer_(renderer), sink_(sink), out_width_(out_width), out_height_(out_height) {}

auto OverlayController::SetLayout(OverlayLayout layout) -> void {
    std::lock_guard lock(mtx_);
    scene_.SetLayout(std::move(layout));
}

auto OverlayController::SetBindingData(const BindingData& data) -> void {
    std::lock_guard lock(mtx_);
    scene_.SetBindingData(data);
}

auto OverlayController::ActivateBanner(const std::string& template_id,
                                       const std::map<std::string, std::string>& params,
                                       std::uint32_t duration_s_override, std::uint64_t now_ms)
    -> bool {
    std::lock_guard lock(mtx_);
    return scene_.ActivateBanner(template_id, params, duration_s_override, now_ms);
}

auto OverlayController::Signature(const RenderScene& scene) -> std::string {
    std::string sig = fmt::format("{}x{}|", scene.canvas_width, scene.canvas_height);
    for (const auto& e : scene.elements) {
        sig += fmt::format("{};{},{},{},{},{};", static_cast<int>(e.shape), e.bounds.x1,
                           e.bounds.y1, e.bounds.x2, e.bounds.y2, e.bounds.z);
        sig += e.text;
        sig += ';';
        sig += e.style.fill_color;
        sig += ';';
        sig += e.style.text_color;
        sig += fmt::format(";{};{}|", e.style.opacity, static_cast<int>(e.style.corner_radius));
    }
    return sig;
}

auto OverlayController::Refresh(std::uint64_t now_ms) -> bool {
    std::lock_guard lock(mtx_);
    const RenderScene scene = scene_.Build(now_ms);
    const std::string sig = Signature(scene);
    if (pushed_once_ && sig == last_signature_) {
        return false;  // nothing visibly changed — don't re-render or push
    }
    last_signature_ = sig;
    pushed_once_ = true;

    RgbaImage frame = renderer_.Render(scene, out_width_, out_height_);
    sink_.PushFrame(frame);
    ++push_count_;
    return true;
}

}  // namespace sst::overlay

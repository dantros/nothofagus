#pragma once

#include <functional>
#include <string>

namespace Nothofagus
{

/// Callback invoked once per Vulkan validation / debug-utils message. The string is
/// the debug-utils message-ID name (e.g. "VUID-..." or "SYNC-HAZARD-...") followed by
/// ": " and the human-readable description, so callers can key off the stable id.
/// Intended for tests and diagnostics (e.g. counting VUIDs or synchronization hazards).
using VulkanValidationCallback = std::function<void(const std::string& message)>;

/// @brief Registers a callback that receives every Vulkan validation message.
///
/// Must be set *before* constructing the Canvas whose messages you want to observe
/// (the debug messenger is created during Canvas construction). Pass `nullptr` to
/// clear. No-op on the OpenGL backend.
///
/// Setting a callback also makes the Vulkan backend create a debug messenger even in
/// release builds, so the validation layer must still be available to the loader
/// (e.g. via `VK_LAYER_PATH`) and explicitly requested feature flags (such as
/// synchronization validation via `VK_LAYER_ENABLES`) honored as usual. When the
/// layer is not loaded, the callback simply never fires.
void setVulkanValidationCallback(VulkanValidationCallback callback);

} // namespace Nothofagus

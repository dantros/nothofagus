#include "render_snapshot.h"
#include "imgui_draw_clone.h" // complete type for the unique_ptr<ClonedImDrawData> member

namespace Nothofagus
{

// Defined out-of-line so RenderSnapshot can hold a unique_ptr to the
// forward-declared ClonedImDrawData in the header (imgui.h stays out of it).
RenderSnapshot::RenderSnapshot() = default;
RenderSnapshot::~RenderSnapshot() = default;

}

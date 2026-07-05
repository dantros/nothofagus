#include <nothofagus.h>
#include <imgui.h>
#include <optional>
#include <cmath>

// Hello DPI scaling — shows Nothofagus's two ImGui regimes side by side:
//
//   * Standard UI (the main context) honors the OS content scale, so apps built
//     on Nothofagus look native on HiDPI displays. Fonts scale via
//     style.FontScaleDpi and widget metrics via ScaleAllSizes — both driven by
//     canvas.contentScale(). Move this onto a 200% monitor (or drag the override
//     slider) and the whole UI grows like any native app.
//
//   * Diegetic UI (a render target) stays in game-resolution pixels, unaffected
//     by OS DPI — the crunchy pixel-art look. Drawn via renderImguiTo().
//
// Two independent knobs are exposed:
//   - setContentScaleOverride(): replaces the OS scale (accessibility / zoom, and
//     the deterministic seam the visual tests use). Toggle "use OS scale" off to
//     drive it by hand.
//   - style.FontScaleMain: an app-level font-only zoom, orthogonal to DPI.
int main()
{
    spdlog::info("Hello DPI scaling!");

    Nothofagus::Canvas canvas({320, 240}, "Hello DPI Scaling", {0.10f, 0.11f, 0.16f}, 3);

    // --- A diegetic (game-resolution) ImGui panel rendered into an RTT. --------
    constexpr Nothofagus::ScreenSize renderTargetSize{150, 110};
    Nothofagus::RenderTargetId renderTargetId = canvas.addRenderTarget(renderTargetSize);
    canvas.setRenderTargetClearColor(renderTargetId, {0.02f, 0.05f, 0.10f, 1.0f});
    Nothofagus::BellotaId diegeticBellotaId =
        canvas.addBellota({{{232.0f, 170.0f}}, canvas.renderTargetTexture(renderTargetId)});

    // Diegetic font baked at the RTT's logical pixel height (DPI-independent).
    Nothofagus::ImguiFontId diegeticId =
        canvas.bakeImguiFont(canvas.defaultImguiFontSourceId(), 11.0f);

    // --- Standard-UI state -----------------------------------------------------
    bool   useOsScale   = true;
    float  overrideScale = 1.5f;     // applied only while useOsScale is false
    float  fontZoom      = 1.0f;     // style.FontScaleMain
    float  sliderValue   = 0.42f;
    int    counter       = 0;
    bool   checkOn       = true;
    int    comboIndex    = 0;
    float  color[3]      = {0.4f, 0.7f, 1.0f};
    char   nameBuf[64]   = "Nothofagus";
    float  time          = 0.0f;

    // TODO(threaded): the diegetic RTT panel uses renderImguiTo, which runs its user
    // callback on the render thread and has no per-RTT draw-data clone, so it is not wired
    // for the sim/render split yet (see THREADED_DIEGETIC_IMGUI.md). Parked on the
    // deprecated single-thread run(update, Controller&) until threaded support lands.
    Nothofagus::Controller deferredController;
    canvas.run([&](float deltaTimeMS)
    {
        time += deltaTimeMS;

        // Apply the chosen scale knobs to the main (standard-UI) context.
        canvas.setContentScaleOverride(useOsScale ? std::optional<float>{} : std::optional<float>{overrideScale});
        ImGui::GetStyle().FontScaleMain = fontZoom;

        // ----- Menu bar -------------------------------------------------------
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File")) { ImGui::MenuItem("New"); ImGui::MenuItem("Open"); ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Edit")) { ImGui::MenuItem("Undo"); ImGui::MenuItem("Redo"); ImGui::EndMenu(); }
            ImGui::EndMainMenuBar();
        }

        // ----- Controls: the two scaling knobs --------------------------------
        ImGui::Begin("Scaling");
        ImGui::Checkbox("use OS scale", &useOsScale);
        ImGui::BeginDisabled(useOsScale);
        ImGui::SliderFloat("override", &overrideScale, 0.75f, 3.0f, "%.2fx");
        ImGui::EndDisabled();
        ImGui::SliderFloat("font zoom (FontScaleMain)", &fontZoom, 0.5f, 2.5f, "%.2fx");
        if (ImGui::Button("reset")) { useOsScale = true; overrideScale = 1.5f; fontZoom = 1.0f; }
        ImGui::End();

        // ----- A broad widget spread so font + metric scaling are obvious -----
        ImGui::Begin("Native-style widgets");
        if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::InputText("name", nameBuf, sizeof(nameBuf));
            ImGui::SliderFloat("value", &sliderValue, 0.0f, 1.0f);
            ImGui::Checkbox("enabled", &checkOn);
            const char* items[] = {"Alpha", "Beta", "Gamma"};
            ImGui::Combo("mode", &comboIndex, items, IM_ARRAYSIZE(items));
            ImGui::ColorEdit3("tint", color);
            if (ImGui::Button("Click me")) counter++;
            ImGui::SameLine();
            ImGui::Text("count = %d", counter);
        }
        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("Table"))
            {
                if (ImGui::BeginTable("t", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("A"); ImGui::TableSetupColumn("B"); ImGui::TableSetupColumn("C");
                    ImGui::TableHeadersRow();
                    for (int row = 0; row < 3; ++row)
                    {
                        ImGui::TableNextRow();
                        for (int col = 0; col < 3; ++col)
                        {
                            ImGui::TableSetColumnIndex(col);
                            ImGui::Text("r%d c%d", row, col);
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tree"))
            {
                if (ImGui::TreeNode("Root"))
                {
                    ImGui::BulletText("Leaf one");
                    if (ImGui::TreeNode("Branch")) { ImGui::BulletText("Leaf two"); ImGui::TreePop(); }
                    ImGui::TreePop();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        // ----- Diagnostics: inspect the scaling on a real monitor -------------
        const ImGuiIO& io = ImGui::GetIO();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::Begin("Diagnostics");
        ImGui::Text("contentScale()      = %.3f", canvas.contentScale());
        ImGui::Text("DisplaySize         = %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("DisplayFbScale      = %.2f x %.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImGui::Text("GetFontSize()       = %.2f", ImGui::GetFontSize());
        ImGui::Text("imguiBaseFontSize   = %.2f", canvas.imguiBaseFontSize());
        ImGui::Text("imguiScaledFontSize = %.2f", canvas.imguiScaledFontSize());
        ImGui::Text("FramePadding        = %.1f x %.1f", style.FramePadding.x, style.FramePadding.y);
        ImGui::Text("FontScaleMain/Dpi   = %.2f / %.2f", style.FontScaleMain, style.FontScaleDpi);
        ImGui::End();

        // ----- The diegetic panel: game-resolution, NOT DPI-scaled ------------
        canvas.bellota(diegeticBellotaId).transform().angle() = 0.0003f * time;
        canvas.renderImguiTo(renderTargetId, diegeticId, [&]
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(
                static_cast<float>(renderTargetSize.width),
                static_cast<float>(renderTargetSize.height)), ImGuiCond_Always);
            ImGui::Begin("In-World", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("Diegetic UI");
            ImGui::TextUnformatted("game pixels,");
            ImGui::TextUnformatted("DPI-independent");
            ImGui::ProgressBar(0.5f + 0.5f * std::sin(0.003f * time));
            ImGui::End();
        });
    }, deferredController);

    return 0;
}

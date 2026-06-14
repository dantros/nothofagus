// visual_tests_explorer — an interactive Nothofagus tool for inspecting golden-image
// regression results across every render backend at once.
//
// For each test case it shows, in a 3x3 grid:
//   row 1            the single canonical golden (SwiftShader-authored), centered
//   row 2            each backend's "actual" render:  opengl gpu | vulkan gpu | vulkan swiftshader
//   row 3            the diff of each backend's actual vs the golden
// A backend that is not available in this build (no Vulkan SDK, or SwiftShader on a
// non-linux host) is marked [unavailable] and leaves its column empty.
//
// The explorer renders nothing itself — Nothofagus does no file I/O, so producing
// actuals means driving the rendering_tests binaries (built per backend by CMake as
// independent ExternalProjects) which own the scene definitions. This tool then
// loads/compares the PNGs via the shared nothofagus_test_helpers lib (stb_image_plus).
//
// All paths are baked in at build time; the explorer reads no environment variables.

#include <canvas.h>
#include <texture.h>
#include <bellota.h>
#include <text.h>
#include <direct_texture_io.h>
#include <direct_texture_compare.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// The single canonical golden set (SwiftShader-authored).
#ifndef VTE_GOLDEN_DIR
    #define VTE_GOLDEN_DIR "."
#endif
// Per-backend rendering_tests binaries (empty string => backend not built).
#ifndef VTE_OPENGL_TESTS_BIN
    #define VTE_OPENGL_TESTS_BIN ""
#endif
#ifndef VTE_VULKAN_TESTS_BIN
    #define VTE_VULKAN_TESTS_BIN ""
#endif
// 1 when a SwiftShader ICD is available to the Vulkan binary, else 0.
#ifndef VTE_SWIFTSHADER_AVAILABLE
    #define VTE_SWIFTSHADER_AVAILABLE 0
#endif
// Per-backend output dirs for the "actual" renders.
#ifndef VTE_ACTUAL_DIR_OPENGL_GPU
    #define VTE_ACTUAL_DIR_OPENGL_GPU "."
#endif
#ifndef VTE_ACTUAL_DIR_VULKAN_GPU
    #define VTE_ACTUAL_DIR_VULKAN_GPU "."
#endif
#ifndef VTE_ACTUAL_DIR_VULKAN_SWIFTSHADER
    #define VTE_ACTUAL_DIR_VULKAN_SWIFTSHADER "."
#endif

namespace fs = std::filesystem;

namespace
{

// One render-backend column of the grid.
struct Lane
{
    std::string name;        // "opengl gpu"
    std::string bin;         // rendering_tests binary path ("" => not built)
    std::string backendEnv;  // value for NOTHOFAGUS_RENDER_BACKEND when driving this lane
    std::string actualDir;   // where this lane's PNGs are written / read
    bool        available;   // can this backend produce actuals in this build?
};

constexpr int kLaneCount = 3;

// Set an environment variable in this process so a child launched via std::system
// inherits it. Portable across POSIX and Windows.
void setEnvVar(const char* key, const std::string& value)
{
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

// Scale that fits an image of the given size into a target box. Small pixel-art
// goldens get an integer up-scale (>= 1) for crisp nearest-neighbor sampling;
// images larger than the box get a fractional down-scale so they still fit
// within the cell instead of overflowing the 3x3 grid.
float fitScale(glm::ivec2 size, int box)
{
    if (size.x <= 0 || size.y <= 0)
        return 1.0f;
    const int longest = std::max(size.x, size.y);
    if (longest > box)
        return static_cast<float>(box) / static_cast<float>(longest); // shrink to fit
    const int byWidth  = std::max(1, box / size.x);
    const int byHeight = std::max(1, box / size.y);
    return static_cast<float>(std::max(1, std::min(byWidth, byHeight)));
}

struct Entry
{
    std::string name;                       // file stem, e.g. "single_bellota"
    std::array<bool, kLaneCount> hasActual; // whether each lane has an actual PNG
};

class VisualTestsExplorer
{
public:
    VisualTestsExplorer() :
        mCanvas({540, 470}, "Nothofagus Visual Tests Explorer", {0.12f, 0.12f, 0.14f}, 2),
        mGoldenDir(VTE_GOLDEN_DIR)
    {
        const std::string openglBin = VTE_OPENGL_TESTS_BIN;
        const std::string vulkanBin = VTE_VULKAN_TESTS_BIN;
        const bool swiftshaderAvailable = (VTE_SWIFTSHADER_AVAILABLE != 0);

        mLanes[0] = {"vulkan swiftshader", vulkanBin, "swiftshader", VTE_ACTUAL_DIR_VULKAN_SWIFTSHADER,
                     !vulkanBin.empty() && swiftshaderAvailable};
        mLanes[1] = {"vulkan gpu",         vulkanBin, "gpu",         VTE_ACTUAL_DIR_VULKAN_GPU,
                     !vulkanBin.empty()};
        mLanes[2] = {"opengl gpu",         openglBin, "auto",        VTE_ACTUAL_DIR_OPENGL_GPU,
                     !openglBin.empty()};

        setupColumnHeaders();
        setupRowHeaders();
        rescan();
    }

    void run()
    {
        mCanvas.run([this](float) { frame(); });
    }

private:
    static constexpr int kSwiftShaderLane = 0;

    std::string goldenPath(const std::string& name) const { return mGoldenDir + "/" + name + ".png"; }
    std::string actualPath(const Lane& lane, const std::string& name) const
    {
        return lane.actualDir + "/" + name + ".png";
    }

    void rescan()
    {
        mEntries.clear();
        std::error_code ec;
        if (fs::is_directory(mGoldenDir, ec))
        {
            for (const auto& dirEntry : fs::directory_iterator(mGoldenDir, ec))
            {
                if (!dirEntry.is_regular_file())
                    continue;
                const fs::path& p = dirEntry.path();
                if (p.extension() != ".png")
                    continue;
                const std::string name = p.stem().string();
                Entry entry{name, {false, false, false}};
                for (int i = 0; i < kLaneCount; ++i)
                    entry.hasActual[i] = mLanes[i].available && fs::exists(actualPath(mLanes[i], name), ec);
                mEntries.push_back(std::move(entry));
            }
            std::sort(mEntries.begin(), mEntries.end(),
                      [](const Entry& a, const Entry& b) { return a.name < b.name; });
        }
        if (mSelected >= static_cast<int>(mEntries.size()))
            mSelected = mEntries.empty() ? -1 : 0;
        rebuildSelection();
    }

    // Loads the selected case's golden + per-lane actuals/diffs and (re)creates the
    // grid of display bellotas.
    void rebuildSelection()
    {
        for (Nothofagus::BellotaId id : mBellotas)
            mCanvas.removeBellota(id);
        mBellotas.clear();

        mGolden.reset();
        for (auto& a : mActuals) a.reset();
        for (auto& d : mDiffs)   d.reset();
        mLoadError.clear();

        if (mSelected < 0 || mSelected >= static_cast<int>(mEntries.size()))
            return;

        const Entry& entry = mEntries[mSelected];
        try
        {
            mGolden = Nothofagus::TestHelpers::load(goldenPath(entry.name));
        }
        catch (const std::exception& e)
        {
            mLoadError = std::string("golden: ") + e.what();
            return;
        }

        for (int i = 0; i < kLaneCount; ++i)
        {
            if (!entry.hasActual[i])
                continue;
            try
            {
                mActuals[i] = Nothofagus::TestHelpers::load(actualPath(mLanes[i], entry.name));
                mDiffs[i]   = Nothofagus::TestHelpers::makeDiff(mActuals[i].value(), mGolden.value());
            }
            catch (const std::exception& e)
            {
                mLoadError = mLanes[i].name + ": " + e.what();
            }
        }

        layoutGrid();
    }

    // Horizontal center of each backend column.
    std::array<float, kLaneCount> columnXs() const
    {
        const auto& size = mCanvas.screenSize();
        return {size.width * 1.0f / 6.0f, size.width * 3.0f / 6.0f, size.width * 5.0f / 6.0f};
    }

    // Paints a label with the bundled font8x8 bitmap font into an IndirectTexture and
    // adds it as a plain bellota — in-game text, not ImGui. angle is in degrees.
    Nothofagus::BellotaId addLabel(const std::string& text, float x, float y, float angle = 0.0f)
    {
        Nothofagus::IndirectTexture label = Nothofagus::makeTextTexture(
            text, Nothofagus::FontType::Basic,
            {1.0f, 1.0f, 1.0f, 1.0f},   // white glyph
            {0.0f, 0.0f, 0.0f, 0.0f});  // transparent bg
        const Nothofagus::TextureId texId = mCanvas.addTexture(label);
        return mCanvas.addBellota(Nothofagus::Bellota(Nothofagus::Transform({x, y}, 1.0f, angle), texId));
    }

    // One static header label per backend column (created once; persists across
    // selections, so it is kept out of the per-selection mBellotas list).
    void setupColumnHeaders()
    {
        const auto cols = columnXs();
        const float headerY = mCanvas.screenSize().height * 0.95f;
        for (int i = 0; i < kLaneCount; ++i)
            mHeaderBellotas.push_back(addLabel(mLanes[i].name, cols[i], headerY));
    }

    // One static row label down the left edge (rotated 90° so the vertical text is
    // narrow), aligned to the golden / actual / diff row centers used by layoutGrid().
    void setupRowHeaders()
    {
        const auto& size = mCanvas.screenSize();
        const float x = size.width * 0.03f;
        mHeaderBellotas.push_back(addLabel("golden", x, size.height * 0.76f, 90.0f));
        mHeaderBellotas.push_back(addLabel("actual", x, size.height * 0.45f, 90.0f));
        mHeaderBellotas.push_back(addLabel("diff",   x, size.height * 0.15f, 90.0f));
    }

    // Grid under the header row: golden centered on the top row; each lane's actual in
    // the middle row and its diff in the bottom row, aligned to the lane's column.
    // Unavailable/empty cells just leave their reserved space blank.
    void layoutGrid()
    {
        const auto& size = mCanvas.screenSize();
        const auto cols = columnXs();
        const float rowGoldenY = size.height * 0.76f;
        const float rowActualY = size.height * 0.45f;
        const float rowDiffY   = size.height * 0.15f;
        constexpr int displayBox = 120;

        auto place = [&](std::optional<Nothofagus::DirectTexture>& tex, float x, float y)
        {
            if (!tex.has_value())
                return;
            const Nothofagus::TextureId texId = mCanvas.addTexture(tex.value());
            const float scale = fitScale(tex.value().size(), displayBox);
            mBellotas.push_back(mCanvas.addBellota(Nothofagus::Bellota(
                Nothofagus::Transform({x, y}, scale), texId)));
        };

        // Golden sits above the swiftshader column: it is the swiftshader-authored
        // reference, so the swiftshader actual directly below it should match exactly.
        place(mGolden, cols[kSwiftShaderLane], rowGoldenY);

        for (int i = 0; i < kLaneCount; ++i)
        {
            place(mActuals[i], cols[i], rowActualY);
            place(mDiffs[i],   cols[i], rowDiffY);
        }
    }

    // Comparison of a lane's actual against the golden (live, so tolerance sliders
    // update immediately). Empty if that lane has no loaded actual.
    std::optional<Nothofagus::TestHelpers::ComparisonResult> laneResult(int lane) const
    {
        if (!mGolden.has_value() || !mActuals[lane].has_value())
            return std::nullopt;
        return Nothofagus::TestHelpers::compare(mActuals[lane].value(), mGolden.value(),
                                    static_cast<std::uint8_t>(mTolerance),
                                    static_cast<std::size_t>(mMaxDiffPixels));
    }

    void frame()
    {
        drawControls();
        drawSelectedStats();
    }

    void drawControls()
    {
        ImGui::SetNextWindowPos({8, 8}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({360, 600}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Visual Tests Explorer");

        ImGui::TextWrapped("Grid: golden (top, centered) | actuals (middle) | diffs vs golden (bottom). "
                           "Columns are the backends below, left to right.");
        ImGui::Separator();

        ImGui::TextUnformatted("Backends:");
        for (int i = 0; i < kLaneCount; ++i)
        {
            ImGui::BulletText("%s", mLanes[i].name.c_str());
            if (!mLanes[i].available)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, {0.7f, 0.7f, 0.4f, 1.0f});
                ImGui::TextUnformatted("[unavailable]");
                ImGui::PopStyleColor();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Generate actual images"))
            generateActualImages();
        ImGui::SameLine();
        if (ImGui::Button("Rescan"))
            rescan();

        ImGui::SliderInt("Tolerance", &mTolerance, 0, 64);
        ImGui::SliderInt("Max diff px", &mMaxDiffPixels, 0, 4096);

        if (!mLoadError.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.4f, 0.4f, 1.0f});
            ImGui::TextWrapped("%s", mLoadError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Text("Cases (%zu):", mEntries.size());
        ImGui::BeginChild("entries", {0, 0}, true);
        const int availableLanes = countAvailableLanes();
        for (int i = 0; i < static_cast<int>(mEntries.size()); ++i)
        {
            const Entry& entry = mEntries[i];
            int withActual = 0;
            for (int l = 0; l < kLaneCount; ++l)
                if (entry.hasActual[l]) ++withActual;

            // Marker shows per-case coverage across the available backends.
            std::string label = withActual == 0
                ? "[--] "
                : "[" + std::to_string(withActual) + "/" + std::to_string(availableLanes) + "] ";
            label += entry.name;

            if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), mSelected == i))
            {
                mSelected = i;
                rebuildSelection();
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void drawSelectedStats()
    {
        if (mSelected < 0 || mSelected >= static_cast<int>(mEntries.size()))
            return;

        const auto& size = mCanvas.screenSize();
        ImGui::SetNextWindowPos({size.width * 2.0f - 250.0f, 8}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({242, 300}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Selected");

        const Entry& entry = mEntries[mSelected];
        ImGui::Text("Case: %s", entry.name.c_str());
        ImGui::TextWrapped("Each backend's actual vs the golden (SwiftShader). "
                           "The swiftshader lane should read 0 — a sanity check.");
        ImGui::Separator();

        for (int i = 0; i < kLaneCount; ++i)
        {
            ImGui::TextUnformatted(mLanes[i].name.c_str());
            if (!mLanes[i].available)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[unavailable]");
                continue;
            }
            const auto result = laneResult(i);
            if (!result)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("[no actual]");
                continue;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(result->withinTolerance ? "MATCH" : "DIFFER");
            ImGui::Text("    diff px %zu, max %d, mean %.3f",
                        result->differingPixels,
                        static_cast<int>(result->maxChannelDelta),
                        result->meanChannelDelta);
        }

        ImGui::Separator();
        const bool canUpdate = mLanes[kSwiftShaderLane].available && entry.hasActual[kSwiftShaderLane];
        ImGui::BeginDisabled(!canUpdate);
        if (ImGui::Button("Update golden from swiftshader actual"))
        {
            updateGoldenFromSwiftshader(entry);
            rebuildSelection();
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Update ALL goldens from swiftshader"))
        {
            for (const Entry& e : mEntries)
                if (e.hasActual[kSwiftShaderLane])
                    updateGoldenFromSwiftshader(e);
            rebuildSelection();
        }
        ImGui::End();
    }

    int countAvailableLanes() const
    {
        int n = 0;
        for (const Lane& lane : mLanes)
            if (lane.available) ++n;
        return n;
    }

    void updateGoldenFromSwiftshader(const Entry& entry)
    {
        std::error_code ec;
        fs::copy_file(actualPath(mLanes[kSwiftShaderLane], entry.name), goldenPath(entry.name),
                      fs::copy_options::overwrite_existing, ec);
        if (ec)
            mLoadError = "update golden failed: " + ec.message();
    }

    // Runs every available backend's rendering_tests with DUMP_ACTUAL=1 so each
    // writes the actual PNGs into its lane dir, then rescans. A non-zero exit just
    // means a backend differs from the golden; the actuals are dumped regardless.
    void generateActualImages()
    {
        mLoadError.clear();
        std::error_code ec;
        for (const Lane& lane : mLanes)
        {
            if (!lane.available)
                continue;
            fs::create_directories(lane.actualDir, ec);
            setEnvVar("NOTHOFAGUS_RENDER_BACKEND", lane.backendEnv);
            setEnvVar("DUMP_ACTUAL", "1");
            setEnvVar("ACTUAL_DIR", lane.actualDir);
            setEnvVar("GOLDEN_DIR", mGoldenDir);
            const std::string command = "\"" + lane.bin + "\"";
            std::system(command.c_str());
        }
        rescan();
    }

    Nothofagus::Canvas mCanvas;
    std::string mGoldenDir;
    std::array<Lane, kLaneCount> mLanes;

    std::vector<Entry> mEntries;
    int mSelected = -1;

    int mTolerance     = 2;
    int mMaxDiffPixels = 0;

    std::optional<Nothofagus::DirectTexture> mGolden;
    std::array<std::optional<Nothofagus::DirectTexture>, kLaneCount> mActuals;
    std::array<std::optional<Nothofagus::DirectTexture>, kLaneCount> mDiffs;
    std::vector<Nothofagus::BellotaId> mBellotas;        // per-selection grid images
    std::vector<Nothofagus::BellotaId> mHeaderBellotas;  // static column-name labels
    std::string mLoadError;
};

} // namespace

int main()
{
    VisualTestsExplorer viewer;
    viewer.run();
    return 0;
}

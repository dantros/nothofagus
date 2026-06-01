// visual_tests_explorer — an interactive Nothofagus tool for inspecting golden-image
// regression results. It renders the selected test's golden / actual / diff
// images side by side and lets you update a golden from the current actual.
//
// Nothofagus deliberately does no file I/O; this tool (via the shared
// nothofagus_test_helpers lib, which wraps stb_image_plus) does all the loading,
// saving and comparison. Run rendering_tests with DUMP_ACTUAL=1 first to
// populate the "actual" directory.
//
//   visual_tests_explorer [goldenDir] [actualDir]
//
// Directories also come from the GOLDEN_DIR / ACTUAL_DIR env vars, or the
// in-app pickers; argv wins, then env, then the compiled-in defaults.

#include <canvas.h>
#include <texture.h>
#include <bellota.h>
#include <direct_texture_io.h>
#include <direct_texture_compare.h>

#include <imgui.h>
#include <imfilebrowser.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#ifndef VISUAL_TESTS_EXPLORER_DEFAULT_GOLDEN_DIR
    #define VISUAL_TESTS_EXPLORER_DEFAULT_GOLDEN_DIR "."
#endif
#ifndef VISUAL_TESTS_EXPLORER_DEFAULT_ACTUAL_DIR
    #define VISUAL_TESTS_EXPLORER_DEFAULT_ACTUAL_DIR "."
#endif
// Path to the rendering_tests binary that produces the actual images. Set by
// CMake when the visual tests are part of the build; empty otherwise (the
// "Generate actual images" button then asks for RENDERING_TESTS_BIN).
#ifndef VISUAL_TESTS_EXPLORER_TESTS_BIN
    #define VISUAL_TESTS_EXPLORER_TESTS_BIN ""
#endif

namespace fs = std::filesystem;

namespace
{

struct Entry
{
    std::string name;      // file stem, e.g. "single_bellota"
    bool        hasActual; // whether <actualDir>/<name>.png exists
};

std::string envOr(const char* key, const std::string& fallback)
{
    const char* value = std::getenv(key);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

// Set an environment variable in this process so a child launched via
// std::system inherits it. Portable across POSIX and Windows.
void setEnvVar(const char* key, const std::string& value)
{
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

// Integer scale that fits an image of the given size into a target box, never
// below 1 (small pixel-art goldens are scaled up, large ones down).
int fitScale(glm::ivec2 size, int box)
{
    if (size.x <= 0 || size.y <= 0)
        return 1;
    const int byWidth  = std::max(1, box / size.x);
    const int byHeight = std::max(1, box / size.y);
    return std::max(1, std::min(byWidth, byHeight));
}

class VisualTestsExplorer
{
public:
    VisualTestsExplorer(std::string goldenDir, std::string actualDir) :
        mCanvas({420, 280}, "Nothofagus Visual Tests Explorer", {0.12f, 0.12f, 0.14f}, 3),
        mGoldenDir(std::move(goldenDir)),
        mActualDir(std::move(actualDir))
    {
        mGoldenPicker.SetTitle("Select golden directory");
        mActualPicker.SetTitle("Select actual directory");
        rescan();
    }

    void run()
    {
        mCanvas.run([this](float) { frame(); });
    }

private:
    std::string goldenPath(const std::string& name) const { return mGoldenDir + "/" + name + ".png"; }
    std::string actualPath(const std::string& name) const { return mActualDir + "/" + name + ".png"; }

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
                mEntries.push_back({name, fs::exists(actualPath(name), ec)});
            }
            std::sort(mEntries.begin(), mEntries.end(),
                      [](const Entry& a, const Entry& b) { return a.name < b.name; });
        }
        // Keep selection valid.
        if (mSelected >= static_cast<int>(mEntries.size()))
            mSelected = mEntries.empty() ? -1 : 0;
        rebuildSelection();
    }

    // Loads the selected entry's images and (re)creates the display bellotas.
    void rebuildSelection()
    {
        // Drop previous display objects; their textures auto-GC next frame.
        for (std::optional<Nothofagus::BellotaId>* id : {&mGoldenBellota, &mActualBellota, &mDiffBellota})
        {
            if (id->has_value())
            {
                mCanvas.removeBellota(id->value());
                id->reset();
            }
        }
        mGolden.reset();
        mActual.reset();
        mDiff.reset();
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

        if (entry.hasActual)
        {
            try
            {
                mActual = Nothofagus::TestHelpers::load(actualPath(entry.name));
                mDiff   = Nothofagus::TestHelpers::makeDiff(mActual.value(), mGolden.value());
            }
            catch (const std::exception& e)
            {
                mLoadError = std::string("actual: ") + e.what();
            }
        }

        // Lay out golden | actual | diff across thirds of the canvas.
        const auto& size = mCanvas.screenSize();
        const float thirdX[3] = {size.width * 0.5f / 3.0f,
                                 size.width * 1.5f / 3.0f,
                                 size.width * 2.5f / 3.0f};
        const float centerY = size.height * 0.5f;
        constexpr int displayBox = 110;

        auto place = [&](std::optional<Nothofagus::DirectTexture>& tex, int column,
                         std::optional<Nothofagus::BellotaId>& outId)
        {
            if (!tex.has_value())
                return;
            const Nothofagus::TextureId texId = mCanvas.addTexture(tex.value());
            const float scale = static_cast<float>(fitScale(tex.value().size(), displayBox));
            outId = mCanvas.addBellota(Nothofagus::Bellota(
                Nothofagus::Transform({thirdX[column], centerY}, scale), texId));
        };

        place(mGolden, 0, mGoldenBellota);
        place(mActual, 1, mActualBellota);
        place(mDiff,   2, mDiffBellota);
    }

    void updateGoldenFromActual(const Entry& entry)
    {
        std::error_code ec;
        fs::copy_file(actualPath(entry.name), goldenPath(entry.name),
                      fs::copy_options::overwrite_existing, ec);
        if (ec)
            mLoadError = "update golden failed: " + ec.message();
    }

    void frame()
    {
        drawControls();

        mGoldenPicker.Display();
        if (mGoldenPicker.HasSelected())
        {
            mGoldenDir = mGoldenPicker.GetSelected().string();
            mGoldenPicker.ClearSelected();
            rescan();
        }
        mActualPicker.Display();
        if (mActualPicker.HasSelected())
        {
            mActualDir = mActualPicker.GetSelected().string();
            mActualPicker.ClearSelected();
            rescan();
        }
    }

    void drawControls()
    {
        ImGui::SetNextWindowPos({8, 8}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({360, 540}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Visual Tests Explorer");

        ImGui::TextWrapped("Layout: golden | actual | diff (left to right). "
                           "Diff is red where the images differ.");
        ImGui::Separator();

        ImGui::TextUnformatted("Golden dir:");
        ImGui::TextWrapped("%s", mGoldenDir.c_str());
        if (ImGui::Button("Pick golden dir..."))
            mGoldenPicker.Open();
        ImGui::SameLine();
        if (ImGui::Button("Rescan"))
            rescan();

        ImGui::TextUnformatted("Actual dir:");
        ImGui::TextWrapped("%s", mActualDir.c_str());
        if (ImGui::Button("Pick actual dir..."))
            mActualPicker.Open();
        ImGui::SameLine();
        if (ImGui::Button("Generate actual images"))
            generateActualImages();

        ImGui::Separator();
        ImGui::SliderInt("Tolerance", &mTolerance, 0, 64);
        ImGui::SliderInt("Max diff px", &mMaxDiffPixels, 0, 4096);

        if (!mLoadError.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.4f, 0.4f, 1.0f});
            ImGui::TextWrapped("%s", mLoadError.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        if (ImGui::Button("Update ALL failing goldens"))
            updateAllFailing();

        ImGui::Separator();
        ImGui::Text("Cases (%zu):", mEntries.size());
        ImGui::BeginChild("entries", {0, 0}, true);
        for (int i = 0; i < static_cast<int>(mEntries.size()); ++i)
        {
            const Entry& entry = mEntries[i];
            std::string label;
            if (!entry.hasActual)
                label = "[--] ";        // no actual to compare
            else
                label = entryWithinTolerance(i) ? "[ok] " : "[XX] ";
            label += entry.name;

            if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), mSelected == i))
            {
                mSelected = i;
                rebuildSelection();
            }
        }
        ImGui::EndChild();
        ImGui::End();

        drawSelectedStats();
    }

    // Comparison for the currently-cached selected images (cheap; recomputed
    // each frame so tolerance sliders update live).
    std::optional<Nothofagus::TestHelpers::ComparisonResult> currentResult() const
    {
        if (!mGolden.has_value() || !mActual.has_value())
            return std::nullopt;
        return Nothofagus::TestHelpers::compare(mActual.value(), mGolden.value(),
                                    static_cast<std::uint8_t>(mTolerance),
                                    static_cast<std::size_t>(mMaxDiffPixels));
    }

    // Re-loads an arbitrary entry just to evaluate pass/fail for the list marker.
    bool entryWithinTolerance(int index) const
    {
        const Entry& entry = mEntries[index];
        if (!entry.hasActual)
            return false;
        try
        {
            Nothofagus::DirectTexture golden = Nothofagus::TestHelpers::load(goldenPath(entry.name));
            Nothofagus::DirectTexture actual = Nothofagus::TestHelpers::load(actualPath(entry.name));
            return Nothofagus::TestHelpers::compare(actual, golden,
                                        static_cast<std::uint8_t>(mTolerance),
                                        static_cast<std::size_t>(mMaxDiffPixels)).withinTolerance;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    void drawSelectedStats()
    {
        if (mSelected < 0 || mSelected >= static_cast<int>(mEntries.size()))
            return;

        const auto& size = mCanvas.screenSize();
        ImGui::SetNextWindowPos({size.width * 3.0f - 240.0f, 8}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({232, 220}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Selected");

        const Entry& entry = mEntries[mSelected];
        ImGui::Text("Case: %s", entry.name.c_str());

        if (!entry.hasActual)
        {
            ImGui::TextWrapped("No actual image. Run rendering_tests with DUMP_ACTUAL=1.");
            ImGui::End();
            return;
        }

        if (const auto result = currentResult())
        {
            ImGui::Text("Verdict: %s", result->withinTolerance ? "PASS" : "FAIL");
            ImGui::Text("Size mismatch: %s", result->sizeMismatch ? "yes" : "no");
            ImGui::Text("Differing px: %zu", result->differingPixels);
            ImGui::Text("Max delta: %d", static_cast<int>(result->maxChannelDelta));
            ImGui::Text("Mean delta: %.4f", result->meanChannelDelta);
        }

        ImGui::Separator();
        if (ImGui::Button("Update golden from actual"))
        {
            updateGoldenFromActual(entry);
            rebuildSelection();
        }
        ImGui::End();
    }

    void updateAllFailing()
    {
        for (int i = 0; i < static_cast<int>(mEntries.size()); ++i)
        {
            if (mEntries[i].hasActual && !entryWithinTolerance(i))
                updateGoldenFromActual(mEntries[i]);
        }
        rebuildSelection();
    }

    // Runs the rendering_tests binary with DUMP_ACTUAL=1 so it renders every
    // case and writes the actual PNGs into the current actual dir, then rescans.
    // The engine does no file I/O itself, so producing actuals means driving the
    // test executable that owns the scene definitions.
    void generateActualImages()
    {
        const std::string bin = envOr("RENDERING_TESTS_BIN", VISUAL_TESTS_EXPLORER_TESTS_BIN);
        if (bin.empty())
        {
            mLoadError = "rendering_tests path unknown. Build with the visual tests "
                         "enabled, or set the RENDERING_TESTS_BIN env var.";
            return;
        }

        std::error_code ec;
        fs::create_directories(mActualDir, ec);

        // The child inherits these via the environment.
        setEnvVar("DUMP_ACTUAL", "1");
        setEnvVar("ACTUAL_DIR", mActualDir);
        setEnvVar("GOLDEN_DIR", mGoldenDir);

        const std::string command = "\"" + bin + "\"";
        const int rc = std::system(command.c_str());
        // A non-zero code just means some cases differ from their goldens; the
        // actual images are dumped regardless, which is all we need here.
        mLoadError = (rc == 0)
            ? std::string{}
            : "rendering_tests reported differences (actuals were still generated).";

        rescan();
    }

    Nothofagus::Canvas mCanvas;
    std::string mGoldenDir;
    std::string mActualDir;

    std::vector<Entry> mEntries;
    int mSelected = -1;

    int mTolerance     = 2;
    int mMaxDiffPixels = 0;

    std::optional<Nothofagus::DirectTexture> mGolden;
    std::optional<Nothofagus::DirectTexture> mActual;
    std::optional<Nothofagus::DirectTexture> mDiff;
    std::optional<Nothofagus::BellotaId> mGoldenBellota;
    std::optional<Nothofagus::BellotaId> mActualBellota;
    std::optional<Nothofagus::BellotaId> mDiffBellota;
    std::string mLoadError;

    ImGui::FileBrowser mGoldenPicker{ImGuiFileBrowserFlags_SelectDirectory};
    ImGui::FileBrowser mActualPicker{ImGuiFileBrowserFlags_SelectDirectory};
};

} // namespace

int main(int argc, char** argv)
{
    std::string goldenDir = envOr("GOLDEN_DIR", VISUAL_TESTS_EXPLORER_DEFAULT_GOLDEN_DIR);
    std::string actualDir = envOr("ACTUAL_DIR", VISUAL_TESTS_EXPLORER_DEFAULT_ACTUAL_DIR);
    if (argc > 1) goldenDir = argv[1];
    if (argc > 2) actualDir = argv[2];

    VisualTestsExplorer viewer(std::move(goldenDir), std::move(actualDir));
    viewer.run();
    return 0;
}

#pragma once

#include "render_backend.h"
#include "opengl_mesh.h"
#include "opengl_texture.h"
#include "opengl_render_target.h"
#include <unordered_map>
#include <span>
#include <string>

namespace Nothofagus
{

class OpenGLBackend
{
public:
    void initialize(void* nativeWindowHandle, glm::ivec2 canvasSize);
    void shutdown();
    void initImGuiRenderer();
    void rebuildImguiFontTexture();

    DTexture      uploadTexture(const Texture& texture,
                                TextureSampleMode minFilter,
                                TextureSampleMode magFilter);
    void          freeTexture(DTexture texture);
    DTexture      uploadPaletteTexture(const std::vector<glm::vec4>& paletteColors);
    void          updatePaletteTexture(DTexture paletteTexture,
                                       const std::vector<glm::vec4>& paletteColors);
    void          freePaletteTexture(DTexture paletteTexture);
    void          linkIndirectTextures(DTexture indexTexture, DTexture paletteTexture);
    DTexture      uploadTileMapTexture(std::span<const std::uint8_t> mapData, glm::ivec2 mapSize);
    void          freeTileMapTexture(DTexture mapTexture);
    void          linkTileMapTextures(DTexture atlasTexture, DTexture mapTexture, DTexture paletteTexture);
    DMesh         uploadMesh(const Mesh& mesh);
    void          freeMesh(DMesh mesh);
    DRenderTarget createRenderTarget(glm::ivec2 size);
    DTexture      getRenderTargetTexture(DRenderTarget renderTarget);
    void          freeRenderTarget(DRenderTarget renderTarget, DTexture proxyTexture);

    void beginFrame(glm::vec3 clearColor, ViewportRect gameViewport,
                    int framebufferWidth, int framebufferHeight);
    void imguiNewFrame();
    void beginRttPass(DRenderTarget renderTarget, glm::vec4 clearColor);
    void endRttPass();
    void beginMainPass(ViewportRect gameViewport);
    void drawSprite(DMesh mesh, DTexture texture, const SpriteDrawParams& params);
    void endFrame(ImDrawData* imguiData,
                  int framebufferWidth, int framebufferHeight);

    // ImGui-to-render-target: each secondary ImGuiContext gets its own
    // ImGui_ImplOpenGL3 backend instance. Called with that context active.
    void initImguiForRenderTarget(DRenderTarget renderTarget);
    void shutdownImguiForRenderTarget(DRenderTarget renderTarget);
    void imguiNewFrameForRenderTarget(DRenderTarget renderTarget);
    void renderImguiDrawDataToRenderTarget(ImDrawData* imguiData, DRenderTarget renderTarget);

    ScreenshotPixels takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const;

    /// Allow canvas_impl to update a texture's filter parameters directly after upload.
    void setTextureMinFilter(DTexture texture, TextureSampleMode mode);
    void setTextureMagFilter(DTexture texture, TextureSampleMode mode);

private:
    unsigned int mShaderProgram = 0;
    unsigned int mIndirectShaderProgram = 0;
    unsigned int mTilemapShaderProgram  = 0;
    unsigned int mActiveShaderProgram = 0;

    struct BellotaShaderUniforms
    {
        int transform     = -1;
        int layerIndex    = -1;
        int tintColor     = -1;
        int tintIntensity = -1;
        int opacity       = -1;
    } mUniforms;

    struct IndirectShaderUniforms
    {
        int transform      = -1;
        int layerIndex     = -1;
        int tintColor      = -1;
        int tintIntensity  = -1;
        int opacity        = -1;
        int indexSampler   = -1;
        int paletteSampler = -1;
    } mIndirectUniforms;

    struct TilemapShaderUniforms
    {
        int transform      = -1;
        int tintColor      = -1;
        int tintIntensity  = -1;
        int opacity        = -1;
        int atlasSampler   = -1;
        int mapSampler     = -1;
        int paletteSampler = -1;
    } mTilemapUniforms;

    std::unordered_map<std::size_t, OpenGLMesh>         mMeshes;
    std::unordered_map<std::size_t, OpenGLTexture>      mTextures;
    std::unordered_map<std::size_t, OpenGLRenderTarget> mRenderTargets;
    std::size_t mNextId = 0;

    unsigned int compileShader(unsigned int type, const std::string& source);
    unsigned int createShaderProgram(unsigned int vertexShader, unsigned int fragmentShader);
    void setupVAO(OpenGLMesh& glMesh);
};

static_assert(RenderBackend<OpenGLBackend>,
    "OpenGLBackend does not satisfy the RenderBackend concept");

} // namespace Nothofagus

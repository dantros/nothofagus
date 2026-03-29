
#include "canvas_impl.h"
#include "check.h"
#include "bellota_to_mesh.h"
#include "dmesh.h"
// #include "dmesh3D.h"
#include "performance_monitor.h"
#include "texture_container.h"
#include "bellota_container.h"
#include "render_target_container.h"
#include "keyboard.h"
#include "mouse.h"
#include "controller.h"
#include "roboto_font.h"
#include <glad/glad.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include "backends/window_backend.h"
#include <ciso646>
#include <cmath>
#include <optional>
#include <vector>
#include <format>
#include <algorithm>

namespace Nothofagus
{

// Window is the selected backend type. Forward declared in canvas_impl.h;
// defined here so the backend headers are only included from this translation unit.
struct Canvas::CanvasImpl::Window : public SelectedWindowBackend
{
    using SelectedWindowBackend::SelectedWindowBackend;
};

static ViewportRect computeLetterboxViewport(int framebufferWidth, int framebufferHeight, unsigned int canvasWidth, unsigned int canvasHeight)
{
    const float canvasAspectRatio      = static_cast<float>(canvasWidth)      / static_cast<float>(canvasHeight);
    const float framebufferAspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    int viewportWidth, viewportHeight, viewportX, viewportY;
    if (framebufferAspectRatio > canvasAspectRatio)
    {   // Pillarbox: framebuffer is wider than canvas — black bands left and right
        viewportHeight = framebufferHeight;
        viewportWidth  = static_cast<int>(framebufferHeight * canvasAspectRatio);
        viewportX      = (framebufferWidth - viewportWidth) / 2;
        viewportY      = 0;
    }
    else
    {   // Letterbox: framebuffer is taller than canvas — black bands top and bottom
        viewportWidth  = framebufferWidth;
        viewportHeight = static_cast<int>(framebufferWidth / canvasAspectRatio);
        viewportX      = 0;
        viewportY      = (framebufferHeight - viewportHeight) / 2;
    }
    return { viewportX, viewportY, viewportWidth, viewportHeight };
}

unsigned int compileShader(GLenum shaderType, const std::string& shaderSource)
{
    unsigned int shader = glCreateShader(shaderType);
    const GLchar* shaderSource_c_str = static_cast<const GLchar*>(shaderSource.c_str());
    glShaderSource(shader, 1, &shaderSource_c_str, nullptr);
    glCompileShader(shader);

    // Comprobar si la compilación fue exitosa
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        spdlog::error("Shader compilation failed: {}", infoLog);
        throw std::runtime_error("Shader compilation failed");
    }

    return shader;
}

unsigned int createShaderProgram(const unsigned int& vertexShader, const unsigned int& fragmentShader)
{
    // Crear programa de shader
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Comprobar si la vinculación fue exitosa
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        spdlog::error("Shader program linking failed: {}", infoLog);
        throw std::runtime_error("Shader program linking failed");
    }

    return shaderProgram;
}

Canvas::CanvasImpl::CanvasImpl(
    const ScreenSize& screenSize,
    const std::string& title,
    const glm::vec3 clearColor,
    const unsigned int pixelSize,
    const float imguiFontSize)
    :
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor),
    mPixelSize(pixelSize),
    mStats(false),
    mGameViewport{0, 0, 0, 0}
{
    // Initialize the window backend (creates window, GL context, loads GLAD)
    mWindow = std::make_unique<Window>(
        mTitle,
        static_cast<int>(mScreenSize.width  * mPixelSize),
        static_cast<int>(mScreenSize.height * mPixelSize)
    );

    const std::string vertexShaderSource = R"(
        #version 330 core
        in vec2 position;
        in vec2 texture;
        out vec2 outTextureCoordinates;
        uniform mat3 transform;
        void main()
        {
            outTextureCoordinates = texture;
            vec3 world2dPosition = transform * vec3(position.x, position.y, 1.0);
            gl_Position = vec4(world2dPosition.x, world2dPosition.y, 0.0, 1.0);
        }
    )";
    const std::string fragmentShaderSource = R"(
        #version 330 core
        in vec2 outTextureCoordinates;
        out vec4 outColor;
        uniform sampler2DArray textureSampler;
        uniform int layerIndex;
        uniform vec3 tintColor;
        uniform float tintIntensity;
        uniform float opacity;
        void main()
        {
            vec4 textureSample = texture(textureSampler, vec3(outTextureCoordinates, layerIndex));
            vec3 textureColor = textureSample.xyz;
            float textureOpacity = textureSample.w;
            vec3 blendColor = (tintColor * tintIntensity) + (textureColor * (1 - tintIntensity));
            outColor = vec4(blendColor, textureOpacity * opacity);
        }
    )";

    // build and compile our shader program

    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    mShaderProgram = createShaderProgram(vertexShader, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Enabling transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    // ImGui setup (backend-specific init + font loading)
    mWindow->initImGui(
        imguiFontSize,
        assets_Roboto_VariableFont_wdth_wght_ttf,
        assets_Roboto_VariableFont_wdth_wght_ttf_len
    );
}

Canvas::CanvasImpl::~CanvasImpl()
{
    // Window destructor handles backend cleanup (GL context, ImGui shutdown, etc.)
    // Needs to be defined in the cpp file to avoid incomplete type errors due to the pimpl idiom for struct Window
}

std::size_t Canvas::CanvasImpl::getCurrentMonitor() const
{
    return mWindow->getCurrentMonitor();
}

bool Canvas::CanvasImpl::isFullscreen() const
{
    return mWindow->isFullscreen();
}

void Canvas::CanvasImpl::setFullScreenOnMonitor(std::size_t monitorIndex)
{
    mLastWindowedAABox = mWindow->getWindowAABox();
    mWindow->setFullscreenOnMonitor(monitorIndex);
}

AABox Canvas::CanvasImpl::getWindowAABox() const
{
    return mWindow->getWindowAABox();
}

void Canvas::CanvasImpl::setWindowed()
{
    mWindow->setWindowed(mLastWindowedAABox);
}

const ScreenSize& Canvas::CanvasImpl::screenSize() const
{
    return mScreenSize;
}

void Canvas::CanvasImpl::setScreenSize(const ScreenSize& screenSize)
{
    mScreenSize = screenSize;
}

void Canvas::CanvasImpl::setClearColor(glm::vec3 clearColor)
{
    mClearColor = clearColor;
}

ScreenSize Canvas::CanvasImpl::windowSize() const
{
    debugCheck(mWindow != nullptr, "Canvas window has not been initialized");
    return mWindow->getWindowSize();
}

ViewportRect Canvas::CanvasImpl::gameViewport() const { return mGameViewport; }

BellotaId Canvas::CanvasImpl::addBellota(const Bellota& bellota)
{
    BellotaId newBellotaId{mBellotas.add({bellota, std::nullopt, std::nullopt})};
    Bellota& newBellota = this->bellota(newBellotaId);
    TextureId newTextureId = newBellota.texture();
    mTextureUsageMonitor.addEntry(newBellotaId, newTextureId);
    return newBellotaId;
}

void Canvas::CanvasImpl::removeBellota(const BellotaId bellotaId)
{
    BellotaPack& bellotaPackToRemove = mBellotas.at(bellotaId.id);
    Bellota& bellotaToRemove = bellotaPackToRemove.bellota;
    TextureId textureId = bellotaToRemove.texture();
    bellotaPackToRemove.clear();
    mBellotas.remove(bellotaId.id);
    mTextureUsageMonitor.removeEntry(bellotaId, textureId);
}

TextureId Canvas::CanvasImpl::addTexture(const Texture& texture)
{
    glm::ivec2 textureSize = std::visit(GetTextureSizeVisitor(), texture);
    TextureId newTextureId{mTextures.add({texture, std::nullopt, textureSize})};
    const bool textureWasAdded = mTextureUsageMonitor.addUnusedTexture(newTextureId);
    debugCheck(textureWasAdded, "Texture ID already present in usage monitor — duplicate addTexture call");
    return newTextureId;
}

void Canvas::CanvasImpl::removeTexture(const TextureId textureId)
{
    const bool textureWasRemoved = mTextureUsageMonitor.removeUnusedTexture(textureId);
    debugCheck(textureWasRemoved, "Texture is not in the unused set — still referenced by a bellota or already removed");

    TexturePack& texturePackToRemove = mTextures.at(textureId.id);
    texturePackToRemove.clear();

    mTextures.remove(textureId.id);
}

void Canvas::CanvasImpl::clearUnusedTextures()
{
    const std::unordered_set<TextureId> unusedTextureIdsCopy = mTextureUsageMonitor.getUnusedTextureIds();
    for (TextureId textureId : unusedTextureIdsCopy)
    {
        // Proxy entries are owned by their RenderTargetPack — skip auto-cleanup.
        if (mTextures.at(textureId.id).isProxy())
            continue;
        removeTexture(textureId);
    }
    mTextureUsageMonitor.clearUnusedTextureIds();
}

void Canvas::CanvasImpl::setTexture(const BellotaId bellotaId, const TextureId textureId)
{
    const Bellota& bellotaOriginal = bellota(bellotaId);
    Bellota bellotaWithNewTexture(
        bellotaOriginal.transform(),
        textureId,
        bellotaOriginal.depthOffset()
    );
    bellotaWithNewTexture.visible() = bellotaOriginal.visible();
    bellotaWithNewTexture.currentLayer() = bellotaOriginal.currentLayer();
    replaceBellota(bellotaId, bellotaWithNewTexture);
}

void Canvas::CanvasImpl::markTextureAsDirty(const TextureId textureId)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    debugCheck(not texturePack.isProxy(), "markTextureAsDirty called on a render target proxy texture.");

    // removing gpu content so it will be generated again with the new data.
    // TODO: enable a fast path so the same GPU memory gets overwritten instead of a new one.
    texturePack.clear();
}

void Canvas::CanvasImpl::setTextureMinFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.minFilter = mode;
    if (texturePack.dtextureOpt.has_value())
    {
        const GLint glFilter = (mode == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
        glBindTexture(GL_TEXTURE_2D_ARRAY, texturePack.dtextureOpt.value().texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, glFilter);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
}

void Canvas::CanvasImpl::setTextureMagFilter(const TextureId textureId, TextureSampleMode mode)
{
    TexturePack& texturePack = mTextures.at(textureId.id);
    texturePack.magFilter = mode;
    if (texturePack.dtextureOpt.has_value())
    {
        const GLint glFilter = (mode == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
        glBindTexture(GL_TEXTURE_2D_ARRAY, texturePack.dtextureOpt.value().texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, glFilter);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
}

RenderTargetId Canvas::CanvasImpl::addRenderTarget(ScreenSize size)
{
    const glm::ivec2 texSize{static_cast<int>(size.width), static_cast<int>(size.height)};

    // Register a proxy TexturePack so bellotas can reference the render target like any other texture.
    TexturePack proxyPack;
    proxyPack.texture = std::nullopt;
    proxyPack.dtextureOpt = std::nullopt;
    proxyPack.mTextureSize = texSize;
    TextureId proxyTexId{mTextures.add(proxyPack)};
    mTextureUsageMonitor.addUnusedTexture(proxyTexId);

    RenderTargetPack renderTargetPack;
    renderTargetPack.renderTarget = RenderTarget{texSize, proxyTexId};
    renderTargetPack.dRenderTargetOpt = std::nullopt;
    RenderTargetId newId{mRenderTargets.add(renderTargetPack)};

    return newId;
}

void Canvas::CanvasImpl::removeRenderTarget(RenderTargetId renderTargetId)
{
    RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
    const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;

    // Destroy the FBO (also frees the colorTexture GL handle).
    renderTargetPack.clear();

    // Detach the borrowed GL handle from the proxy TexturePack before removing it.
    mTextures.at(proxyTexId.id).dtextureOpt = std::nullopt;
    mTextures.remove(proxyTexId.id);

    // Remove from usage monitor if currently unused (i.e. no bellotas reference it).
    mTextureUsageMonitor.removeUnusedTexture(proxyTexId);

    mRenderTargets.remove(renderTargetId.id);
}

TextureId Canvas::CanvasImpl::renderTargetTexture(RenderTargetId renderTargetId) const
{
    return mRenderTargets.at(renderTargetId.id).renderTarget.mProxyTextureId;
}

void Canvas::CanvasImpl::renderTo(RenderTargetId renderTargetId, std::vector<BellotaId> bellotaIds)
{
    mPendingRttPasses.emplace_back(renderTargetId, std::move(bellotaIds));
}

void Canvas::CanvasImpl::setRenderTargetClearColor(RenderTargetId renderTargetId, glm::vec4 clearColor)
{
    mRenderTargets.at(renderTargetId.id).renderTarget.mClearColor = clearColor;
}

void Canvas::CanvasImpl::setTint(const BellotaId bellotaId, const Tint& tint)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");

    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);
    bellotaPack.tintOpt = tint;
}

void Canvas::CanvasImpl::removeTint(const BellotaId bellotaId)
{
    debugCheck(mBellotas.contains(bellotaId.id), "There is no Bellota associated with the BellotaId provided");

    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);
    bellotaPack.tintOpt = std::nullopt;
}

Bellota& Canvas::CanvasImpl::bellota(BellotaId bellotaId)
{
    return mBellotas.at(bellotaId.id).bellota;
}

const Bellota& Canvas::CanvasImpl::bellota(BellotaId bellotaId) const
{
    return mBellotas.at(bellotaId.id).bellota;
}

Texture& Canvas::CanvasImpl::texture(TextureId textureId)
{
    return mTextures.at(textureId.id).texture.value();
}

const Texture& Canvas::CanvasImpl::texture(TextureId textureId) const
{
    return mTextures.at(textureId.id).texture.value();
}

bool& Canvas::CanvasImpl::stats()
{
    return mStats;
}

const bool& Canvas::CanvasImpl::stats() const
{
    return mStats;
}

DirectTexture Canvas::CanvasImpl::takeScreenshot() const
{
    const int gameWidth  = static_cast<int>(mScreenSize.width);
    const int gameHeight = static_cast<int>(mScreenSize.height);

    // Create a temporary FBO at game resolution.
    // The game viewport in the framebuffer may be larger than the game canvas
    // (e.g. a 128x96 canvas at 6x pixel scale produces a 768x576 framebuffer
    // viewport), so we blit into a game-sized FBO to normalise the dimensions.
    unsigned int tempFbo, tempColorTex;
    glGenFramebuffers(1, &tempFbo);
    glGenTextures(1, &tempColorTex);

    glBindTexture(GL_TEXTURE_2D, tempColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gameWidth, gameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, tempFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempColorTex, 0);

    // Blit the game viewport from the front buffer into the temp FBO, scaling
    // it down to game resolution. Inverting the destination Y range converts
    // from OpenGL's bottom-to-top row order to top-to-bottom order so the
    // resulting pixel data is ready for conventional image use without an
    // additional flip pass.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_FRONT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tempFbo);
    glBlitFramebuffer(
        mGameViewport.x,
        mGameViewport.y,
        mGameViewport.x + mGameViewport.width,
        mGameViewport.y + mGameViewport.height,
        0, gameHeight, gameWidth, 0,    // inverted dest Y → flip to top-to-bottom
        GL_COLOR_BUFFER_BIT, GL_LINEAR
    );

    // Read the (already top-to-bottom) pixels from the temp FBO.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, tempFbo);
    TextureData textureData(gameWidth, gameHeight, 1);
    glReadPixels(0, 0, gameWidth, gameHeight, GL_RGBA, GL_UNSIGNED_BYTE, textureData.getDataSpan().data());

    // Restore default framebuffer state and release temp objects.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glDeleteFramebuffers(1, &tempFbo);
    glDeleteTextures(1, &tempColorTex);

    return DirectTexture(std::move(textureData));
}

void setupVAO(DMesh& dmesh, unsigned int shaderProgram)
{
    // Binding VAO to setup
    glBindVertexArray(dmesh.vao);

    // Binding buffers to the current VAO
    glBindBuffer(GL_ARRAY_BUFFER, dmesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dmesh.ebo);

    constexpr unsigned int positionAttribLength = 2;
    constexpr unsigned int textureAttribLength = 2;
    constexpr unsigned int stride = positionAttribLength + textureAttribLength;

    const auto positionAttribLocation = glGetAttribLocation(shaderProgram, "position");
    const auto textureAttribLocation = glGetAttribLocation(shaderProgram, "texture");

    glVertexAttribPointer(positionAttribLocation, positionAttribLength, GL_FLOAT, GL_FALSE, stride * sizeof(GLfloat), (void*)(0 * sizeof(GLfloat)));
    glEnableVertexAttribArray(positionAttribLocation);  
    
    glVertexAttribPointer(textureAttribLocation, textureAttribLength, GL_FLOAT, GL_FALSE, stride * sizeof(GLfloat), (void*)(positionAttribLength * sizeof(GLfloat)));
    glEnableVertexAttribArray(textureAttribLocation);

    // Unbinding current VAO
    glBindVertexArray(0);
}

GLuint textureArraySimpleSetup(const TextureData& textureData, TextureSampleMode minFilter, TextureSampleMode magFilter)
{
    // wrapMode: GL_REPEAT, GL_CLAMP_TO_EDGE
    // filterMode: GL_LINEAR, GL_NEAREST

    GLuint gpuTexture;
    glGenTextures(1, &gpuTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpuTexture);

    GLuint internalFormat = GL_RGBA;
    GLuint format = GL_RGBA;

    std::span<std::uint8_t> textureDataSpan = textureData.getDataSpan();
    std::uint8_t& firstTextureValue = textureDataSpan.front();
    std::uint8_t* pointerToFirstTextureValue = &firstTextureValue;

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, textureData.width(), textureData.height(), textureData.layers(), 0, format, GL_UNSIGNED_BYTE, pointerToFirstTextureValue);

    // texture wrapping params
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // texture filtering params
    const GLint glMinFilter = (minFilter == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
    const GLint glMagFilter = (magFilter == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, glMinFilter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, glMagFilter);

    return gpuTexture;
}

void initializeTexturePacks(TextureContainer& textures)
{
    for (auto& [textureIndex, texturePack] : textures)
    {
        if (not texturePack.isDirty())
            continue;

        // Proxy entries (render target color attachments) are initialized separately.
        if (texturePack.isProxy())
            continue;

        const Texture& texture = texturePack.texture.value();
        TextureData textureData = std::visit(GenerateTextureDataVisitor(), texture);

        texturePack.dtextureOpt = DTexture{ textureArraySimpleSetup(textureData, texturePack.minFilter, texturePack.magFilter) };
    }
}

void initializeRenderTargets(RenderTargetContainer& renderTargets, TextureContainer& textures)
{
    for (auto& [renderTargetIndex, renderTargetPack] : renderTargets)
    {
        if (not renderTargetPack.isDirty())
            continue;

        DRenderTarget dRenderTarget;
        dRenderTarget.create(renderTargetPack.renderTarget.mSize);
        const unsigned int colorTexture = dRenderTarget.colorTexture;
        renderTargetPack.dRenderTargetOpt = dRenderTarget;

        // Wire the FBO color attachment into the proxy TexturePack so DMesh can sample it.
        const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
        textures.at(proxyTexId.id).dtextureOpt = DTexture{colorTexture};
    }
}

void initializeBellotas(BellotaContainer& bellotas, TextureContainer& textures, unsigned int shaderProgram)
{
    for (auto& [bellotaIndex, bellotaPack] : bellotas)
    {
        if (not bellotaPack.isDirty())
            continue;

        const Bellota& bellota = bellotaPack.bellota;

        bellotaPack.meshOpt = generateMesh(textures, bellota);

        bellotaPack.dmeshOpt = DMesh();
        DMesh& dmesh = bellotaPack.dmeshOpt.value();
        dmesh.initBuffers();
        setupVAO(dmesh, shaderProgram);
        dmesh.fillBuffers(bellotaPack.meshOpt.value(), GL_STATIC_DRAW);

        TextureId textureId = bellota.texture();
        const TexturePack& texturePack = textures.at(textureId.id);

        debugCheck(texturePack.dtextureOpt.has_value(), "Texture has not been initializad on GPU.");

        const DTexture dtexture = texturePack.dtextureOpt.value();

        dmesh.texture = dtexture.texture;
    }
}

struct BellotaShaderUniforms
{
    GLint transform;
    GLint layerIndex;
    GLint tintColor;
    GLint tintIntensity;
    GLint opacity;
};

void drawBellotaPacks(
    const std::vector<const BellotaPack*>& sortedPacks,
    const glm::mat3& worldTransform,
    const BellotaShaderUniforms& uniforms)
{
    for (const BellotaPack* packPtr : sortedPacks)
    {
        debugCheck(packPtr != nullptr, "invalid pointer to bellota pack.");
        const Bellota& bellota = packPtr->bellota;

        if (not bellota.visible())
            continue;

        debugCheck(packPtr->dmeshOpt.has_value(), "DMesh has not been initialized.");
        const DMesh& dmesh = packPtr->dmeshOpt.value();
        const glm::mat3 totalTransformMat = worldTransform * bellota.transform().toMat3();

        if (packPtr->tintOpt != std::nullopt)
        {
            const Tint& tint = packPtr->tintOpt.value();
            glUniform3f(uniforms.tintColor, tint.color.r, tint.color.g, tint.color.b);
            glUniform1f(uniforms.tintIntensity, tint.intensity);
        }
        else
        {
            glUniform3f(uniforms.tintColor, 1.0f, 1.0f, 1.0f);
            glUniform1f(uniforms.tintIntensity, 0.0f);
        }
        glUniform1f(uniforms.opacity, bellota.opacity());
        glUniformMatrix3fv(uniforms.transform, 1, GL_FALSE, glm::value_ptr(totalTransformMat));
        glUniform1i(uniforms.layerIndex, bellota.currentLayer());

        dmesh.drawCall();
    }
}

void Canvas::CanvasImpl::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    mWindow->beginSession(controller);

    // state variable
    bool fillPolygons = true;

    auto computeWorldTransformMat = [](const ScreenSize& screenSize)
    {
        glm::mat3 worldTransformMat(1.0);
        worldTransformMat = glm::translate(worldTransformMat, glm::vec2(-1.0, -1.0));
        const glm::vec2 worldScale(
            2.0f / screenSize.width,
            2.0f / screenSize.height
        );
        return glm::scale(worldTransformMat, worldScale);
    };
    glm::mat3 worldTransformMat = computeWorldTransformMat(mScreenSize);

    // clang off
    const BellotaShaderUniforms bellotaShaderUniforms{
        /*GLint transform;      */ glGetUniformLocation(mShaderProgram, "transform"),
        /*GLint layerIndex;     */ glGetUniformLocation(mShaderProgram, "layerIndex"),
        /*GLint tintColor;      */ glGetUniformLocation(mShaderProgram, "tintColor"),
        /*GLint tintIntensity;  */ glGetUniformLocation(mShaderProgram, "tintIntensity"),
        /*GLint opacity;        */ glGetUniformLocation(mShaderProgram, "opacity"),
    };
    //clang on

    PerformanceMonitor performanceMonitor(mWindow->getTime(), 0.5f);

    // Dirty fix to sort bellotas by depth as required by transparent objects.
    std::vector<const BellotaPack*> sortedBellotaPacks;

    // Leaving some room in case more bellotas are created during runtime.
    sortedBellotaPacks.reserve(mBellotas.size()*2);

    auto sortByDepthOffset = [](const BellotaContainer& bellotas, std::vector<const BellotaPack*>& sortedBellotas)
    {
        // Per spec, clear does not change the underlaying memory allocation (capacity)
        sortedBellotas.clear();

        for (const auto& [bellotaIndex, bellotaPack] : bellotas)
        {
            sortedBellotas.push_back(&bellotaPack);
        }

        std::sort(sortedBellotas.begin(), sortedBellotas.end(),
            [](const BellotaPack* lhs, const BellotaPack* rhs)
            {
                debugCheck(lhs != nullptr and rhs != nullptr, "invalid pointers");
                const auto lhsDepthOffset = lhs->bellota.depthOffset();
                const auto rhsDepthOffset = rhs->bellota.depthOffset();
                return lhsDepthOffset < rhsDepthOffset;
            }
        );
    };

    ScreenSize currentScreenSize = mScreenSize;

    while (mWindow->isRunning())
    {
        controller.processInputs();

        performanceMonitor.update(mWindow->getTime());
        const float deltaTimeMS = performanceMonitor.getMS();

        // Get current framebuffer size and compute letterboxed viewport
        auto [framebufferWidth, framebufferHeight] = mWindow->getFramebufferSize();
        mGameViewport = computeLetterboxViewport(framebufferWidth, framebufferHeight, mScreenSize.width, mScreenSize.height);

        // Clear entire framebuffer to black (fills the letterbox/pillarbox bands)
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Clear game area only to the game background color
        glViewport(mGameViewport.x, mGameViewport.y, mGameViewport.width, mGameViewport.height);
        glScissor(mGameViewport.x, mGameViewport.y, mGameViewport.width, mGameViewport.height);
        glEnable(GL_SCISSOR_TEST);
        glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        mWindow->newImGuiFrame();
        ImGui::NewFrame();

        // executing user provided update
        update(deltaTimeMS);

        if (currentScreenSize != mScreenSize)
        {
            currentScreenSize = mScreenSize;
            worldTransformMat = computeWorldTransformMat(mScreenSize);
        }

        clearUnusedTextures();
        initializeTexturePacks(mTextures);
        initializeRenderTargets(mRenderTargets, mTextures);
        initializeBellotas(mBellotas, mTextures, mShaderProgram);
        sortByDepthOffset(mBellotas, sortedBellotaPacks);

        glUseProgram(mShaderProgram);

        // RTT pre-passes — render requested bellotas into their render targets
        // before drawing to the main framebuffer.
        if (not mPendingRttPasses.empty())
        {
            // Scissor test was enabled for the game-area clear above.
            // Each RTT FBO uses its own coordinate space starting at (0,0), so a
            // letterboxed/pillarboxed scissor rect (offset from the origin) would
            // clip all draws and clears inside the render target.  Disable it here;
            // it is restored with the correct game-viewport rect after the loop.
            glDisable(GL_SCISSOR_TEST);
            for (auto& [renderTargetId, bellotaIds] : mPendingRttPasses)
            {
                if (not mRenderTargets.contains(renderTargetId.id))
                    continue;

                RenderTargetPack& renderTargetPack = mRenderTargets.at(renderTargetId.id);
                if (not renderTargetPack.dRenderTargetOpt.has_value())
                    continue;

                DRenderTarget& dRenderTarget = renderTargetPack.dRenderTargetOpt.value();
                dRenderTarget.bind();
                glViewport(0, 0, dRenderTarget.size.x, dRenderTarget.size.y);
                const glm::vec4& clearColor = renderTargetPack.renderTarget.mClearColor;
                glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glm::mat3 renderTargetWorldTransform(1.0f);
                renderTargetWorldTransform = glm::translate(renderTargetWorldTransform, glm::vec2(-1.0f, -1.0f));
                renderTargetWorldTransform = glm::scale(renderTargetWorldTransform, glm::vec2(2.0f / dRenderTarget.size.x, 2.0f / dRenderTarget.size.y));

                std::vector<const BellotaPack*> renderTargetSortedPacks;
                for (const BellotaId bellotaId : bellotaIds)
                {
                    if (mBellotas.contains(bellotaId.id))
                        renderTargetSortedPacks.push_back(&mBellotas.at(bellotaId.id));
                }
                std::sort(renderTargetSortedPacks.begin(), renderTargetSortedPacks.end(),
                    [](const BellotaPack* lhs, const BellotaPack* rhs)
                    {
                        return lhs->bellota.depthOffset() < rhs->bellota.depthOffset();
                    }
                );

                drawBellotaPacks(renderTargetSortedPacks, renderTargetWorldTransform, bellotaShaderUniforms);

                dRenderTarget.unbind();
            }

            // Restore viewport and clear color to the main framebuffer values.
            {
                auto [frameBufferWidth, frameBufferHeight] = mWindow->getFramebufferSize();
                glViewport(0, 0, frameBufferWidth, frameBufferHeight);
                glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);
            }
            mPendingRttPasses.clear();
        }

        // Re-establish game viewport + scissor for the main draw pass.
        // RTT passes bind their own FBOs and change the viewport; restoring here
        // ensures correct letterboxed rendering whether or not RTT was used.
        glViewport(mGameViewport.x, mGameViewport.y, mGameViewport.width, mGameViewport.height);
        glScissor(mGameViewport.x, mGameViewport.y, mGameViewport.width, mGameViewport.height);
        glEnable(GL_SCISSOR_TEST);

        // drawing with OpenGL
        drawBellotaPacks(sortedBellotaPacks, worldTransformMat, bellotaShaderUniforms);

        // Restore full framebuffer viewport for ImGui (header, popups, stats)
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        if (mStats)
        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::Begin("stats", NULL, ImGuiWindowFlags_NoTitleBar);
            ImGui::Text("%.2f fps", performanceMonitor.getFPS());
            ImGui::Text("%.2f ms", performanceMonitor.getMS());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        mWindow->endFrame(controller, mGameViewport, mScreenSize);
    }

    for (auto& [bellotaIndex, bellotaPack] : mBellotas)
    {
        bellotaPack.clear();
    }

    // Render targets must be cleared before textures: DRenderTarget::clear() deletes
    // the colorTexture GL handle that the proxy TexturePack's dtextureOpt borrows.
    for (auto& [renderTargetIndex, renderTargetPack] : mRenderTargets)
    {
        if (renderTargetPack.dRenderTargetOpt.has_value())
        {
            const TextureId proxyTexId = renderTargetPack.renderTarget.mProxyTextureId;
            if (mTextures.contains(proxyTexId.id))
                mTextures.at(proxyTexId.id).dtextureOpt = std::nullopt;
        }
        renderTargetPack.clear();
    }

    for (auto& [textureIndex, texturePack] : mTextures)
    {
        texturePack.clear();
    }
}

void Canvas::CanvasImpl::close()
{
    mWindow->requestClose();
}

void Canvas::CanvasImpl::replaceBellota(const BellotaId bellotaId, const Bellota& newBellota)
{
    BellotaPack& bellotaPack = mBellotas.at(bellotaId.id);

    TextureId textureIdToReplace = bellotaPack.bellota.texture();
    const bool oldEntryRemoved = mTextureUsageMonitor.removeEntry(bellotaId, textureIdToReplace);
    debugCheck(oldEntryRemoved, "Failed to remove old texture entry from usage monitor during bellota replacement");

    TextureId newTextureId = newBellota.texture();
    const bool newEntryAdded = mTextureUsageMonitor.addEntry(bellotaId, newTextureId);
    debugCheck(newEntryAdded, "Failed to add new texture entry to usage monitor during bellota replacement");

    bellotaPack.clearMesh();
    bellotaPack.bellota = newBellota;
}

ScreenSize getPrimaryMonitorSize()
{
    return SelectedWindowBackend::getPrimaryMonitorSize();
}

}

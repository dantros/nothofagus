#include "opengl_backend.h"
#include "check.h"
#include <glad/glad.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <span>

namespace Nothofagus
{

unsigned int OpenGLBackend::compileShader(unsigned int type, const std::string& source)
{
    unsigned int shader = glCreateShader(type);
    const GLchar* sourceCStr = static_cast<const GLchar*>(source.c_str());
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    glCompileShader(shader);

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

unsigned int OpenGLBackend::createShaderProgram(unsigned int vertexShader, unsigned int fragmentShader)
{
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        spdlog::error("Shader program linking failed: {}", infoLog);
        throw std::runtime_error("Shader program linking failed");
    }
    return program;
}

void OpenGLBackend::setupVAO(OpenGLMesh& glMesh)
{
    glBindVertexArray(glMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);

    constexpr unsigned int positionAttribLength = 2;
    constexpr unsigned int textureAttribLength  = 2;
    constexpr unsigned int stride = positionAttribLength + textureAttribLength;

    const auto positionAttribLocation = glGetAttribLocation(mShaderProgram, "position");
    const auto textureAttribLocation  = glGetAttribLocation(mShaderProgram, "texture");

    glVertexAttribPointer(positionAttribLocation, positionAttribLength, GL_FLOAT, GL_FALSE,
                          stride * sizeof(GLfloat), (void*)(0 * sizeof(GLfloat)));
    glEnableVertexAttribArray(positionAttribLocation);

    glVertexAttribPointer(textureAttribLocation, textureAttribLength, GL_FLOAT, GL_FALSE,
                          stride * sizeof(GLfloat), (void*)(positionAttribLength * sizeof(GLfloat)));
    glEnableVertexAttribArray(textureAttribLocation);

    glBindVertexArray(0);
}

void OpenGLBackend::initialize(void* /*nativeWindowHandle*/, glm::ivec2 /*canvasSize*/)
{
    // GLAD was already loaded by the window backend constructor.

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

    unsigned int vertexShader   = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    mShaderProgram = createShaderProgram(vertexShader, fragmentShader);
    glDeleteShader(fragmentShader);

    mUniforms.transform     = glGetUniformLocation(mShaderProgram, "transform");
    mUniforms.layerIndex    = glGetUniformLocation(mShaderProgram, "layerIndex");
    mUniforms.tintColor     = glGetUniformLocation(mShaderProgram, "tintColor");
    mUniforms.tintIntensity = glGetUniformLocation(mShaderProgram, "tintIntensity");
    mUniforms.opacity       = glGetUniformLocation(mShaderProgram, "opacity");

    // --- Indirect (palette-based) shader ---
    const std::string indirectFragmentShaderSource = R"(
        #version 330 core
        in vec2 outTextureCoordinates;
        out vec4 outColor;
        uniform usampler2DArray indexSampler;
        uniform sampler1D paletteSampler;
        uniform int layerIndex;
        uniform vec3 tintColor;
        uniform float tintIntensity;
        uniform float opacity;
        void main()
        {
            ivec3 texSize = textureSize(indexSampler, 0);
            uint colorIndex = texelFetch(indexSampler,
                ivec3(int(outTextureCoordinates.x * float(texSize.x)),
                      int(outTextureCoordinates.y * float(texSize.y)),
                      layerIndex), 0).r;
            vec4 paletteColor = texelFetch(paletteSampler, int(colorIndex), 0);
            vec3 blendColor = tintColor * tintIntensity + paletteColor.rgb * (1.0 - tintIntensity);
            outColor = vec4(blendColor, paletteColor.a * opacity);
        }
    )";

    unsigned int indirectFragmentShader = compileShader(GL_FRAGMENT_SHADER, indirectFragmentShaderSource);
    mIndirectShaderProgram = createShaderProgram(vertexShader, indirectFragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(indirectFragmentShader);

    mIndirectUniforms.transform      = glGetUniformLocation(mIndirectShaderProgram, "transform");
    mIndirectUniforms.layerIndex     = glGetUniformLocation(mIndirectShaderProgram, "layerIndex");
    mIndirectUniforms.tintColor      = glGetUniformLocation(mIndirectShaderProgram, "tintColor");
    mIndirectUniforms.tintIntensity  = glGetUniformLocation(mIndirectShaderProgram, "tintIntensity");
    mIndirectUniforms.opacity        = glGetUniformLocation(mIndirectShaderProgram, "opacity");
    mIndirectUniforms.indexSampler   = glGetUniformLocation(mIndirectShaderProgram, "indexSampler");
    mIndirectUniforms.paletteSampler = glGetUniformLocation(mIndirectShaderProgram, "paletteSampler");

    // Set sampler uniform values for the indirect program (texture units 0 and 1).
    glUseProgram(mIndirectShaderProgram);
    glUniform1i(mIndirectUniforms.indexSampler, 0);
    glUniform1i(mIndirectUniforms.paletteSampler, 1);
    glUseProgram(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
}

void OpenGLBackend::initImGuiRenderer()
{
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OpenGLBackend::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    glDeleteProgram(mShaderProgram);
    mShaderProgram = 0;
    glDeleteProgram(mIndirectShaderProgram);
    mIndirectShaderProgram = 0;
    mActiveShaderProgram = 0;
}

DTexture OpenGLBackend::uploadTexture(const Texture& texture,
                                       TextureSampleMode minFilter,
                                       TextureSampleMode magFilter)
{
    GLuint gpuTexture;
    glGenTextures(1, &gpuTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpuTexture);

    if (std::holds_alternative<IndirectTexture>(texture))
    {
        // Upload raw color indices as an R8UI integer texture.
        const auto& indirectTexture = std::get<IndirectTexture>(texture);
        const std::vector<std::uint8_t> indexData = indirectTexture.generateIndexData();
        const glm::ivec2 textureSize = indirectTexture.size();
        const GLsizei layerCount = static_cast<GLsizei>(indirectTexture.layers());

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8UI,
                     textureSize.x, textureSize.y, layerCount,
                     0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, indexData.data());

        // Integer textures require GL_NEAREST — linear filtering is not supported.
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        // Direct texture: upload pre-resolved RGBA data.
        TextureData textureData = std::visit(GenerateTextureDataVisitor(), texture);
        std::span<std::uint8_t> dataSpan = textureData.getDataSpan();
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA,
                     textureData.width(), textureData.height(), textureData.layers(),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, dataSpan.data());

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

        const GLint glMinFilter = (minFilter == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
        const GLint glMagFilter = (magFilter == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, glMinFilter);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, glMagFilter);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    std::size_t newId = mNextId++;
    mTextures[newId] = OpenGLTexture{gpuTexture};
    return DTexture{newId};
}

void OpenGLBackend::freeTexture(DTexture texture)
{
    auto it = mTextures.find(texture.id);
    if (it != mTextures.end())
    {
        it->second.clear();
        mTextures.erase(it);
    }
}

DTexture OpenGLBackend::uploadPaletteTexture(const std::vector<glm::vec4>& paletteColors)
{
    GLuint gpuTexture;
    glGenTextures(1, &gpuTexture);
    glBindTexture(GL_TEXTURE_1D, gpuTexture);

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F,
                 static_cast<GLsizei>(paletteColors.size()),
                 0, GL_RGBA, GL_FLOAT, paletteColors.data());

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_1D, 0);

    std::size_t newId = mNextId++;
    mTextures[newId] = OpenGLTexture{gpuTexture};
    return DTexture{newId};
}

void OpenGLBackend::updatePaletteTexture(DTexture paletteTexture,
                                          const std::vector<glm::vec4>& paletteColors)
{
    auto it = mTextures.find(paletteTexture.id);
    if (it == mTextures.end()) return;

    glBindTexture(GL_TEXTURE_1D, it->second.texture);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0,
                    static_cast<GLsizei>(paletteColors.size()),
                    GL_RGBA, GL_FLOAT, paletteColors.data());
    glBindTexture(GL_TEXTURE_1D, 0);
}

void OpenGLBackend::freePaletteTexture(DTexture paletteTexture)
{
    freeTexture(paletteTexture);
}

void OpenGLBackend::linkIndirectTextures(DTexture /*indexTexture*/, DTexture /*paletteTexture*/)
{
    // No-op for OpenGL — texture binding is handled per-draw in drawSprite().
}

DMesh OpenGLBackend::uploadMesh(const Mesh& mesh)
{
    OpenGLMesh glMesh{};
    glMesh.initBuffers();
    setupVAO(glMesh);
    glMesh.fillBuffers(mesh, GL_STATIC_DRAW);

    std::size_t newId = mNextId++;
    mMeshes[newId] = glMesh;
    return DMesh{newId};
}

void OpenGLBackend::freeMesh(DMesh mesh)
{
    auto it = mMeshes.find(mesh.id);
    if (it != mMeshes.end())
    {
        it->second.clear();
        mMeshes.erase(it);
    }
}

DRenderTarget OpenGLBackend::createRenderTarget(glm::ivec2 size)
{
    OpenGLRenderTarget glRenderTarget{};
    glRenderTarget.create(size);

    std::size_t newId = mNextId++;
    mRenderTargets[newId] = glRenderTarget;
    return DRenderTarget{newId, size};
}

DTexture OpenGLBackend::getRenderTargetTexture(DRenderTarget renderTarget)
{
    auto it = mRenderTargets.find(renderTarget.id);
    debugCheck(it != mRenderTargets.end(), "getRenderTargetTexture: render target not found");

    // Register the color texture in the texture map so drawSprite can find it.
    // The proxy DTexture shares the same GL handle as the render target's color attachment.
    std::size_t newId = mNextId++;
    mTextures[newId] = OpenGLTexture{it->second.colorTexture};
    return DTexture{newId};
}

void OpenGLBackend::freeRenderTarget(DRenderTarget renderTarget, DTexture proxyTexture)
{
    // Remove the proxy texture entry without calling glDeleteTextures — the GL handle
    // is owned by the render target's color attachment and freed below.
    mTextures.erase(proxyTexture.id);

    auto it = mRenderTargets.find(renderTarget.id);
    if (it != mRenderTargets.end())
    {
        it->second.clear();
        mRenderTargets.erase(it);
    }
}

void OpenGLBackend::beginFrame(glm::vec3 clearColor, ViewportRect gameViewport,
                                int framebufferWidth, int framebufferHeight)
{
    // Clear entire framebuffer to black (fills letterbox/pillarbox bands).
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Clear game area only to the canvas background color.
    glViewport(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    glScissor(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(mShaderProgram);
    mActiveShaderProgram = mShaderProgram;
}

void OpenGLBackend::imguiNewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLBackend::beginRttPass(DRenderTarget renderTarget, glm::vec4 clearColor)
{
    auto it = mRenderTargets.find(renderTarget.id);
    debugCheck(it != mRenderTargets.end(), "beginRttPass: render target not found");
    OpenGLRenderTarget& glRenderTarget = it->second;

    // Scissor test was enabled for the game-area clear in beginFrame.
    // FBO coordinate space starts at (0,0), so the letterboxed scissor rect
    // would clip all draws inside the render target — disable it here.
    glDisable(GL_SCISSOR_TEST);
    glRenderTarget.bind();
    glViewport(0, 0, glRenderTarget.size.x, glRenderTarget.size.y);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLBackend::endRttPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLBackend::beginMainPass(ViewportRect gameViewport)
{
    // Re-establish the game viewport + scissor for the main draw pass.
    glViewport(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    glScissor(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    glEnable(GL_SCISSOR_TEST);
}

void OpenGLBackend::drawSprite(DMesh mesh, DTexture texture, const SpriteDrawParams& params)
{
    auto meshIt = mMeshes.find(mesh.id);
    debugCheck(meshIt != mMeshes.end(), "drawSprite: mesh not found");
    auto texIt = mTextures.find(texture.id);
    debugCheck(texIt != mTextures.end(), "drawSprite: texture not found");

    OpenGLMesh& glMesh = meshIt->second;
    const GLuint glTexture = texIt->second.texture;

    if (params.isIndirect)
    {
        if (mActiveShaderProgram != mIndirectShaderProgram)
        {
            glUseProgram(mIndirectShaderProgram);
            mActiveShaderProgram = mIndirectShaderProgram;
        }

        // Bind index texture to unit 0.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, glTexture);

        // Bind palette texture to unit 1.
        auto paletteIt = mTextures.find(params.paletteTexture.id);
        debugCheck(paletteIt != mTextures.end(), "drawSprite: palette texture not found");
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, paletteIt->second.texture);

        glUniform3f(mIndirectUniforms.tintColor, params.tintColor.r, params.tintColor.g, params.tintColor.b);
        glUniform1f(mIndirectUniforms.tintIntensity, params.tintIntensity);
        glUniform1f(mIndirectUniforms.opacity, params.opacity);
        glUniformMatrix3fv(mIndirectUniforms.transform, 1, GL_FALSE, glm::value_ptr(params.transform));
        glUniform1i(mIndirectUniforms.layerIndex, params.layerIndex);
    }
    else
    {
        if (mActiveShaderProgram != mShaderProgram)
        {
            glUseProgram(mShaderProgram);
            mActiveShaderProgram = mShaderProgram;
        }

        // Bind RGBA texture to unit 0.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, glTexture);

        glUniform3f(mUniforms.tintColor, params.tintColor.r, params.tintColor.g, params.tintColor.b);
        glUniform1f(mUniforms.tintIntensity, params.tintIntensity);
        glUniform1f(mUniforms.opacity, params.opacity);
        glUniformMatrix3fv(mUniforms.transform, 1, GL_FALSE, glm::value_ptr(params.transform));
        glUniform1i(mUniforms.layerIndex, params.layerIndex);
    }

    glMesh.drawCall();

    // Unbind textures.
    if (params.isIndirect)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void OpenGLBackend::endFrame(ImDrawData* imguiData,
                              int framebufferWidth, int framebufferHeight)
{
    // Restore full framebuffer viewport for ImGui (header, popups, stats).
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    ImGui_ImplOpenGL3_RenderDrawData(imguiData);
    // Buffer swap is performed by the window backend's endFrame().
}

ScreenshotPixels OpenGLBackend::takeScreenshot(ViewportRect gameViewport, glm::ivec2 gameSize) const
{
    const int gameWidth  = gameSize.x;
    const int gameHeight = gameSize.y;

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

    // Blit game viewport from the front buffer into the temp FBO at game resolution.
    // Inverting the destination Y range converts from OpenGL's bottom-to-top row order
    // to top-to-bottom order.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_FRONT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tempFbo);
    glBlitFramebuffer(
        gameViewport.x, gameViewport.y,
        gameViewport.x + gameViewport.width, gameViewport.y + gameViewport.height,
        0, gameHeight, gameWidth, 0,    // inverted dest Y → flip to top-to-bottom
        GL_COLOR_BUFFER_BIT, GL_LINEAR
    );

    glBindFramebuffer(GL_READ_FRAMEBUFFER, tempFbo);
    ScreenshotPixels result;
    result.width  = gameWidth;
    result.height = gameHeight;
    result.data.resize(static_cast<std::size_t>(gameWidth) * static_cast<std::size_t>(gameHeight) * 4);
    glReadPixels(0, 0, gameWidth, gameHeight, GL_RGBA, GL_UNSIGNED_BYTE, result.data.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glDeleteFramebuffers(1, &tempFbo);
    glDeleteTextures(1, &tempColorTex);

    return result;
}

void OpenGLBackend::setTextureMinFilter(DTexture texture, TextureSampleMode mode)
{
    auto it = mTextures.find(texture.id);
    if (it == mTextures.end()) return;
    const GLint glFilter = (mode == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
    glBindTexture(GL_TEXTURE_2D_ARRAY, it->second.texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, glFilter);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void OpenGLBackend::setTextureMagFilter(DTexture texture, TextureSampleMode mode)
{
    auto it = mTextures.find(texture.id);
    if (it == mTextures.end()) return;
    const GLint glFilter = (mode == TextureSampleMode::Linear) ? GL_LINEAR : GL_NEAREST;
    glBindTexture(GL_TEXTURE_2D_ARRAY, it->second.texture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, glFilter);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

} // namespace Nothofagus

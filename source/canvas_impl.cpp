
#include "canvas_impl.h"
#include "check.h"
#include "bellota_to_mesh.h"
#include "dmesh.h"
// #include "dmesh3D.h"
#include "performance_monitor.h"
#include "texture_container.h"
#include "bellota_container.h"
#include "keyboard.h"
#include "controller.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ciso646>
#include <optional>
#include <vector>
#include <format>
#include <algorithm>

namespace Nothofagus
{

// Wrapper class forward declared in the .h to avoid including GLFW dependecies in the header file.
struct Canvas::CanvasImpl::Window
{
    GLFWwindow* glfwWindow;
};

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
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

Canvas::CanvasImpl::CanvasImpl(const ScreenSize& screenSize, const std::string& title, const glm::vec3 clearColor, const unsigned int pixelSize) :
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor),
    mPixelSize(pixelSize),
    mStats(false)
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    GLFWwindow* glfwWindow = glfwCreateWindow(mScreenSize.width * mPixelSize, mScreenSize.height * mPixelSize, mTitle.c_str(), NULL, NULL);
    if (glfwWindow == nullptr)
    {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        throw;
    }

    mWindow = std::make_unique<Window>(glfwWindow);

    glfwMakeContextCurrent(mWindow->glfwWindow);
    glfwSetFramebufferSizeCallback(mWindow->glfwWindow, framebufferSizeCallback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize GLAD");
        throw;
    }

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
        uniform sampler2D textureSampler;
        uniform int layerIndex;
        uniform vec3 tintColor;
        uniform float tintIntensity;
        void main()
        {
            vec4 textureSample = texture(textureSampler, outTextureCoordinates + layerIndex);
            vec3 textureColor = textureSample.xyz;
            float textureOpacity = textureSample.w;
            vec3 blendColor = (tintColor * tintIntensity) + (textureColor * (1 - tintIntensity));
            outColor = vec4(blendColor, textureOpacity);
        }
    )";
    
    const std::string textureArrayFragmentShaderSource = R"(
        #version 330 core
        in vec2 outTextureCoordinates;
        out vec4 outColor;
        uniform sampler2DArray textureArray;
        uniform int layerIndex;
        uniform vec3 tintColor;
        uniform float tintIntensity;
        void main()
        {
            vec4 textureArraySample = texture(textureArray, vec3(outTextureCoordinates, layerIndex));
            vec3 textureColor = textureArraySample.xyz;
            float textureOpacity = textureArraySample.w;
            vec3 blendColor = (tintColor * tintIntensity) + (textureColor * (1 - tintIntensity));
            outColor = vec4(blendColor, textureOpacity);
        }
    )";

    // build and compile our shader program

    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int textureArrayFragmentShader = compileShader(GL_FRAGMENT_SHADER, textureArrayFragmentShaderSource);

    mShaderProgram = createShaderProgram(vertexShader, fragmentShader);
    mAnimatedShaderProgram = createShaderProgram(vertexShader, textureArrayFragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(textureArrayFragmentShader);

    // Enabling transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    // ImGui setup
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(mWindow->glfwWindow, true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);
}

Canvas::CanvasImpl::~CanvasImpl()
{
    // freeing GPU memorygpuShape.clear();
    glfwTerminate();

    // Needs to be defined in the cpp file to avoid incomplete type errors due to the pimpl idiom for struct Window
}

const ScreenSize& Canvas::CanvasImpl::screenSize() const
{
    return mScreenSize;
}

BellotaId Canvas::CanvasImpl::addBellota(const Bellota& bellota)
{
    return {mBellotas.add({bellota, std::nullopt, std::nullopt})};
}

void Canvas::CanvasImpl::removeBellota(const BellotaId bellotaId)
{
    mBellotas.remove(bellotaId.id);
}

BellotaId Canvas::CanvasImpl::addAnimatedBellota(const Bellota& animatedBellota)
{
    return {mAnimatedBellotas.add({animatedBellota, std::nullopt, std::nullopt})};
}

void Canvas::CanvasImpl::removeAnimatedBellota(const BellotaId animatedBellotaId)
{
    mAnimatedBellotas.remove(animatedBellotaId.id);
}

TextureId Canvas::CanvasImpl::addTexture(const Texture& texture)
{
    return { mTextures.add({texture, std::nullopt}) };
}

void Canvas::CanvasImpl::removeTexture(const TextureId textureId)
{
    mTextures.remove(textureId.id);
}

TextureId Canvas::CanvasImpl::addTextureArray(const TextureArray& textureArray)
{
    return { mTexturesArrays.add({textureArray, std::nullopt}) };
}

void Canvas::CanvasImpl::removeTextureArray(const TextureId textureId)
{
    mTexturesArrays.remove(textureId.id);
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

Bellota& Canvas::CanvasImpl::animatedBellota(BellotaId animatedBellotaId)
{
    return mAnimatedBellotas.at(animatedBellotaId.id).bellota;
}

const Bellota& Canvas::CanvasImpl::animatedBellota(BellotaId animatedBellotaId) const
{
    return mAnimatedBellotas.at(animatedBellotaId.id).bellota;
}

Texture& Canvas::CanvasImpl::texture(TextureId textureId)
{
    return mTextures.at(textureId.id).texture;
}

const Texture& Canvas::CanvasImpl::texture(TextureId textureId) const
{
    return mTextures.at(textureId.id).texture;
}

TextureArray& Canvas::CanvasImpl::textureArray(TextureId textureId)
{
    return mTexturesArrays.at(textureId.id).textureArray;
}

const TextureArray& Canvas::CanvasImpl::textureArray(TextureId textureId) const
{
    return mTexturesArrays.at(textureId.id).textureArray;
}

bool& Canvas::CanvasImpl::stats()
{
    return mStats;
}

const bool& Canvas::CanvasImpl::stats() const
{
    return mStats;
}

//void setupVAO(DMesh& dMesh, GPUID shaderProgram)
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

GLuint textureSimpleSetup(const TextureData& textureData)
{
    // wrapMode: GL_REPEAT, GL_CLAMP_TO_EDGE
    // filterMode: GL_LINEAR, GL_NEAREST

    GLuint gpuTexture;
    glGenTextures(1, &gpuTexture);
    glBindTexture(GL_TEXTURE_2D, gpuTexture);

    // texture wrapping params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // texture filtering params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint internalFormat = GL_RGBA;
    GLuint format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureData.width, textureData.height, 0, format, GL_UNSIGNED_BYTE, textureData.getData());

    return gpuTexture;
}

GLuint textureArraySimpleSetup(const TextureData& textureData)
{
    // wrapMode: GL_REPEAT, GL_CLAMP_TO_EDGE
    // filterMode: GL_LINEAR, GL_NEAREST

    GLuint gpuTexture;
    glGenTextures(1, &gpuTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpuTexture);

    GLuint internalFormat = GL_RGBA;
    GLuint format = GL_RGBA;

    std::cout << "layers: " << textureData.layers << std::endl;
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, textureData.width, textureData.height, textureData.layers, 0, format, GL_UNSIGNED_BYTE, textureData.getData());

    // texture wrapping params
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // texture filtering params
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);



    return gpuTexture;
}

void keyCallback(GLFWwindow* window, int glfwKey, int scancode, int action, int mods)
{
    if (not (action == GLFW_PRESS or action == GLFW_RELEASE))
        return;

    auto myUserPointer = glfwGetWindowUserPointer(window);
    Controller* controller = static_cast<Controller*>(myUserPointer);

    Key key = KeyboardImplementation::toKeyCode(glfwKey);

    DiscreteTrigger trigger = action == GLFW_PRESS ? DiscreteTrigger::Press : DiscreteTrigger::Release;
    controller->activate({ key, trigger });
}

void Canvas::CanvasImpl::run(std::function<void(float deltaTime)> update, Controller& controller)
{
    debugCheck(mWindow->glfwWindow != nullptr, "GLFW Window has not been initialized.");

    glfwSetWindowUserPointer(mWindow->glfwWindow, &controller);
    glfwSetKeyCallback(mWindow->glfwWindow, keyCallback);
    
    // TODO: create IndexedContainerIterator
    for (auto& pair : mTextures.map())
    {
        const TextureId textureId{ pair.first };
        TexturePack& texturePack = pair.second;

        const Texture& texture = texturePack.texture;
        TextureData textureData = texture.generateTextureData();

        texturePack.dtextureOpt = DTexture{ textureSimpleSetup(textureData) };
    }

    for (auto& pair : mTexturesArrays.map())
    {
        const TextureId textureId{ pair.first };
        TextureArrayPack& texturePack = pair.second;

        const TextureArray& texture = texturePack.textureArray;
        TextureData textureData = texture.generateTextureData();

        texturePack.dtextureOpt = DTextureArray{ textureArraySimpleSetup(textureData) };
    }
    
    for (auto& pair : mBellotas.map())
    {
        const BellotaId bellotaId{ pair.first };
        BellotaPack& bellotaPack = pair.second;
        const Bellota& bellota = bellotaPack.bellota;

        bellotaPack.meshOpt = generateMesh(mTextures, bellota);

        bellotaPack.dmeshOpt = DMesh();
        DMesh& dmesh = bellotaPack.dmeshOpt.value();
        dmesh.initBuffers();
        setupVAO(dmesh, mShaderProgram);
        dmesh.fillBuffers(bellotaPack.meshOpt.value(), GL_STATIC_DRAW);

        TextureId textureId = bellota.texture();
        const TexturePack& texturePack = mTextures.at(textureId.id);

        debugCheck(texturePack.dtextureOpt.has_value(), "Texture has not been initializad on GPU.");

        const DTexture dtexture = texturePack.dtextureOpt.value();

        dmesh.texture = dtexture.texture;
    }
    
    for (auto& pair : mAnimatedBellotas.map())
    {
        const BellotaId animatedBellotaId{ pair.first };
        BellotaPack& animatedBellotaPack = pair.second;
        const Bellota& animatedBellota = animatedBellotaPack.bellota;

        animatedBellotaPack.meshOpt = generateMesh(mTexturesArrays, animatedBellota);

        animatedBellotaPack.dmeshOpt = DMesh();
        DMesh& dmesh = animatedBellotaPack.dmeshOpt.value();
        dmesh.initBuffers();
        setupVAO(dmesh, mAnimatedShaderProgram);
        dmesh.fillBuffers(animatedBellotaPack.meshOpt.value(), GL_STATIC_DRAW);
        TextureId textureId = animatedBellota.texture();
        const TextureArrayPack& textureArrayPack = mTexturesArrays.at(textureId.id);

        debugCheck(textureArrayPack.dtextureOpt.has_value(), "Texture array has not been initializad on GPU.");

        const DTextureArray dtexture = textureArrayPack.dtextureOpt.value();

        dmesh.texture = dtexture.textureArray;
    }

    // state variable
    bool fillPolygons = true;

    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);

    glm::mat3 worldTransformMat(1.0);
    worldTransformMat = glm::translate(worldTransformMat, glm::vec2(-1.0, -1.0));
    const glm::vec2 worldScale(
        2.0f / mScreenSize.width,
        2.0f / mScreenSize.height
    );
    worldTransformMat = glm::scale(worldTransformMat, worldScale);

    const auto dLayerIndexLocation0 = glGetUniformLocation(mShaderProgram, "layerIndex");
    const auto dTransformLocation = glGetUniformLocation(mShaderProgram, "transform");
    const auto dTintColorLocation = glGetUniformLocation(mShaderProgram, "tintColor");
    const auto dTintIntensityLocation = glGetUniformLocation(mShaderProgram, "tintIntensity");
    
    // TA = TextureArray
    const auto dTATransformLocation = glGetUniformLocation(mAnimatedShaderProgram, "transform");
    const auto dTATintColorLocation = glGetUniformLocation(mAnimatedShaderProgram, "tintColor");
    const auto dTATintIntensityLocation = glGetUniformLocation(mAnimatedShaderProgram, "tintIntensity");
    const auto dLayerIndexLocation = glGetUniformLocation(mAnimatedShaderProgram, "layerIndex");

    PerformanceMonitor performanceMonitor(glfwGetTime(), 0.5f);

    // Dirty fix to sort bellotas by depth as required by transparent objects.
    std::vector<const BellotaPack*> sortedBellotaPacks;
    std::vector<const BellotaPack*> sortedAnimatedBellotaPacks;

    // Leaving some room in case more bellotas are created during runtime.
    sortedBellotaPacks.reserve(mBellotas.size()*2);
    sortedAnimatedBellotaPacks.reserve(mAnimatedBellotas.size()*2);

    auto sortByDepthOffset = [](const BellotaContainer& bellotas, std::vector<const BellotaPack*>& sortedBellotas)
    {
        // Per spec, clear does not change the underlaying memory allocation (capacity)
        sortedBellotas.clear();

        for (const auto& pair : bellotas.map())
        {
            const BellotaPack& bellotaPack = pair.second;
            sortedBellotas.push_back(&bellotaPack);
        }

        std::sort(sortedBellotas.begin(), sortedBellotas.end(),
            [](const BellotaPack* lhs, const BellotaPack* rhs)
            {
                debugCheck(lhs != nullptr and rhs != nullptr, "invalid pointers");
                const auto lhsDepthOffset = lhs->bellota.depthOffset();
                const auto rhsDepthOffset = rhs->bellota.depthOffset();
                return lhsDepthOffset < rhsDepthOffset;
            });
    };

    while (!glfwWindowShouldClose(mWindow->glfwWindow))
    {
        controller.processInputs();

        performanceMonitor.update(glfwGetTime());
        const float deltaTimeMS = performanceMonitor.getMS();
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // executing user provided update
        update(deltaTimeMS);

        sortByDepthOffset(mBellotas, sortedBellotaPacks);
        sortByDepthOffset(mAnimatedBellotas, sortedAnimatedBellotaPacks);

        // drawing with OpenGL
        glUseProgram(mShaderProgram);

        for (auto& bellotaPackPtr : sortedBellotaPacks)
        {
            debugCheck(bellotaPackPtr != nullptr, "invalid pointer to bellota pack.");
            auto& bellotaPack = *bellotaPackPtr;
            debugCheck(bellotaPack.dmeshOpt.has_value(), "DMesh has not been initialized.");

            const Bellota& bellota = bellotaPack.bellota;

            if (not bellota.visible())
                continue;

            const DMesh& dmesh = bellotaPack.dmeshOpt.value();
            const Transform& modelTransform = bellota.transform();
            const glm::mat3 modelTransformMat = modelTransform.toMat3();
            const glm::mat3 totalTransformMat = worldTransformMat * modelTransformMat;

            if (bellotaPack.tintOpt != std::nullopt)
            {
                const Tint& tint = bellotaPack.tintOpt.value();
                glUniform3f(dTintColorLocation, tint.color.r, tint.color.g, tint.color.b);
                glUniform1f(dTintIntensityLocation, tint.intensity);
            }
            else
            {
                glUniform3f(dTintColorLocation, 1.0f, 1.0f, 1.0f);
                glUniform1f(dTintIntensityLocation, 0.0f);
            }
            glUniformMatrix3fv(dTransformLocation, 1, GL_FALSE, glm::value_ptr(totalTransformMat));
            glUniform1i(dLayerIndexLocation0, bellota.currentLayer());
            
            dmesh.drawCall();
        }
        

        glUseProgram(mAnimatedShaderProgram);

        for (auto& animatedBellotaPackPtr : sortedAnimatedBellotaPacks)
        {
            debugCheck(animatedBellotaPackPtr != nullptr, "invalid pointer to bellota pack.");
            auto& animatedBellotaPack = *animatedBellotaPackPtr;
            debugCheck(animatedBellotaPack.dmeshOpt.has_value(), "DMesh3D has not been initialized.");

            const Bellota& animatedBellota = animatedBellotaPack.bellota;

            if (not animatedBellota.visible())
                continue;

            const DMesh& dmesh = animatedBellotaPack.dmeshOpt.value();
            const Transform& modelTransform = animatedBellota.transform();
            const glm::mat3 modelTransformMat = modelTransform.toMat3();
            const glm::mat3 totalTransformMat = worldTransformMat * modelTransformMat;

            if (animatedBellotaPack.tintOpt != std::nullopt)
            {
                const Tint& tint = animatedBellotaPack.tintOpt.value();
                glUniform3f(dTATintColorLocation, tint.color.r, tint.color.g, tint.color.b);
                glUniform1f(dTATintIntensityLocation, tint.intensity);
            }
            else
            {
                glUniform3f(dTATintColorLocation, 1.0f, 1.0f, 1.0f);
                glUniform1f(dTATintIntensityLocation, 0.0f);
            }
            glUniformMatrix3fv(dTATransformLocation, 1, GL_FALSE, glm::value_ptr(totalTransformMat));
            glUniform1i(dLayerIndexLocation, animatedBellota.currentLayer());
            
            dmesh.drawCall();
        }

        if (mStats)
        {
            ImGui::Begin("stats");
            ImGui::Text("%.2f fps", performanceMonitor.getFPS());
            ImGui::Text("%.2f ms", performanceMonitor.getMS());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(mWindow->glfwWindow);
        glfwPollEvents();
    }

    for (auto& pair : mBellotas.map())
    {
        BellotaPack& bellotaPack = pair.second;
        DMesh& dmesh = bellotaPack.dmeshOpt.value();
        dmesh.clear();
    }

    for (auto& pair : mAnimatedBellotas.map())
    {
        BellotaPack& bellotaPack = pair.second;
        DMesh& dmesh = bellotaPack.dmeshOpt.value();
        dmesh.clear();
    }
}


void Canvas::CanvasImpl::close()
{
    glfwSetWindowShouldClose(mWindow->glfwWindow, true);
}

}

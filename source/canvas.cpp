/*#ifdef _WIN32
#include <windows.h>
#else
#define APIENTRY
#endif*/

#include "canvas.h"
#include "check.h"
#include "bellota_to_mesh.h"
#include "dmesh.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <nothofagus.h>
#include <argparse/argparse.hpp>
#include <ciso646>


namespace Nothofagus
{

// Wrapper class forward declared in the .h to avoid including GLFW dependecies in the header file.
struct Canvas::Window
{
    GLFWwindow* glfwWindow;
};

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

Canvas::Canvas(const ScreenSize& screenSize, const std::string& title, const glm::vec3 clearColor):
    mScreenSize(screenSize),
    mTitle(title),
    mClearColor(clearColor)
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    GLFWwindow* glfwWindow = glfwCreateWindow(DEFAULT_SCREEN_SIZE.width, DEFAULT_SCREEN_SIZE.height, DEFAULT_TITLE.c_str(), NULL, NULL);
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
        void main()
        {
           outColor = texture(textureSampler, outTextureCoordinates);
        }
    )";

    // build and compile our shader program
    
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* vertexShaderSource_c_str = static_cast<const GLchar*>(vertexShaderSource.c_str());
    glShaderSource(vertexShader, 1, &vertexShaderSource_c_str, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (not success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        spdlog::error("Vertex shader compilation failed {}", infoLog);
        throw;
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fragmentShaderSource_c_str = static_cast<const GLchar*>(fragmentShaderSource.c_str());
    glShaderSource(fragmentShader, 1, &fragmentShaderSource_c_str, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (not success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        spdlog::error("Fragment shader compilation failed {}", infoLog);
        throw;
    }
    // link shaders
    mShaderProgram = glCreateProgram();
    glAttachShader(mShaderProgram, vertexShader);
    glAttachShader(mShaderProgram, fragmentShader);
    glLinkProgram(mShaderProgram);
    // check for linking errors
    glGetProgramiv(mShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(mShaderProgram, 512, NULL, infoLog);
        spdlog::error("Shader program linking failed {}", infoLog);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ImGui setup
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(mWindow->glfwWindow, true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);
}

//void setupVAO(DMesh& dMesh, GPUID shaderProgram)
void setupVAO(DMesh& dmesh, unsigned int shaderProgram)
{
    // Binding VAO to setup
    glBindVertexArray(dmesh.vao);

    // Binding buffers to the current VAO
    glBindBuffer(GL_ARRAY_BUFFER, dmesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dmesh.ebo);

    const unsigned int positionAttribLength = 2;
    const auto positionAttribLocation = glGetAttribLocation(shaderProgram, "position");
    glVertexAttribPointer(positionAttribLocation, positionAttribLength, GL_FLOAT, GL_FALSE, positionAttribLength * sizeof(GLfloat), (void*)(0 * sizeof(GLfloat)));
    glEnableVertexAttribArray(positionAttribLocation);

    const unsigned int textureAttribLength = 2;
    const auto textureAttribLocation = glGetAttribLocation(shaderProgram, "texture");
    glVertexAttribPointer(textureAttribLocation, textureAttribLength, GL_FLOAT, GL_FALSE, textureAttribLength * sizeof(GLfloat), (void*)(positionAttribLength * sizeof(GLfloat)));
    glEnableVertexAttribArray(textureAttribLocation);

    // Unbinding current VAO
    glBindVertexArray(0);
}


void Canvas::run()
{
    //debugCheck(mWindow->glfwWindow != nullptr, "GLFW Window has not been initialized.");
    
    // TODO: create IndexedContainerIterator
    for (auto& pair : mBellotas.map())
    {
        const BellotaId bellotaId{ pair.first };
        BellotaPack& bellotaPack = pair.second;

        bellotaPack.meshOpt = generateMesh(mTextures, bellotaPack.bellota);

        bellotaPack.dmeshOpt = DMesh();
        DMesh& dmesh = bellotaPack.dmeshOpt.value();
        dmesh.initBuffers();
        setupVAO(dmesh, mShaderProgram);
        dmesh.fillBuffers(bellotaPack.meshOpt.value(), GL_STATIC_DRAW);
    }

    // state variable
    bool fillPolygons = true;

    glClearColor(mClearColor.x, mClearColor.y, mClearColor.z, 1.0f);

    const auto dTransformLocation = glGetUniformLocation(mShaderProgram, "transform");

    while (!glfwWindowShouldClose(mWindow->glfwWindow))
    {
        processInput(mWindow->glfwWindow);
        
        glClear(GL_COLOR_BUFFER_BIT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Hi ImGui");
        ImGui::Text("Prepare your colors...");
        ImGui::Checkbox("Fill Polygons?", &fillPolygons);
        glPolygonMode(GL_FRONT_AND_BACK, fillPolygons ? GL_FILL : GL_LINE);
        ImGui::End();

        // drawing with OpenGL
        glUseProgram(mShaderProgram);

        for (auto& pair : mBellotas.map())
        {
            const BellotaId bellotaId{ pair.first };
            BellotaPack& bellotaPack = pair.second;

            debugCheck(bellotaPack.dmeshOpt.has_value(), "DMesh has not been initialized.");

            const Bellota& bellota = bellotaPack.bellota;
            const DMesh& dmesh = bellotaPack.dmeshOpt.value();
            const glm::mat3 transform = bellota.transform().toMat3();

            glUniformMatrix3fv(dTransformLocation, 1, GL_FALSE, glm::value_ptr(transform));
            dmesh.drawCall();
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
}

Canvas::~Canvas()
{
    // freeing GPU memorygpuShape.clear();
    glfwTerminate();

    // Needs to be defined in the cpp file to avoid incomplete type errors due to the pimpl idiom for struct Window
}

void Canvas::close()
{
    glfwSetWindowShouldClose(mWindow->glfwWindow, true);
}

}

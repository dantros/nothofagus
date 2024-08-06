
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <whereami2cpp.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>

struct Screen
{
    unsigned int width, height;
};

// default settings
constexpr Screen SCREEN{ 800, 600 };
const std::string DEFAULT_CONFIG_FILE = "config.json";

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

nlohmann::json defaultConfigFile()
{
    nlohmann::json defaultConfig
    {
        { "background_color", {0.3f, 0.3f, 0.3f, 1.0f}},
        { "vertices",
            {
                0.5f,  0.6f, 0.0f, 1.0f, 0.0f, 0.0f, // top right
                0.4f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // bottom right
                -0.6f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom left
                -0.7f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f   // top left 
            }
        },
        { "indices",
            {
                0, 1, 3,  // first Triangle
                1, 2, 3   // second Triangle
            }
        }
    };

    return defaultConfig;
}

int main(int argc, char *argv[])
{
    const std::string appName = "C++ OpenGL App Template";
    const std::string executablePath = whereami::get_executable_path();
    spdlog::info("Running {}\n    located at {}", appName, executablePath);

    glm::vec2 location(0.5,0.25);
    spdlog::info("Testing glm vec2: {}, {}", location.x, location.y);

    argparse::ArgumentParser program(appName.c_str());

    // Loading/Creating a json file with a polygon to draw.
    
    std::string configFilepath;
    program.add_argument("-f", "--file")
        .help("file name of configuration file with the polygon to draw")
        .default_value(std::string(DEFAULT_CONFIG_FILE))
        .store_into(configFilepath);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        spdlog::error("Error when parsing arguments. {}", err.what());
        return 1;
    }

    const bool configExits = std::filesystem::is_regular_file(configFilepath);
    nlohmann::json configFile; 
    if (configExits)
    {
        spdlog::info("Configuration file {} found. Parsing it.", configFilepath);
        std::ifstream inputStream(configFilepath);
        configFile = nlohmann::json::parse(inputStream);
    }
    else
    {
        spdlog::info("Configuration file {} does not exist, creating it in the current directory with default parameters.", configFilepath);
        configFile = defaultConfigFile();
        std::ofstream outputStream(configFilepath);
        outputStream << configFile.dump(4);
        outputStream.close();
    }
    spdlog::info("Configuration file content:\n {}", configFile.dump(4));

    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCREEN.width, SCREEN.height, appName.c_str(), NULL, NULL);
    if (window == nullptr)
    {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        spdlog::error("Failed to initialize GLAD");
        return -1;
    }

    const std::string vertexShaderSource = R"(
        #version 330 core
        in vec3 position;
        in vec3 color;
        out vec3 fragColor;
        void main()
        {
            fragColor = color;
            gl_Position = vec4(position.x, position.y, position.z, 1.0);
        }
    )";
    const std::string fragmentShaderSource = R"(
        #version 330 core
        in vec3 fragColor;
        out vec4 outColor;
        void main()
        {
           outColor = vec4(fragColor, 1.0f);
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
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        spdlog::error("Vertex shader compilation failed {}", infoLog);
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fragmentShaderSource_c_str = static_cast<const GLchar*>(fragmentShaderSource.c_str());
    glShaderSource(fragmentShader, 1, &fragmentShaderSource_c_str, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        spdlog::error("Fragment shader compilation failed {}", infoLog);
    }
    // link shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        spdlog::error("Shader program linking failed {}", infoLog);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    std::vector<float> backgroundColor = configFile["background_color"];
    std::vector<float> vertices = configFile["vertices"];
    std::vector<unsigned int> indices = configFile["indices"];

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    auto position = glGetAttribLocation(shaderProgram, "position");
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(position);

    auto color = glGetAttribLocation(shaderProgram, "color");
    glVertexAttribPointer(color, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(color);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);

    // ImGui setup
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // state variable
    bool fillPolygons = true;

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(backgroundColor[0], backgroundColor[1], backgroundColor[2], backgroundColor[3]);
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
        glUseProgram(shaderProgram);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ImGui Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // de-allocate OpenGL resources once they've outlived their purpose:
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();

    return 0;
}
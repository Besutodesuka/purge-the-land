#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model.h>

#include <go/obb.h>
#include <go/camera_3rd.h>
#include <go/player.h>

#include <iostream>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(3.0f, glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, 20.0f);
Player player(nullptr, glm::vec3(0.0f), glm::vec3(0.0f), 1.0f); // Temp init
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// PHYSICS
std::vector<OBB> levelColliders;

int main()
{
    // ------------------------------------------------------------------
    // GLFW Initialization
    // ------------------------------------------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Parkour Game (OBB Physics)", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // ------------------------------------------------------------------
    // OpenGL Configuration
    // ------------------------------------------------------------------
    stbi_set_flip_vertically_on_load(false);
    glEnable(GL_DEPTH_TEST);

    // Build and compile shaders
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");

    // ------------------------------------------------------------------
    // Load Models
    // ------------------------------------------------------------------
    // 1. Player Model
    Model playerModel(FileSystem::getPath("resources/objects/rat/cartoon-low-poly-rat-pack/rat.obj"));

    // 2. Level Model (Visual)
    std::cout << "Loading Level Model..." << std::endl;
    Model visualModel(FileSystem::getPath("resources/objects/map/map.obj"));

    // ------------------------------------------------------------------
    // Player Setup
    // ------------------------------------------------------------------
    glm::vec3 playerStartPos(0.0f, 5.0f, 0.0f); // Start higher to drop in
    glm::vec3 playerBoxSize(0.2f, 0.35f, 0.4f); // Width, Height, Depth
    float playerModelScale = 0.2f;

    // Re-initialize global player with loaded model
    player = Player(&playerModel, playerStartPos, playerBoxSize, playerModelScale);

    // ------------------------------------------------------------------
    // Generate Hitboxes (Auto-Generate from Visual Model)
    // ------------------------------------------------------------------
    std::cout << "Generating OBB Colliders from Level..." << std::endl;

    // Define Level Transform (Rotation/Scale/Position)
    // If your map is too big/small, change the scale here.
    glm::mat4 levelMatrix = glm::mat4(1.0f);
    // levelMatrix = glm::scale(levelMatrix, glm::vec3(0.1f)); // Example scaling

    // *** THE MAGIC FUNCTION ***
    // This scans every mesh in your visual model and creates a box for it.
    levelColliders = GenerateOBBsFromModel(visualModel, levelMatrix);

    std::cout << "Generated " << levelColliders.size() << " collision boxes." << std::endl;
    std::cout << "Starting Main Loop..." << std::endl;

    std::cout << "DEBUG: Level loaded with " << visualModel.meshes.size() << " meshes." << std::endl;
    std::cout << "DEBUG: Generated " << levelColliders.size() << " collision boxes." << std::endl;

    if (levelColliders.size() <= 1) {
        std::cout << "WARNING: Your level is only 1 solid object!" << std::endl;
        std::cout << "       The physics will treat it as a solid block you cannot enter." << std::endl;
        std::cout << "       Solution: Export your OBJ with 'Split by Objects' or 'Keep Vertex Order'." << std::endl;
    }

    // ------------------------------------------------------------------
    // Main Render Loop
    // ------------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        // Per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window, &player, &camera, deltaTime);

        // Physics Update
        player.Update(deltaTime, levelColliders);

        // Camera Logic (Follow Player)
        glm::vec3 cameraTarget = player.GetBoxCenter();
        camera.SetIsometricView(cameraTarget, 2.0f);

        // Render
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        // View/Projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);


        // 3. Draw the Player with the original Shader (since it works)
        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        player.Draw(ourShader);

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Input Handling
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    std::vector<int> direction = { 0,0,0,0 }; // W, A, S, D

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction[0] = 1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction[1] = 1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction[2] = 1;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction[3] = 1;

    player->ProcessKeyboard(camera->Front, camera->Right, direction, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player->Jump();
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    double angle = atan2(ypos - (SCR_HEIGHT / 2.0), xpos - (SCR_WIDTH / 2.0));
    player.SetDirectionByMouse(angle);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
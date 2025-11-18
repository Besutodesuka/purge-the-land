#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model.h>

// Our new class headers
#include <go/physic.h>
#include <go/camera_3rd.h>
#include <go/player.h>

#include <iostream>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
// Modified processInput to pass the player
void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime);

// settings
const unsigned int SCR_WIDTH = 1980;
const unsigned int SCR_HEIGHT = 1080;

// camera
// We now initialize our new camera. All logic is inside the class.
Camera camera(3.0f, glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, 20.0f); // 3.0f is the starting distance
Player player(nullptr, glm::vec3(0.0f), glm::vec3(0.0f), 1.0f); // Temporary initialization
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Scene collision boxes
std::vector<AABB> levelColliders;

int main()
{
    // ... (glfw initialization, window creation, glad loading - NO CHANGES) ...
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Parkour Game", NULL, NULL);
    if (window == NULL) { /* ... error check ... */ return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { /* ... error check ... */ return -1; }
    // ------------------------------------------------------------------

    stbi_set_flip_vertically_on_load(false); // Most OBJ models need this
	glEnable(GL_DEPTH_TEST);// Enable depth testing ensure rendering order

    // Build and compile shaders
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");

    // Load models
    // We follow the plan: one visual model, one collision model
    Model playerModel(FileSystem::getPath("resources/objects/rat/cartoon-low-poly-rat-pack/rat.obj"));
    Model visualModel(FileSystem::getPath("resources/objects/map/world/pine_forest.obj"));
    Model collisionModel(FileSystem::getPath("resources/objects/map/world/hitbox_map.obj")); 

    // Create the Player
    glm::vec3 playerStartPos(0.0f, 40.0f, 0.0f);
    glm::vec3 playerBoxSize(0.2f, 0.35f, 0.4f); // get collision box from AABB function/static for optimization maybe visualize them to see if it is ok or not
    float playerModelScale = 0.2f;
    player = Player(&playerModel, playerStartPos, playerBoxSize, playerModelScale);
	Model GroundModel(FileSystem::getPath("resources/objects/map/world/ground/ground.obj"));
	player.SetGroundModel(&GroundModel);
    
    // Populate the level colliders vector from the invisible collision model
    std::cout << "Building collision geometry..." << std::endl;
   for (Mesh& mesh : collisionModel.meshes)
    {
        glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::lowest());

        for (Vertex& vertex : mesh.vertices)
        {
            minAABB.x = std::min(minAABB.x, vertex.Position.x);
            minAABB.y = std::min(minAABB.y, vertex.Position.y);
            minAABB.z = std::min(minAABB.z, vertex.Position.z);
            maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
            maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
            maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
    }
    levelColliders.push_back(AABB(minAABB, maxAABB));
    }
    std::cout << "Finished building collision geometry..." << std::endl;

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window, &player, &camera, deltaTime);

        // Update player logic (physics, collision)
        player.Update(deltaTime, levelColliders);

		// Update camera position based on player
        glm::vec3 cameraTarget = player.GetBoxCenter(); // Follow center of collision box
        camera.SetIsometricView(cameraTarget, 3.0f); // Initial isometric view

        // --- Third-Person Camera Collision Logic (from your code) ---
        // 1. Define the "target" (player's head/center)

        // 2. Define the ray for camera collision
        //glm::vec3 rayOrigin = cameraTarget;
        //glm::vec3 rayDir = -camera.Front; // Ray goes *backwards* from the player
        //float maxCameraDist = camera.TargetDistance; // From mouse scroll
        //float closestHitDist = maxCameraDist;
        //float hitDist;

        //// 3. Check for intersections
        //for (const AABB& box : levelColliders) {
        //    if (rayIntersectsAABB(rayOrigin, rayDir, box, hitDist) && hitDist < closestHitDist && hitDist > 0.0f) {
        //        closestHitDist = hitDist;
        //    }
        //}

        //// 4. Calculate final distance
        //float actualCameraDist = std::min(maxCameraDist, closestHitDist - 0.2f); // Use desired dist or hit dist
        //if (actualCameraDist < 0.2f) actualCameraDist = 0.2f; // Don't go inside player

        //// 5. Set final camera position
        //glm::vec3 finalCamPos = cameraTarget - (camera.Front * actualCameraDist);
        //camera.SetFinalPosition(finalCamPos, cameraTarget);


        // render
        // ------
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // render the visual level
        glm::mat4 model = glm::mat4(1.0f);
        //model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)); 
        ourShader.setMat4("model", model);
        visualModel.Draw(ourShader);
		// GroundModel.Draw(ourShader);

        // Render the player
        player.Draw(ourShader);

        // glfw: swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// processInput:
// Now passes input to the Player class
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime)
{
	// determine movement direction from mouse position
	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
    // draw UI
	
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
	std::vector<int> direction = {0,0,0,0};
    //Camera_Movement direction = NONE;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        direction[0] = 1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        direction[1] = 1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        direction[2] = 1;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        direction[3] = 1;

    player->ProcessKeyboard(camera->Front, camera->Right, direction, deltaTime);

    // Handle jumping separately (no "UP")
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player->Jump();
}
// framebuffer_size_callback (NO CHANGES)
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// mouse_callback:
// Simplified to just call the camera class
// ---------------------------------------------------------------------------------------------
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
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

	double angle = atan2(ypos - (SCR_HEIGHT / 2.0), xpos - (SCR_WIDTH / 2.0));

    player.SetDirectionByMouse(angle);

    // always draw the cursor skin when in combat mode (equip bow as defualt so we dontgot head ache with animation)
    // turn the player model based on mouse position relative to the center of the screen
    // player.
    // get radians from center
    // trans form model (rotate with z-axis)

     //camera.ProcessMouseMovement(xoffset, yoffset);
}

// scroll_callback (NO CHANGES)
// ---------------------------------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    //camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
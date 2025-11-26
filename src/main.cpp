#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model_animation.h>

// Our new class headers
#include <go/physic.h>
#include <go/camera_3rd.h>
#include <go/player.h>
#include <go/ui.h>
#include <go/arrow.h>
#include <go/debug_renderer.h>
#include <go/enemy.h>

#include <iostream>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
// Modified processInput to pass the player
void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime);
glm::vec3 GetRayFromMouse(float mouseX, float mouseY, int screenW, int screenH, glm::mat4 view, glm::mat4 projection);

// settings
const unsigned int SCR_WIDTH = 1980;
const unsigned int SCR_HEIGHT = 1080;

// camera
// We now initialize our new camera. All logic is inside the class.
Camera camera(3.0f, glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, 20.0f); // 3.0f is the starting distance
Player* player = nullptr; // Temporary initialization
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

Reticle myReticle;
float sensitivity = 1.0f;
// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

DebugRenderer debugRenderer;
// state
int BowMode = 0; // 0 = idle, 1 = draw, 2 = shoot

// Scene collision boxes
std::vector<OBB> levelColliders;

EnemyManager enemyManager;


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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Purge the land", NULL, NULL);
    if (window == NULL) { /* ... error check ... */ return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { /* ... error check ... */ return -1; }
    // ------------------------------------------------------------------

    stbi_set_flip_vertically_on_load(true); // Most OBJ models need this
	glEnable(GL_DEPTH_TEST);// Enable depth testing ensure rendering order

    // Build and compile shaders
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");
    Shader cursorShader("crosshair.vs", "crosshair.fs");
    Shader AnimationShader("anim_model.vs", "anim_model.fs");

    // Load models
    // We follow the plan: one visual model, one collision model
    Model playerModel(FileSystem::getPath("resources/objects/player/Erika Archer With Bow Arrow.dae"));
    Model arrowModel(FileSystem::getPath("resources/objects/arrow/arrow.obj"));
    Model visualModel(FileSystem::getPath("resources/objects/map/map.obj"));
	Model skyboxModel(FileSystem::getPath("resources/objects/skybox/skybox.obj"));
    //Model collisionModel(FileSystem::getPath("resources/objects/map/world/hitbox_map.obj")); 

    // Create the Player
    glm::vec3 playerStartPos(10.0f, 40.0f, 10.0f);
    glm::vec3 playerBoxSize(0.2f, 0.35f, 0.4f); // get collision box from AABB function/static for optimization maybe visualize them to see if it is ok or not
    float playerModelScale = 0.5f;
    player = new Player(&playerModel, playerStartPos, playerBoxSize, playerModelScale);
	Model GroundModel(FileSystem::getPath("resources/objects/ground/ground.obj"));
	Model TargetModel(FileSystem::getPath("resources/objects/target/target.obj"));
	player->SetGroundModel(&GroundModel);
	player->SetArrowManager(&arrowModel, 0.01f);
    
    // // Populate the level colliders vector from the invisible collision model
    // std::cout << "Building collision geometry..." << std::endl;
    // for (Mesh& mesh : collisionModel.meshes)
    // {
    //     glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
    //     glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::lowest());

    //     for (Vertex& vertex : mesh.vertices)
    //     {
    //         minAABB.x = std::min(minAABB.x, vertex.Position.x);
    //         minAABB.y = std::min(minAABB.y, vertex.Position.y);
    //         minAABB.z = std::min(minAABB.z, vertex.Position.z);
    //         maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
    //         maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
    //         maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
    //     }
    //     levelColliders.push_back(AABB(minAABB, maxAABB));
    // }
    // std::cout << "Finished building collision geometry..." << std::endl;
    // for (Mesh& mesh : TargetModel.meshes)
    // {
    //     glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
    //     glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::lowest());

    //     for (Vertex& vertex : mesh.vertices)
    //     {
    //         minAABB.x = std::min(minAABB.x, vertex.Position.x);
    //         minAABB.y = std::min(minAABB.y, vertex.Position.y);
    //         minAABB.z = std::min(minAABB.z, vertex.Position.z);
    //         maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
    //         maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
    //         maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
    //     }
    //     monsterColliders.push_back(AABB(minAABB, maxAABB));
    // }
    // std::cout << "Finished building collision geometry..." << std::endl;

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

    // Spawn 3 enemies at different locations
    //enemies.emplace_back(&arrowModel, glm::vec3(5.0f, 0.5f, 5.0f), 0.5f, 100.0f);
    //enemies.emplace_back(&TargetModel, glm::vec3(-5.0f, 0.5f, 5.0f), 10.0f, 100.0f);
    //enemies.emplace_back(&TargetModel, glm::vec3(0.0f, 0.5f, -8.0f), 15.0f, 150.0f); // Big boss

    // 2. Init Manager
    enemyManager.Init(&TargetModel);

    // 3. Spawn Enemies (Note the smaller scale!)
    enemyManager.Spawn(glm::vec3(5.0f, 0.5f, 5.0f), 0.2f, 100.0f);
    enemyManager.Spawn(glm::vec3(-5.0f, 0.5f, 5.0f), 0.2f, 100.0f);
    enemyManager.Spawn(glm::vec3(0.0f, 0.5f, -8.0f), 0.3f, 150.0f);
    levelColliders = GenerateOBBsFromModel(visualModel, levelMatrix);

    debugRenderer.Init();
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window, player, &camera, deltaTime);

        // Update player logic (physics, collision)
        // player.Update(deltaTime, levelColliders); //TODO: this one is needed but the map needed to be fix first
        player->Update(deltaTime, &levelColliders);
        enemyManager.Update(deltaTime, player->arrowManager);
		player->arrowManager.Render(ourShader);

		// Update camera position based on player
        glm::vec3 cameraTarget = player->GetBoxCenter(); // Follow center of collision box
        camera.SetIsometricView(cameraTarget, 5.0f); // Initial isometric view

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
        glm::mat4 model = glm::mat4(1.0f);
        ourShader.setMat4("model", model);

        // render the visual level
        //model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		skyboxModel.Draw(ourShader);
        visualModel.Draw(ourShader);
		GroundModel.Draw(ourShader);

  //      model = glm::mat4(1.0f);
		//model = glm::translate(model, glm::vec3(-1.0f, -0.25f, 1.0f));
  //      model = glm::scale(model, 0.5f*glm::vec3(1.0f, 1.0f, 1.0f));
  //      ourShader.setMat4("model", model);
		//TargetModel.Draw(ourShader);

        // Draw the yellow circle at the aim target
        // Render the player

        // One line to draw all enemies
        enemyManager.Render(ourShader);
        
        AnimationShader.use();
        model = glm::mat4(1.0f);
        AnimationShader.setMat4("model", model);
        AnimationShader.setMat4("projection", projection);
        AnimationShader.setMat4("view", view);
        player->Draw(AnimationShader, false);

        cursorShader.use();
        cursorShader.setMat4("view", view);
        cursorShader.setMat4("projection", projection);
        model = glm::mat4(1.0f);
        cursorShader.setMat4("model", model);
        glm::vec3 flatDir = glm::normalize(glm::vec3(player->ShootDirection.x, 0.0f, player->ShootDirection.z));
        glm::vec3 reticlePos = player->Position + (flatDir * player->currentAimDist);
        myReticle.Draw(cursorShader, reticlePos);
        // One line to draw all Health Bars
        enemyManager.RenderUI(cursorShader);

        //debug hit box
        for (const Arrow& a : player->arrowManager.arrows) {
            debugRenderer.DrawOBB(a.hitbox, view, projection);
		}
        for (const auto& enemy : enemyManager.GetEnemies()) {
            debugRenderer.DrawOBB(enemy.Collider, view, projection);
        }
        debugRenderer.DrawOBB(player->Collider, view, projection);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime)
{
    // --- 1. KEYBOARD MOVEMENT (No Changes) ---
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    std::vector<int> direction = { 0,0,0,0 };
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) direction[0] = 1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) direction[1] = 1;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) direction[2] = 1;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) direction[3] = 1;

    player->ProcessKeyboard(camera->Front, camera->Right, direction, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player->Jump();

    // --- 2. CONSTANT SENSITIVITY AIMING ---

    // A. Get Mouse Position & Offsets
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // Calculate how far mouse is from center of screen (in pixels)
    float deltaX = (float)mouseX - (SCR_WIDTH / 2.0f);
    float deltaY = (float)mouseY - (SCR_HEIGHT / 2.0f);

    // B. Get Camera Direction Vectors (Flattened to ignore Y/Height)
    // This ensures "Up" on mouse pad always equals "Forward" on ground
    glm::vec3 flatFront = glm::normalize(glm::vec3(camera->Front.x, 0.0f, camera->Front.z));
    glm::vec3 flatRight = glm::normalize(glm::vec3(camera->Right.x, 0.0f, camera->Right.z));

    // C. Map Screen Pixels to World Meters
    // SENSITIVITY: 0.025f means 100 pixels = 2.5 meters. Tweak this number!
    

    // Combine vectors: 
    // +DeltaX (Right on screen) moves along Camera Right
    // +DeltaY (Down on screen) moves BACKWARDS along Camera Front (so we negate it)
    glm::vec3 aimVector = (flatRight * deltaX * sensitivity) - (flatFront * deltaY * sensitivity);

    // D. Extract Data for Player
    float dist = glm::length(aimVector);

    // Handle the case where mouse is exactly at center (dist = 0) to avoid NaN
    if (dist > 0.001f) {
        float angle = atan2(aimVector.x, aimVector.z);

        // Pass the linear distance directly.
        // The player class clamp (1.0f to 12.0f) will still apply to keep it valid.
        player->SetDirectionByMouse(angle, dist);
    }

    // --- 3. SHOOTING LOGIC (No Changes) ---
    static int BowMode = 0;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        player->ProcessMouse(1, deltaTime);
        if (BowMode == 0) BowMode = 1;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        if (BowMode == 1) {
            player->ProcessMouse(0, deltaTime);
            BowMode = 0;
        }
    }
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
    //float xpos = static_cast<float>(xposIn);
    //float ypos = static_cast<float>(yposIn);

    //if (firstMouse)
    //{
    //    lastX = xpos;
    //    lastY = ypos;
    //    firstMouse = false;
    //}

    //// We don't need xoffset/yoffset for isometric cursor logic
    //lastX = xpos;
    //lastY = ypos;

    //// 1. Calculate Offsets from Center of Screen
    //// (We assume Player is always in the center of the screen)
    //float deltaX = xpos - (SCR_WIDTH / 2.0f);
    //float deltaY = ypos - (SCR_HEIGHT / 2.0f);

    //// 2. Calculate Angle
    //// Note: 'atan2' takes (y, x). We use deltaY, deltaX.
    //double angle = atan2(deltaY, deltaX);

    //float distPixels = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    //
    //player->SetDirectionByMouse(angle, distPixels);
}

// scroll_callback (NO CHANGES)
// ---------------------------------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    //camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// Returns the 3D direction vector of the ray coming out of the camera
glm::vec3 GetRayFromMouse(float mouseX, float mouseY, int screenW, int screenH, glm::mat4 view, glm::mat4 projection) {
    // 1. Convert to Normalized Device Coordinates (NDC)
    // Range [-1, 1], with (0,0) in center
    float x = (2.0f * mouseX) / screenW - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenH; // Invert Y for OpenGL
    float z = 1.0f;
    glm::vec3 ray_nds = glm::vec3(x, y, z);

    // 2. Convert to Clip Coordinates
    glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0, 1.0);

    // 3. Convert to Eye (View) Coordinates
    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0); // We only want direction, not position

    // 4. Convert to World Coordinates
    glm::vec3 ray_wor = (glm::inverse(view) * ray_eye);
    ray_wor = glm::normalize(ray_wor);

    return ray_wor;
}
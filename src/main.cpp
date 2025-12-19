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
#include <go/menu.h>
#include <go/text_renderer.h>
#include <go/spawner.h>

#include <algorithm>
#include <iostream>
#include <vector>
#include <string> // Added for loading screen text

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processGameplayInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime);

enum GameState {
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER,
    GAME_VICTORY
};
GameState gameState = GAME_PLAYING;

// Helper for toggle
bool escPressedLastFrame = false;
bool showDebugHitboxes = false;
bool f3PressedLastFrame = false;
bool FREECAM = false;

// settings
const unsigned int SCR_WIDTH = 1980;
const unsigned int SCR_HEIGHT = 1080;

// camera
Camera camera(3.0f, glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, 20.0f);
float camera_distance = 7.5f;
Player* player = nullptr;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// UI Elements
PlayerHUD playerHUD;
Reticle myReticle;
GameOverScreen gameOverScreen;
PauseScreen pauseScreen;
VictoryScreen victoryScreen;
TextRenderer textRenderer;

float sensitivity = 1.0f;
// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// state
DebugRenderer debugRenderer;
std::vector<OBB> levelColliders;
EnemyManager enemyManager;
MobSpawner* mobSpawner = nullptr;
std::vector<MobSpawner> mobSpawners;

// Models (Now Pointers so we can control loading time)
Model* playerModel = nullptr;
Model* arrowModel = nullptr;
Model* visualModel = nullptr;
Model* DecorModel = nullptr;
Model* GroundModel = nullptr;
Model* skeletonModel = nullptr;
Model* mobSpawnerModel = nullptr;
Model* boarderModel = nullptr;
Model* boarderHitboxModel = nullptr;

// Reset Game Helper
void ResetGame() {
    if (player) player->Reset();
    if (player) player->arrowManager.arrows.clear();
    enemyManager.Reset();

    // Reset Spawner if it exists
    for (auto& mobSpawner : mobSpawners) {
        // Note: mobSpawner is a reference here, so no need for 'if (&mobSpawner)' check really
        mobSpawner.CurrentHealth = mobSpawner.MaxHealth;
        mobSpawner.IsDestroyed = false;
        mobSpawner.myEnemies.clear();
        mobSpawner.InitialSpawn(enemyManager);
    }
    gameState = GAME_PLAYING;
}

int main()
{
    // ------------------------------------------------------------------
    // 1. STANDARD INIT
    // ------------------------------------------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Purge the land", NULL, NULL);
    if (window == NULL) { return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Start locked
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Build and compile shaders
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");
    Shader AnimationShader("anim_model.vs", "anim_model.fs");
    Shader cursorShader("ui.vs", "ui.fs");

    // ------------------------------------------------------------------
    // 2. TEXT RENDERER INIT (Must happen first for Loading Screen)
    // ------------------------------------------------------------------
    std::cout << "Initializing Text Renderer..." << std::endl;
    textRenderer.Init(SCR_WIDTH, SCR_HEIGHT, "text.vs", "text.fs", "resources/fonts/Antonio-Bold.ttf");

    // --- LOADING SCREEN HELPER LAMBDA ---
    auto UpdateLoadingScreen = [&](std::string message) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw loading text
        textRenderer.RenderText(message, 50.0f, SCR_HEIGHT / 2.0f, 1.0f, glm::vec3(1.0f));

        // CRITICAL: Swap buffers and poll events to tell Windows "I am alive"
        glfwSwapBuffers(window);
        glfwPollEvents();
        };
    // ------------------------------------

    // ------------------------------------------------------------------
    // 3. SEQUENTIAL ASSET LOADING
    // ------------------------------------------------------------------

    UpdateLoadingScreen("Loading Player Model...");
    playerModel = new Model("resources/objects/player/Erika Archer With Bow Arrow.dae");

    UpdateLoadingScreen("Loading Arrow Model...");
    arrowModel = new Model("resources/objects/arrow/arrow.obj");

    UpdateLoadingScreen("Loading Level Structure...");
    visualModel = new Model("resources/objects/map/structure.obj");

    UpdateLoadingScreen("Loading Decoration...");
    DecorModel = new Model("resources/objects/map/decorations.obj");

    UpdateLoadingScreen("Loading Ground...");
    GroundModel = new Model("resources/objects/map/ground.obj");

    UpdateLoadingScreen("Loading Enemies...");
    skeletonModel = new Model("resources/objects/enemy/skelly.dae");

    UpdateLoadingScreen("Loading Spawners...");
    mobSpawnerModel = new Model("resources/objects/map/mobSpawner.obj");

    UpdateLoadingScreen("Loading Borders...");
    boarderModel = new Model("resources/objects/map/border.obj");
    boarderHitboxModel = new Model("resources/objects/map/border_hitbox.obj");

    // Create the Player
    UpdateLoadingScreen("Initializing Player...");
    glm::vec3 playerStartPos(0.0f, 10.0f, 0.0f);
    glm::vec3 playerBoxSize(0.2f, 1.6f, 0.4f);
    float playerModelScale = 1.0f;
    // Pass pointer directly (no &)
    player = new Player(playerModel, playerStartPos, playerBoxSize, playerModelScale);
    player->SetGroundModel(GroundModel);
    player->SetArrowManager(arrowModel, 0.01f);

    // Generate Hitboxes
    UpdateLoadingScreen("Generating Physics (This may take a moment)...");
    glm::mat4 levelMatrix = glm::mat4(1.0f);

    // Dereference pointers (*visualModel) to pass values to the vector
    levelColliders = GenerateOBBsFromModels(std::vector<Model>{*visualModel, * boarderHitboxModel}, levelMatrix);

    std::cout << "DEBUG: Level loaded with " << visualModel->meshes.size() << " meshes." << std::endl;
    std::cout << "DEBUG: Generated " << levelColliders.size() << " collision boxes." << std::endl;

    if (levelColliders.size() <= 1) {
        std::cout << "WARNING: Your level is only 1 solid object!" << std::endl;
    }

    // Initialize Spawners
    UpdateLoadingScreen("Initializing Spawners...");
    for (int counter = 0; counter < mobSpawnerModel->meshes.size(); counter++)
    {
        MobSpawner* mobSpawner = new MobSpawner(
            mobSpawnerModel,
            mobSpawnerModel->meshes[counter].vertices[0].Position + glm::vec3(0.0f, -4.0f, 0.0f),
            1.0f,
            counter
        );
        mobSpawner->InitialSpawn(enemyManager);
        mobSpawners.push_back(*mobSpawner);
    }

    enemyManager.Init(skeletonModel, &player->GroundModel);
    debugRenderer.Init();

    UpdateLoadingScreen("Starting Game...");

    // ------------------------------------------------------------------
    // 4. MAIN RENDER LOOP
    // ------------------------------------------------------------------
    lastFrame = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Global Input
        bool escPressed = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
        if (escPressed && !escPressedLastFrame) {
            if (gameState == GAME_PLAYING) {
                gameState = GAME_PAUSED;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else if (gameState == GAME_PAUSED) {
                gameState = GAME_PLAYING;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
        escPressedLastFrame = escPressed;

        bool f3Pressed = (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS);
        if (f3Pressed && !f3PressedLastFrame) {
            showDebugHitboxes = !showDebugHitboxes;
        }
        f3PressedLastFrame = f3Pressed;

        // Logic
        if (gameState == GAME_PLAYING) {
            if (player->IsDead) {
                gameState = GAME_OVER;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            // Check Victory Condition
            bool all_destroyed = true;
            for (auto& spawner : mobSpawners) {
                if (&spawner && !spawner.IsDestroyed) {
                    all_destroyed = false;
                }
            }
            if (all_destroyed) {
                gameState = GAME_VICTORY;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            processGameplayInput(window, player, &camera, deltaTime);
            player->Update(deltaTime, &levelColliders);

            // Update Spawner Logic
            for (auto& mobSpawner : mobSpawners) {
                if (&mobSpawner) {
                    mobSpawner.Update(deltaTime, enemyManager);

                    if (!mobSpawner.IsDestroyed) {
                        for (Arrow& arrow : player->arrowManager.arrows) {
                            if (!arrow.active) continue;
                            if (TestOBBOBB(mobSpawner.Collider, arrow.hitbox)) {
                                arrow.active = false;
                                float dmg = player->GetArrowDamage(arrow.velocity);
                                mobSpawner.TakeDamage(dmg);
                            }
                        }
                    }
                }
            }
            enemyManager.Update(deltaTime, player->arrowManager, *player);

            glm::vec3 cameraTarget = player->GetBoxCenter();
            cameraTarget.y += 1.0f;
            if (!FREECAM) camera.SetIsometricView(cameraTarget, camera_distance);
        }
        else if (gameState == GAME_PAUSED) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                int action = pauseScreen.ProcessClick((float)mx, (float)my, SCR_WIDTH, SCR_HEIGHT);
                if (action == 1) {
                    gameState = GAME_PLAYING;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                else if (action == 2) {
                    glfwSetWindowShouldClose(window, true);
                }
            }
        }
        else if (gameState == GAME_OVER) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                int action = gameOverScreen.ProcessClick((float)mx, (float)my, SCR_WIDTH, SCR_HEIGHT);
                if (action == 1) { // Retry
                    ResetGame();
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                else if (action == 2) { // Quit
                    glfwSetWindowShouldClose(window, true);
                }
            }
        }
        else if (gameState == GAME_VICTORY) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                int action = victoryScreen.ProcessClick((float)mx, (float)my, SCR_WIDTH, SCR_HEIGHT);
                if (action == 1) { // Retry
                    ResetGame();
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                else if (action == 2) { // Quit
                    glfwSetWindowShouldClose(window, true);
                }
            }
        }

        // Render Background
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", glm::mat4(1.0f));

        player->arrowManager.Render(ourShader);
        ourShader.setMat4("model", glm::mat4(1.0f));
        ourShader.setVec3("camPos", camera.Position);

        // Lights
        int lightCount = 0;
        std::string posName, colorName;

        // Player Light
        posName = "lightPositions[" + std::to_string(lightCount) + "]";
        colorName = "lightColors[" + std::to_string(lightCount) + "]";
        ourShader.setVec3(posName, player->Position + glm::vec3(0.0f, 1.5f, 0.0f));
        ourShader.setVec3(colorName, glm::vec3(20.0f, 10.0f, 1.0f) * 10.0f);
        lightCount++;

        // Draw Static Models (Access via pointers ->)
        visualModel->Draw(ourShader);
        GroundModel->Draw(ourShader);
        DecorModel->Draw(ourShader);
        boarderModel->Draw(ourShader);

        for (auto& mobSpawner : mobSpawners) {
            if (mobSpawner.IsDestroyed) continue;
            posName = "lightPositions[" + std::to_string(lightCount) + "]";
            colorName = "lightColors[" + std::to_string(lightCount) + "]";
            ourShader.setVec3(colorName, glm::vec3(20.0f, 5.0f, 20.0f) * 20.0f);
            ourShader.setVec3(posName, mobSpawner.Position + glm::vec3(0.0f, 10.5f, 0.0f));
            if (&mobSpawner) mobSpawner.Draw(ourShader);
            lightCount++;
        }
        ourShader.setInt("numLights", lightCount);

        // Animation
        AnimationShader.use();
        AnimationShader.setMat4("projection", projection);
        AnimationShader.setMat4("view", view);
        enemyManager.Render(AnimationShader);
        ourShader.setMat4("model", glm::mat4(1.0f));
        if (!player->IsDead) {
            AnimationShader.use();
            AnimationShader.setMat4("projection", projection);
            AnimationShader.setMat4("view", view);
            player->Draw(AnimationShader, false);
        }

        // UI
        cursorShader.use();
        cursorShader.setMat4("view", view);
        cursorShader.setMat4("projection", projection);

        if (gameState == GAME_PLAYING || gameState == GAME_PAUSED) {
            glm::vec3 flatDir = glm::normalize(glm::vec3(player->ShootDirection.x, 0.0f, player->ShootDirection.z));
            glm::vec3 reticlePos = player->Position + (flatDir * player->currentAimDist);
            myReticle.Draw(cursorShader, reticlePos);
            enemyManager.RenderUI(cursorShader);
            for (auto& mobSpawner : mobSpawners) if (&mobSpawner) mobSpawner.DrawHealthBar(cursorShader);
        }

        if (showDebugHitboxes) {
            debugRenderer.DrawOBB(player->Collider, view, projection);
            for (const auto& enemy : enemyManager.GetEnemies()) {
                debugRenderer.DrawOBB(enemy->Collider, view, projection);
            }
            for (auto& mobSpawner : mobSpawners) {
                if (&mobSpawner && !mobSpawner.IsDestroyed) {
                    debugRenderer.DrawOBB(mobSpawner.Collider, view, projection);
                }
            }
            for (const Arrow& a : player->arrowManager.arrows) {
                if (a.active) debugRenderer.DrawOBB(a.hitbox, view, projection);
            }
            for (const OBB& wall : levelColliders) {
                debugRenderer.DrawOBB(wall, view, projection);
            }
        }

        // Screen UI
        cursorShader.use();
        cursorShader.setMat4("projection", glm::mat4(1.0f));
        cursorShader.setMat4("view", glm::mat4(1.0f));

        if (gameState == GAME_PLAYING) {
            playerHUD.Draw(cursorShader,
                player->CurrentHealth, player->MaxHealth,
                player->power, player->min_power,
                player->max_power,
                player->currentDashCooldown, DASH_COOLDOWN,
                player->currentHealCooldown, HEAL_COOLDOWN);
        }
        else if (gameState == GAME_PAUSED) {
            pauseScreen.Draw(cursorShader, textRenderer, SCR_WIDTH, SCR_HEIGHT);
        }
        else if (gameState == GAME_OVER) {
            gameOverScreen.Draw(cursorShader, textRenderer, SCR_WIDTH, SCR_HEIGHT);
        }
        else if (gameState == GAME_VICTORY) {
            victoryScreen.Draw(cursorShader, textRenderer, SCR_WIDTH, SCR_HEIGHT);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


void processGameplayInput(GLFWwindow* window, Player* player, Camera* camera, float deltaTime)
{
    // Movement
    std::vector<int> direction = { 0,0,0,0 };
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        direction[0] = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        direction[1] = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        direction[2] = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        direction[3] = 1;
    }

    if (!FREECAM) {
        player->ProcessKeyboard(camera->Front, camera->Right, direction, deltaTime);
    }
    else {
        float speedMultiplier = 3.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera->ProcessKeyboard(FORWARD, deltaTime * speedMultiplier);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera->ProcessKeyboard(LEFT, deltaTime * speedMultiplier);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camera->ProcessKeyboard(BACKWARD, deltaTime * speedMultiplier);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera->ProcessKeyboard(RIGHT, deltaTime * speedMultiplier);
        }
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player->Dash();

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        player->Heal();

    // Mouse Aiming
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    float deltaX = (float)mouseX - (SCR_WIDTH / 2.0f);
    float deltaY = (float)mouseY - (SCR_HEIGHT / 2.0f);

    glm::vec3 flatFront = glm::normalize(glm::vec3(camera->Front.x, 0.0f, camera->Front.z));
    glm::vec3 flatRight = glm::normalize(glm::vec3(camera->Right.x, 0.0f, camera->Right.z));

    glm::vec3 aimVector = (flatRight * deltaX * sensitivity) - (flatFront * deltaY * sensitivity);
    float dist = glm::length(aimVector);

    if (dist > 0.001f) {
        float angle = atan2(aimVector.x, aimVector.z);
        player->SetDirectionByMouse(angle, dist);
    }

    // Shooting
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

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (!FREECAM) {
        return;
    }
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

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!FREECAM) {
        camera_distance += static_cast<float>(yoffset) * 0.5f;
        camera_distance = std::clamp(camera_distance, 7.5f, 20.0f);
        return;
    }
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

glm::vec3 GetRayFromMouse(float mouseX, float mouseY, int screenW, int screenH, glm::mat4 view, glm::mat4 projection) {
    float x = (2.0f * mouseX) / screenW - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenH;
    float z = 1.0f;
    glm::vec3 ray_nds = glm::vec3(x, y, z);
    glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0, 1.0);
    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
    glm::vec3 ray_wor = (glm::inverse(view) * ray_eye);
    ray_wor = glm::normalize(ray_wor);
    return ray_wor;
}
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
// We now initialize our new camera. All logic is inside the class.
Camera camera(3.0f, glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, 20.0f); // 3.0f is the starting distance
float camera_distance = 7.5f;
Player* player = nullptr; // Temporary initialization
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
//int BowMode = 0; // 0 = idle, 1 = draw, 2 = shoot
DebugRenderer debugRenderer;
std::vector<OBB> levelColliders;
EnemyManager enemyManager;
MobSpawner* mobSpawner = nullptr;
std::vector<MobSpawner> mobSpawners;

// Reset Game Helper
void ResetGame() {
    if (player) player->Reset();
    if (player) player->arrowManager.arrows.clear();
    enemyManager.Reset();

    // Reset Spawner if it exists
    if (mobSpawner) {
        mobSpawner->CurrentHealth = mobSpawner->MaxHealth;
        mobSpawner->IsDestroyed = false;
        mobSpawner->myEnemies.clear();
        mobSpawner->InitialSpawn(enemyManager); // Spawns the initial 3
    }

    // You can remove these hardcoded spawns if you want the spawner to be the only source
    // enemyManager.Spawn(glm::vec3(5.0f, 0.5f, 5.0f), 0.5f, 100.0f);
    // ...

    gameState = GAME_PLAYING;
}

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
    glEnable(GL_BLEND); // Enable blending globally for text
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Build and compile shaders
    Shader ourShader("1.model_loading.vs", "1.model_loading.fs");
    Shader AnimationShader("anim_model.vs", "anim_model.fs");
    Shader cursorShader("ui.vs", "ui.fs");
    //Shader cursorShader("crosshair.vs", "crosshair.fs");

    // INIT TEXT RENDERER
    std::cout << "Initializing Text Renderer..." << std::endl;
    textRenderer.Init(
        SCR_WIDTH,
        SCR_HEIGHT,
        "text.vs",
        "text.fs",
        FileSystem::getPath("resources/fonts/Antonio-Bold.ttf").c_str()
    );

    // Load models
    Model playerModel(FileSystem::getPath("resources/objects/player/Erika Archer With Bow Arrow.dae"));
    Model arrowModel(FileSystem::getPath("resources/objects/arrow/arrow.obj"));
    Model visualModel(FileSystem::getPath("resources/objects/map/structure.obj"));
	//Model skyboxModel(FileSystem::getPath("resources/objects/skybox/skybox.obj"));
	Model DecorModel(FileSystem::getPath("resources/objects/map/decorations.obj"));
    Model GroundModel(FileSystem::getPath("resources/objects/map/ground.obj"));
    Model TargetModel(FileSystem::getPath("resources/objects/target/target.obj"));
    Model skeletonModel(FileSystem::getPath("resources/objects/enemy/skelly.dae"));
    Model mobSpawnerModel(FileSystem::getPath("resources/objects/map/mobSpawner.obj"));
	Model boarderModel(FileSystem::getPath("resources/objects/map/border.obj"));
	Model boarderHitboxModel(FileSystem::getPath("resources/objects/map/border_hitbox.obj"));

    // Create the Player
    glm::vec3 playerStartPos(0.0f, 10.0f, 0.0f);
    glm::vec3 playerBoxSize(0.2f, 0.35f, 0.4f); // get collision box from AABB function/static for optimization maybe visualize them to see if it is ok or not
    float playerModelScale = 1.0f;
    player = new Player(&playerModel, playerStartPos, playerBoxSize, playerModelScale);
    player->SetGroundModel(&GroundModel);
    player->SetArrowManager(&arrowModel, 0.01f);

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
    levelColliders = GenerateOBBsFromModels(std::vector<Model>{visualModel, boarderHitboxModel}, levelMatrix);

    std::cout << "Generated " << levelColliders.size() << " collision boxes." << std::endl;
    std::cout << "Starting Main Loop..." << std::endl;

    std::cout << "DEBUG: Level loaded with " << visualModel.meshes.size() << " meshes." << std::endl;
    std::cout << "DEBUG: Generated " << levelColliders.size() << " collision boxes." << std::endl;

    if (levelColliders.size() <= 1) {
        std::cout << "WARNING: Your level is only 1 solid object!" << std::endl;
        std::cout << "       The physics will treat it as a solid block you cannot enter." << std::endl;
        std::cout << "       Solution: Export your OBJ with 'Split by Objects' or 'Keep Vertex Order'." << std::endl;
    }

    // Initialize Spawner
    // Positioned at (0, 0, 0) or wherever you want it on the map
	int counter = 0;
    for (int counter = 0; counter < mobSpawnerModel.meshes.size(); counter++)
    {
        MobSpawner* mobSpawner = new MobSpawner(
            &mobSpawnerModel,
            mobSpawnerModel.meshes[counter].vertices[0].Position + glm::vec3(0.0f, -4.0f, 0.0f),
            1.0f,
            counter
        );
		
        mobSpawner->InitialSpawn(enemyManager);
		//mobSpawner->IsDestroyed = true; // Start inactive to debug win screen
        mobSpawners.push_back(*mobSpawner);
    }

    enemyManager.Init(&skeletonModel, &player->GroundModel);
    //enemyManager.Spawn(glm::vec3(5.0f, 10.0f, 5.0f), 0.5f, 100.0f); 
    //enemyManager.Spawn(glm::vec3(-10.0f, 10.0f, 10.0f), 0.5f, 100.0f);

    debugRenderer.Init();

    // render loop
    // -----------
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

            // Check Victory Condition (Spawner Destroyed)
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
                    // Remove deleted enemies from spawner tracking before Manager deletes them
                    mobSpawner.Update(deltaTime, enemyManager);

                    // Collision: Arrows vs Spawner
                    if (!mobSpawner.IsDestroyed) {
                        for (Arrow& arrow : player->arrowManager.arrows) {
                            if (!arrow.active) continue;
                            if (TestOBBOBB(mobSpawner.Collider, arrow.hitbox)) {
                                arrow.active = false;
                                mobSpawner.TakeDamage(35.0f); // Damage spawner
                                std::cout << "Spawner Hit! Health: " << mobSpawner.CurrentHealth << std::endl;
                            }
                        }
                    }
            }

			}
            enemyManager.Update(deltaTime, player->arrowManager, *player);
            glm::vec3 cameraTarget = player->GetBoxCenter();
            cameraTarget.y += 1.0f; // Slightly above the player
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

        player->arrowManager.Render(ourShader);

        // Camera Logic
        
        

        // Render Background
        // Render
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", glm::mat4(1.0f));

        //skyboxModel.Draw(ourShader);
        ourShader.setVec3("camPos", camera.Position);

        // Light Position (e.g., above the player)
        ourShader.setVec3("lightPos", player->Position + glm::vec3(0.0f, 1.5f, 0.0f));

        // Light Color (High intensity because PBR uses physical falloff)
        ourShader.setVec3("lightColor", glm::vec3(20.0f, 10.0f, 1.0f) * 10.0f);
        visualModel.Draw(ourShader);
        GroundModel.Draw(ourShader);
		DecorModel.Draw(ourShader);
		boarderModel.Draw(ourShader);
		//mobSpawnerModel.Draw(ourShader);
        // ourShader.setVec3("lightColor", glm::vec3(20.0f, 5.0f, 20.0f) * 10.0f);
        // for (auto& mobSpawner : mobSpawners) {
        //     ourShader.setVec3("lightPos", mobSpawner.Position + glm::vec3(0.0f, 10.5f, 0.0f));
        //     if (&mobSpawner) mobSpawner.Draw(ourShader);
        // }
        for (auto& mobSpawner : mobSpawners) if (&mobSpawner) mobSpawner.Draw(ourShader); //TODO: make crystal grow purple

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
            glEnable(GL_DEPTH_TEST);
            // Optional: Draw Score or other text here
            // textRenderer.RenderText("Score: 0", 20.0f, SCR_HEIGHT - 40.0f, 0.8f, glm::vec3(1.0f));
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
        playerHUD.Draw(cursorShader,
            player->CurrentHealth, player->MaxHealth,
            player->power, player->min_power,
            player->max_power,
            player->currentDashCooldown, 2.0f,   // Dash: current, max
            player->currentHealCooldown, 30.0f); // Heal: current, max

        // Debug Drawing (Uncomment to see hitboxes)
        if (showDebugHitboxes) {
            for (const Arrow& a : player->arrowManager.arrows) { debugRenderer.DrawOBB(a.hitbox, view, projection); }
            for (Enemy* enemy : enemyManager.GetEnemies()) {
                debugRenderer.DrawOBB(enemy->Collider, view, projection);
            }
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
        //camera->ProcessKeyboard(FORWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        direction[1] = 1;
        //camera->ProcessKeyboard(LEFT, deltaTime);
    } 
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        direction[2] = 1;
        //camera->ProcessKeyboard(BACKWARD, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        direction[3] = 1;
        //camera->ProcessKeyboard(RIGHT, deltaTime);
    }

    if (!FREECAM) {
        player->ProcessKeyboard(camera->Front, camera->Right, direction, deltaTime);
    }
    else {
		float speedMultiplier = 3.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera->ProcessKeyboard(FORWARD, deltaTime* speedMultiplier);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS * speedMultiplier) {
            camera->ProcessKeyboard(LEFT, deltaTime);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS * speedMultiplier) {
            camera->ProcessKeyboard(BACKWARD, deltaTime);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS * speedMultiplier) {
            camera->ProcessKeyboard(RIGHT, deltaTime);
        }
    }
    

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player->Dash();

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        player->Heal();

    // Mouse Aiming
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    // ... [Rest of function remains unchanged] ...

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

// framebuffer_size_callback (NO CHANGES)
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// mouse_callback:
// used for debug map
// ---------------------------------------------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (!FREECAM) {
        return; // Skip if in not freecam mode
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
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// scroll_callback (NO CHANGES)
// ---------------------------------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!FREECAM) {
        camera_distance += static_cast<float>(yoffset) * 0.5f;

        // Automatically limits the value between 7.5 and 20.0
        camera_distance = std::clamp(camera_distance, 7.5f, 20.0f);

        return;
    }
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
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
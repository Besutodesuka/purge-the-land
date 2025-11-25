#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>
#include <iostream>

#include <learnopengl/model.h>
#include <learnopengl/shader_m.h>
#include <go/physic.h>
#include <go/camera_3rd.h> 
#include <go/arrow.h>
#include <go/ui.h> // Include the UI header for the Reticle

// Player physics constants
const float GRAVITY = -9.8f; // Standard Earth gravity for better feel
const float JUMP_FORCE = 4.0f; // Increased to match stronger gravity
const float PLAYER_SPEED = 2.5f;

class Player {
public:
    // Player state
    glm::vec3 Position;
    glm::vec3 Velocity;
    float RotationY;      
    bool IsGrounded;

    // Collision
    AABB CollisionBox;
    glm::vec3 AABBSize;   
    float HeightOffset;   
    OBB Collider;         

    // Model
    Model* PlayerModel;   
    float ModelScale;
    TerrainCollider GroundModel;
    ArrowManager arrowManager;
    
    // UI / Aiming
    Reticle aimReticle; // The visual cursor
    float aimDistance = 2.0f; // How far the cursor is from the player

    // Shooting Mechanics
    float min_power = 0.5f;
    float max_power = 3.0f; // Increased power for better arcs
    float power = min_power;
    glm::vec3 ShootDirection; // Stores the horizontal direction (X, Z)
    float currentAimDist = 2.0f; // Stores the dynamic distance

    // --- CONSTRUCTOR ---
    Player(Model* model, glm::vec3 startPos, glm::vec3 boxSize, float modelScale)
        : PlayerModel(model), Position(startPos), AABBSize(boxSize),
        Velocity(0.0f), RotationY(0.0f), IsGrounded(false), ModelScale(modelScale), arrowManager(nullptr)
    {
        Collider.halfExtents = boxSize * 0.5f;
        HeightOffset = boxSize.y / 2.0f;
        SyncColliders();
        
        // Initialize default shoot direction to Forward
        ShootDirection = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // --- GETTERS & SETTERS ---

    glm::vec3 GetBoxCenter() {
        return Position + glm::vec3(0.0f, HeightOffset, 0.0f);
    }

    void SetGroundModel(Model* model) {
        std::vector<Vertex> myLoadedVertices = model->meshes[0].vertices;
        std::vector<glm::vec3> collisionPositions;
        collisionPositions.reserve(myLoadedVertices.size());
        for (const auto& v : myLoadedVertices) {
            collisionPositions.push_back(v.Position);
        }
        GroundModel.buildFromMesh(collisionPositions);
    }

    void SetArrowManager(Model* model, float scale = 0.1f) {
        arrowManager = ArrowManager(model, scale);
    }

    // --- INPUT PROCESSING ---

    // FIX: Direction is now purely horizontal (X and Z)
    void SetDirectionByMouse(float angle, float distanceInPixels){
        // cos(angle) = X, sin(angle) = Z. 
        // We set Y to 0 because the "Direction" of the mouse is flat on the ground.
        glm::vec3 targetDir;
        ShootDirection.x = sin(angle);
        ShootDirection.y = 0.0f; 
        ShootDirection.z = cos(angle);
        
        ShootDirection = glm::normalize(ShootDirection);

        float pixelToWorldScale = 0.005f;
        currentAimDist = distanceInPixels * pixelToWorldScale;

        // Optional: Clamp minimum distance so it doesn't clip inside the player model
        //if (currentAimDist < 0.5f) currentAimDist = 0.5f;
        if (currentAimDist > 5.0f) currentAimDist = 5.0f;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        // Not used for top-down/isometric aiming
    }

    void ProcessMouse(int click, float deltaTime) {
        if (click == 1) {
            // Charge power
            power += deltaTime * 2.0f; // Charge faster
            power = std::min(max_power, power);
            std::cout << "Charging power: " << power << std::endl;
        }
        else if (click == 0) {
            // FIX: Release arrow at 45 degrees
            
            // 1. Get the flat direction
            glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
            
            // 2. Create a launch vector 45 degrees UP.
            // If Horizontal length is 1, and Vertical length is 1, angle is 45 deg.
            glm::vec3 launchDir = flatDir;
            launchDir.y = 1.0f; 
            
            launchDir = glm::normalize(launchDir);

            // 3. Shoot
            arrowManager.Add_Arrow(
                launchDir * (power * 2.5f), // Multiply power to counteract gravity
                Position + glm::vec3(0.0f, HeightOffset , 0.0f) // Start at player center
            );
            std::cout << "Release arrow at 45 deg" << std::endl;
            power = min_power; // Reset
        }
    }

    void ProcessKeyboard(const glm::vec3& camFront, const glm::vec3& camRight, std::vector<int> direction, float deltaTime) {
        float velocity = PLAYER_SPEED;
        glm::vec3 moveDirection(0.0f);

        // Flatten camera vectors to keep movement on the ground
        glm::vec3 camForwardHorizontal = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        glm::vec3 camRightHorizontal = glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));

        if (direction[0] == 1) moveDirection += camForwardHorizontal;
        if (direction[1] == 1) moveDirection -= camRightHorizontal;
        if (direction[2] == 1) moveDirection -= camForwardHorizontal;
        if (direction[3] == 1) moveDirection += camRightHorizontal;

        if (glm::length(moveDirection) > 0.0f) {
            moveDirection = glm::normalize(moveDirection);
            Velocity.x = moveDirection.x * velocity;
            Velocity.z = moveDirection.z * velocity;
            RotationY = glm::degrees(atan2(moveDirection.x, moveDirection.z));
        }
        else {
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }
    }

    void Jump() {
        if (IsGrounded) {
            Velocity.y = JUMP_FORCE;
            IsGrounded = false;
        }
    }

    // --- PHYSICS & UPDATE ---
    
    void Update(float deltaTime, const std::vector<OBB>& levelColliders) {
        ApplyGravity(deltaTime);

        // Y-Axis
        Position.y += Velocity.y * deltaTime;
        SyncColliders(); 
        IsGrounded = false;

        CheckTerrainCollision();

        // X/Z-Axis
        Position.x += Velocity.x * deltaTime;
        Position.z += Velocity.z * deltaTime;
        SyncColliders(); 

        for (const OBB& wall : levelColliders) {
            if (TestOBBOBB(Collider, wall)) {
                Position.x -= Velocity.x * deltaTime;
                Position.z -= Velocity.z * deltaTime;
                SyncColliders();
                break;
            }
        }
        
        // Update Arrows
        arrowManager.Update(deltaTime, levelColliders);
    }

    // --- RENDER ---

    void Draw(Shader& shader, bool flip = true) {
        // 1. Draw Player
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        float offset = flip ? 180.0f : 0.0f;
        model = glm::rotate(model, glm::radians(RotationY + offset), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(ModelScale));
        shader.setMat4("model", model);
        PlayerModel->Draw(shader);

        // 2. Draw Arrows
        arrowManager.Render(shader);

        // 3. Draw Reticle (Cursor)
        // Calculate position: Start at player feet -> Move in Direction -> Raise slightly above ground
        glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
        glm::vec3 reticlePos = Position + (flatDir * currentAimDist);
        
        aimReticle.Draw(shader, reticlePos);
    }

private:
    void ApplyGravity(float deltaTime) {
        if (!IsGrounded) {
            Velocity.y += GRAVITY * deltaTime;
        }
    }

    void SyncColliders() {
        CollisionBox.Update(GetBoxCenter(), AABBSize);
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = glm::rotate(model, glm::radians(RotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::translate(model, glm::vec3(0.0f, HeightOffset, 0.0f));

        Collider.center = glm::vec3(model[3]);
        glm::mat3 R = glm::mat3(model);
        Collider.axes[0] = glm::normalize(R[0]); 
        Collider.axes[1] = glm::normalize(R[1]); 
        Collider.axes[2] = glm::normalize(R[2]); 
    }

    void CheckTerrainCollision() {
        if (GroundModel.isBuilt()) {
            float groundHeight = GroundModel.getExactHeightAt(Position);
            if (Position.y < groundHeight) {
                Position.y = groundHeight;
                Velocity.y = 0.0f;
                IsGrounded = true;
                SyncColliders(); 
            }
        }
    }
};

#endif
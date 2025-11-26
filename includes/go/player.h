#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

// Includes for Animation
#include <learnopengl/shader_m.h>
#include <learnopengl/animator.h>        
#include <learnopengl/model_animation.h> 
#include <learnopengl/filesystem.h>      

#include <go/physic.h>
#include <go/camera_3rd.h> 
#include <go/arrow.h>
#include <go/ui.h>

// Player physics constants
const float GRAVITY = -9.8f;
const float JUMP_FORCE = 4.0f;
const float PLAYER_SPEED = 2.5f;

class Player {
public:
    // Animation State Enum
    enum AnimState {
        IDLE = 1,
        // Walking animations blend states
        IDLE_WALK_FORWARD,
        WALK_FORWARD_IDLE,
        IDLE_WALK_BACKWARD,
        WALK_BACKWARD_IDLE,
        IDLE_WALK_LEFT,
        WALK_LEFT_IDLE,
        IDLE_WALK_RIGHT,
        WALK_RIGHT_IDLE,

        WALK_FORWARD,
        WALK_BACKWARD,
        WALK_LEFT,
        WALK_RIGHT,

        // New State for Shooting
        SHOOTING_RECOIL
    };

    // Player state
    glm::vec3 Position;
    glm::vec3 Velocity;
    float RotationY;
    bool IsGrounded;
    bool isAiming;        // Track if mouse button is held
    bool isShooting;      // Track if recoil is playing

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
    Reticle aimReticle;
    float aimDistance = 2.0f;

    float recoilTimer;

    // Shooting Mechanics
    float min_power = 0.5f;
    float max_power = 3.0f;
    float power = min_power;
    glm::vec3 ShootDirection;
    float currentAimDist = 2.0f;

    // --- ANIMATION MEMBERS ---
    Animator* animator;

    // Standard Animations
    Animation* idleAnimation;
    Animation* walkForwardAnimation;
    Animation* walkBackwardAnimation;
    Animation* walkLeftAnimation;
    Animation* walkRightAnimation;

    // Aiming Animations
    Animation* aimIdleAnimation;
    Animation* aimRecoilAnimation;
    Animation* aimWalkForwardAnimation;
    Animation* aimWalkBackwardAnimation;
    Animation* aimWalkLeftAnimation;
    Animation* aimWalkRightAnimation;

    AnimState charState;
    float blendAmount;
    float blendRate;

    // Animation Input Flags
    bool requestWalkForward;
    bool requestWalkBackward;
    bool requestWalkLeft;
    bool requestWalkRight;

    // --- CONSTRUCTOR ---
    Player(Model* model, glm::vec3 startPos, glm::vec3 boxSize, float modelScale)
        : PlayerModel(model), Position(startPos), AABBSize(boxSize),
        Velocity(0.0f), RotationY(0.0f), IsGrounded(false), ModelScale(modelScale), arrowManager(nullptr)
    {
        // Safety check
        if (model == nullptr) {
            std::cerr << "CRITICAL ERROR: PlayerModel is NULL!" << std::endl;
            throw std::runtime_error("PlayerModel cannot be null");
        }

        Collider.halfExtents = boxSize * 0.5f;
        HeightOffset = boxSize.y / 2.0f;
        SyncColliders();

        ShootDirection = glm::vec3(0.0f, 0.0f, 1.0f);
        isAiming = false;
        isShooting = false;

        recoilTimer = 0.0f;

        // Initialize Animation Data
        InitAnimations();
    }

    // Destructor
    ~Player() {
        delete animator;
        delete idleAnimation;
        delete walkForwardAnimation;
        delete walkBackwardAnimation;
        delete walkLeftAnimation;
        delete walkRightAnimation;

        // Delete new animations
        delete aimIdleAnimation;
        delete aimRecoilAnimation;
        delete aimWalkForwardAnimation;
        delete aimWalkBackwardAnimation;
        delete aimWalkLeftAnimation;
        delete aimWalkRightAnimation;
    }

    // --- INITIALIZATION ---
    void InitAnimations() {
        // Standard Set (No Aim)
        idleAnimation = new Animation(FileSystem::getPath("resources/objects/player/Idle.dae"), PlayerModel);
        walkForwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Forward.dae"), PlayerModel);
        walkBackwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Back.dae"), PlayerModel);
        walkLeftAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Left.dae"), PlayerModel);
        walkRightAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Right.dae"), PlayerModel);

        // Aiming Set
        // Note: Using "Standing Aim Idle 01" for both for smoother transition, or you could load a separate non-aim idle
        aimIdleAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Idle 01.dae"), PlayerModel);
        aimRecoilAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Recoil.dae"), PlayerModel);
        aimWalkForwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Forward.dae"), PlayerModel);
        aimWalkBackwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Back.dae"), PlayerModel);
        aimWalkLeftAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Left.dae"), PlayerModel);
        aimWalkRightAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Right.dae"), PlayerModel);

        animator = new Animator(idleAnimation);

        charState = IDLE;
        blendAmount = 0.0f;
        blendRate = 4.0f; // Faster blending

        requestWalkForward = false;
        requestWalkBackward = false;
        requestWalkLeft = false;
        requestWalkRight = false;
    }

    // --- GETTERS & SETTERS ---

    glm::vec3 GetBoxCenter() {
        return Position + glm::vec3(0.0f, HeightOffset, 0.0f);
    }

    void SetGroundModel(Model* model) {
        if (!model) return;
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

    void SetDirectionByMouse(float angle, float distanceInPixels) {
        glm::vec3 targetDir;
        ShootDirection.x = sin(angle);
        ShootDirection.y = 0.0f;
        ShootDirection.z = cos(angle);

        ShootDirection = glm::normalize(ShootDirection);

        // If aiming, character rotates to face cursor
        if (isAiming) {
            RotationY = glm::degrees(atan2(ShootDirection.x, ShootDirection.z));
        }

        float pixelToWorldScale = 0.005f;
        currentAimDist = distanceInPixels * pixelToWorldScale;
        if (currentAimDist > 5.0f) currentAimDist = 5.0f;
    }

    void ProcessMouse(int click, float deltaTime) {
        // click: 1 = pressed (aim), 0 = released (fire/stop aim)

        if (click == 1) {
            // Rule: Left Click Input -> Aim
            isAiming = true;
            power += deltaTime * 2.0f;
            power = std::min(max_power, power);
        }
        else if (click == 0 && isAiming) {
            // Rule: Release -> Fire and Stop Aim
            FireArrow();

            // Trigger Recoil
            charState = SHOOTING_RECOIL;
            recoilTimer = 0.0f;
            animator->PlayAnimation(aimRecoilAnimation, NULL, 0.0f, 0.0f, 0.0f);

            // Reset logic
            power = min_power;
            isAiming = false;
        }
    }

    void FireArrow() {
        glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
        glm::vec3 launchDir = flatDir;

        // 15 Degree launch angle (tan(15) ~ 0.268)
        launchDir.y = 0.268f;
        launchDir = glm::normalize(launchDir);

        // Spawn at 75% height
        float spawnHeight = Position.y + (AABBSize.y * 0.75f);

        arrowManager.Add_Arrow(
            launchDir * (power * 5.0f),
            glm::vec3(Position.x, spawnHeight, Position.z)
        );
    }

    void ProcessKeyboard(const glm::vec3& camFront, const glm::vec3& camRight, std::vector<int> direction, float deltaTime) {
        requestWalkForward = false;
        requestWalkBackward = false;
        requestWalkLeft = false;
        requestWalkRight = false;

        float velocity = isAiming ? PLAYER_SPEED * 0.6f : PLAYER_SPEED; // Slower when aiming
        glm::vec3 moveDirection(0.0f);

        glm::vec3 camForwardHorizontal = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        glm::vec3 camRightHorizontal = glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));

        if (direction[0] == 1) {
            moveDirection += camForwardHorizontal;
            requestWalkForward = true;
        }
        if (direction[1] == 1) {
            moveDirection -= camRightHorizontal;
            requestWalkLeft = true;
        }
        if (direction[2] == 1) {
            moveDirection -= camForwardHorizontal;
            requestWalkBackward = true;
        }
        if (direction[3] == 1) {
            moveDirection += camRightHorizontal;
            requestWalkRight = true;
        }

        if (glm::length(moveDirection) > 0.0f) {
            moveDirection = glm::normalize(moveDirection);
            Velocity.x = moveDirection.x * velocity;
            Velocity.z = moveDirection.z * velocity;

            // Rule: If NOT aiming, rotate to movement direction.
            // If aiming, SetDirectionByMouse handles rotation.
            if (!isAiming) {
                RotationY = glm::degrees(atan2(moveDirection.x, moveDirection.z));
            }
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

    void Update(float deltaTime, const std::vector<OBB>* levelColliders = nullptr) {
        UpdatePhysics(deltaTime, levelColliders);

        // Handle optional colliders for arrows
        static std::vector<OBB> empty;
        arrowManager.Update(deltaTime, levelColliders ? *levelColliders : empty);

        UpdateAnimationLogic(deltaTime);
    }

    void UpdatePhysics(float deltaTime, const std::vector<OBB>* levelColliders) {
        ApplyGravity(deltaTime);

        // 1. Move Y first (Gravity/Jump)
        Position.y += Velocity.y * deltaTime;

        // 2. Move X/Z (Horizontal) immediately
        Position.x += Velocity.x * deltaTime;
        Position.z += Velocity.z * deltaTime;

        // Sync colliders at new potential position
        SyncColliders();

        // 3. Wall Collision (Horizontal Revert)
        if (levelColliders) {
            for (const OBB& wall : *levelColliders) {
                if (TestOBBOBB(Collider, wall)) {
                    Position.x -= Velocity.x * deltaTime;
                    Position.z -= Velocity.z * deltaTime;
                    SyncColliders();
                    break;
                }
            }
        }

        // 4. Terrain Collision (Snap Y)
        // CRITICAL FIX: This must happen LAST.
        // It calculates height at the NEW X/Z position and snaps Y up immediately.
        IsGrounded = false;
        CheckTerrainCollision();
    }

    // --- ANIMATION STATE MACHINE ---
    // UPDATED LOGIC TO FOLLOW RULES:
    // 1. No Input -> IDLE (No Aim)
    // 2. Left Click Only -> AIM IDLE
    // 3. Walk Only -> WALK NORMAL
    // 4. Walk + Left Click -> AIM WALK
    void UpdateAnimationLogic(float deltaTime) {
        animator->UpdateAnimation(deltaTime);

        // 1. Determine the "Target" animations based on input state
        // This ensures that if 'isAiming' is true, we ALWAYS use the Aim variants.
        Animation* targetIdle = isAiming ? aimIdleAnimation : idleAnimation;
        Animation* targetFwd = isAiming ? aimWalkForwardAnimation : walkForwardAnimation;
        Animation* targetBack = isAiming ? aimWalkBackwardAnimation : walkBackwardAnimation;
        Animation* targetLeft = isAiming ? aimWalkLeftAnimation : walkLeftAnimation;
        Animation* targetRight = isAiming ? aimWalkRightAnimation : walkRightAnimation;

        switch (charState) {
        case SHOOTING_RECOIL:
            // --- NEW LOGIC START ---
            recoilTimer += deltaTime;

            // Calculate duration in seconds explicitly
            {
                float tps = aimRecoilAnimation->GetTicksPerSecond();
                if (tps == 0.0f) tps = 25.0f; // Safety default
                float durationInSeconds = aimRecoilAnimation->GetDuration() / tps;

                if (recoilTimer >= durationInSeconds) {
                    charState = IDLE;
                    animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
                }
            }
            // --- NEW LOGIC END ---
            break;

        case IDLE:
            // Rule: If Aim status changes while Idle, swap animation immediately.
            // (e.g., Idle -> Aim Idle)
            if (animator->m_CurrentAnimation != targetIdle && animator->m_CurrentAnimation != aimRecoilAnimation) {
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }

            // Transitions to Walking (Normal or Aim based on target pointers)
            if (requestWalkForward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetIdle, targetFwd, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_FORWARD;
            }
            else if (requestWalkBackward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetIdle, targetBack, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_BACKWARD;
            }
            else if (requestWalkLeft) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetIdle, targetLeft, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_LEFT;
            }
            else if (requestWalkRight) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetIdle, targetRight, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_RIGHT;
            }
            break;

        case IDLE_WALK_FORWARD:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetFwd, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_FORWARD;
            }
            else {
                // Continue blend. Note we use targetIdle/targetFwd. 
                // If Aim state changes mid-blend, this swaps the animation instantly.
                animator->PlayAnimation(targetIdle, targetFwd, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkForward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetFwd, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_FORWARD_IDLE;
            }
            break;

        case WALK_FORWARD:
            // Rule: If Walk -> Aim Walk (or vice versa), swap animation immediately.
            if (animator->m_CurrentAnimation != targetFwd) {
                float oldTime = animator->m_CurrentTime; // Keep sync
                animator->PlayAnimation(targetFwd, NULL, oldTime, 0.0f, 0.0f);
            }

            if (!requestWalkForward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetFwd, targetIdle, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_FORWARD_IDLE;
            }
            break;

        case WALK_FORWARD_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(targetFwd, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkForward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetIdle, targetFwd, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_FORWARD;
            }
            break;

            // --- BACKWARD ---
        case IDLE_WALK_BACKWARD:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetBack, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_BACKWARD;
            }
            else {
                animator->PlayAnimation(targetIdle, targetBack, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkBackward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetBack, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_BACKWARD_IDLE;
            }
            break;

        case WALK_BACKWARD:
            if (animator->m_CurrentAnimation != targetBack) {
                float oldTime = animator->m_CurrentTime;
                animator->PlayAnimation(targetBack, NULL, oldTime, 0.0f, 0.0f);
            }
            if (!requestWalkBackward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetBack, targetIdle, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_BACKWARD_IDLE;
            }
            break;

        case WALK_BACKWARD_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(targetBack, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkBackward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetIdle, targetBack, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_BACKWARD;
            }
            break;

            // --- LEFT ---
        case IDLE_WALK_LEFT:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetLeft, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_LEFT;
            }
            else {
                animator->PlayAnimation(targetIdle, targetLeft, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkLeft) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetLeft, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_LEFT_IDLE;
            }
            break;

        case WALK_LEFT:
            if (animator->m_CurrentAnimation != targetLeft) {
                float oldTime = animator->m_CurrentTime;
                animator->PlayAnimation(targetLeft, NULL, oldTime, 0.0f, 0.0f);
            }
            if (!requestWalkLeft) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetLeft, targetIdle, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_LEFT_IDLE;
            }
            break;

        case WALK_LEFT_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(targetLeft, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkLeft) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetIdle, targetLeft, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_LEFT;
            }
            break;

            // --- RIGHT ---
        case IDLE_WALK_RIGHT:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetRight, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_RIGHT;
            }
            else {
                animator->PlayAnimation(targetIdle, targetRight, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkRight) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetRight, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_RIGHT_IDLE;
            }
            break;

        case WALK_RIGHT:
            if (animator->m_CurrentAnimation != targetRight) {
                float oldTime = animator->m_CurrentTime;
                animator->PlayAnimation(targetRight, NULL, oldTime, 0.0f, 0.0f);
            }
            if (!requestWalkRight) {
                blendAmount = 0.0f;
                animator->PlayAnimation(targetRight, targetIdle, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_RIGHT_IDLE;
            }
            break;

        case WALK_RIGHT_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(targetRight, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkRight) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetIdle, targetRight, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_RIGHT;
            }
            break;
        }
    }

    // --- RENDER ---

    void Draw(Shader& shader, bool flip = true) {
        auto transforms = animator->GetFinalBoneMatrices();
        for (int i = 0; i < transforms.size(); ++i) {
            shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        float offset = flip ? 180.0f : 0.0f;
        model = glm::rotate(model, glm::radians(RotationY + offset), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(ModelScale));
        shader.setMat4("model", model);
        PlayerModel->Draw(shader);

        glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
        glm::vec3 reticlePos = Position + (flatDir * currentAimDist);
        aimReticle.Draw(shader, reticlePos);
    }

    // --- DISABLE COPYING ---
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

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
            // --- CHANGE: Added safety check to avoid falling into void ---
            if (groundHeight > -500.0f && Position.y < groundHeight) {
                Position.y = groundHeight;
                Velocity.y = 0.0f;
                IsGrounded = true;
                SyncColliders();
            }
        }
    }
};

#endif
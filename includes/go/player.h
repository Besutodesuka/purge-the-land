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
// const float JUMP_FORCE = 4.0f; // Removed
const float PLAYER_SPEED = 2.5f;

// --- DASH CONSTANTS ---
const float DASH_SPEED = 10.0f;       
const float DASH_DURATION = 0.25f;    
const float DASH_COOLDOWN = 2.0f;     

// --- HEAL CONSTANTS ---
const float HEAL_COOLDOWN = 30.0f;
const float HEAL_AMOUNT = 30.0f;

class Player {
public:
    // Animation State Enum
    enum AnimState {
        IDLE = 1,
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
        SHOOTING_RECOIL
    };

    // Player state
    glm::vec3 Position;
    glm::vec3 Velocity;
    float RotationY;
    bool IsGrounded;
    bool isAiming;
    bool isShooting;

    // --- SKILL VARIABLES ---
    float currentDashDuration; 
    float currentDashCooldown; 
    float currentHealCooldown;

    // Health Stats
    float MaxHealth;
    float CurrentHealth;
    bool IsDead;

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
        if (model == nullptr) {
            std::cerr << "CRITICAL ERROR: PlayerModel is NULL!" << std::endl;
            throw std::runtime_error("PlayerModel cannot be null");
        }

        // Initialize Health
        MaxHealth = 100.0f;
        CurrentHealth = 100.0f;
        IsDead = false;

        // Initialize Cooldowns
        currentDashDuration = 0.0f;
        currentDashCooldown = 0.0f;
        currentHealCooldown = 0.0f;

        Collider.halfExtents = boxSize * 0.5f;
        HeightOffset = boxSize.y / 2.0f;
        SyncColliders();

        ShootDirection = glm::vec3(0.0f, 0.0f, 1.0f);
        isAiming = false;
        isShooting = false;
        recoilTimer = 0.0f;

        this->startPosition = startPos;
        InitAnimations();
    }

     void Reset() {
        Position = startPosition;
        CurrentHealth = MaxHealth;
        IsDead = false;
        isAiming = false;
        isShooting = false;
        Velocity = glm::vec3(0.0f);
        IsGrounded = false;
        power = min_power;

        currentDashDuration = 0.0f;
        currentDashCooldown = 0.0f;
        currentHealCooldown = 0.0f;
        
        // Reset Animation
        charState = IDLE;
        animator->PlayAnimation(idleAnimation, NULL, 0.0f, 0.0f, 0.0f);
        
        // Sync Physics
        SyncColliders();
    }

    // Destructor
    ~Player() {
        delete animator;
        delete idleAnimation;
        delete walkForwardAnimation;
        delete walkBackwardAnimation;
        delete walkLeftAnimation;
        delete walkRightAnimation;
        delete aimIdleAnimation;
        delete aimRecoilAnimation;
        delete aimWalkForwardAnimation;
        delete aimWalkBackwardAnimation;
        delete aimWalkLeftAnimation;
        delete aimWalkRightAnimation;
    }

    // --- DAMAGE LOGIC ---
    void TakeDamage(float amount) {
        if (IsDead) return;

        CurrentHealth -= amount;
        if (CurrentHealth <= 0.0f) {
            CurrentHealth = 0.0f;
            IsDead = true;
            std::cout << "Player has died!" << std::endl;
        }
    }

    void Heal() {
        if (IsDead) return;
        if (currentHealCooldown <= 0.0f && CurrentHealth < MaxHealth) {
            CurrentHealth += HEAL_AMOUNT;
            if (CurrentHealth > MaxHealth) CurrentHealth = MaxHealth;
            currentHealCooldown = HEAL_COOLDOWN;
            std::cout << "Player Healed! HP: " << CurrentHealth << std::endl;
        }
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
        aimIdleAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Idle 01.dae"), PlayerModel);
        aimRecoilAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Recoil.dae"), PlayerModel);
        aimWalkForwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Forward.dae"), PlayerModel);
        aimWalkBackwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Back.dae"), PlayerModel);
        aimWalkLeftAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Left.dae"), PlayerModel);
        aimWalkRightAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Walk Right.dae"), PlayerModel);

        animator = new Animator(idleAnimation);

        charState = IDLE;
        blendAmount = 0.0f;
        blendRate = 3.0f; 

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

        if (isAiming) {
            RotationY = glm::degrees(atan2(ShootDirection.x, ShootDirection.z));
        }

        float pixelToWorldScale = 0.005f;
        currentAimDist = distanceInPixels * pixelToWorldScale;
        if (currentAimDist > 5.0f) currentAimDist = 5.0f;
    }

    void ProcessMouse(int click, float deltaTime) {
        if (click == 1) {
            isAiming = true;
            power += deltaTime * 2.0f;
            power = std::min(max_power, power);
        }
        else if (click == 0 && isAiming) {
            FireArrow();
            charState = SHOOTING_RECOIL;
            recoilTimer = 0.0f;
            animator->PlayAnimation(aimRecoilAnimation, NULL, 0.0f, 0.0f, 0.0f);
            power = min_power;
            isAiming = false;
        }
    }

    void FireArrow() {
        glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
        glm::vec3 launchDir = flatDir;
        launchDir.y = 0.268f;
        launchDir = glm::normalize(launchDir);
        float spawnHeight = Position.y + (AABBSize.y * 0.75f);

        arrowManager.Add_Arrow(
            launchDir * (power * 5.0f),
            glm::vec3(Position.x, spawnHeight, Position.z)
        );
    }

    void ProcessKeyboard(const glm::vec3& camFront, const glm::vec3& camRight, std::vector<int> direction, float deltaTime) {
        if (currentDashDuration > 0.0f) return;
        requestWalkForward = false; requestWalkBackward = false; requestWalkLeft = false; requestWalkRight = false;

        float velocity = isAiming ? PLAYER_SPEED * 0.6f : PLAYER_SPEED;
        glm::vec3 moveDirection(0.0f);
        glm::vec3 camForwardHorizontal = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        glm::vec3 camRightHorizontal = glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));
        if (direction[0] == 1) { moveDirection += camForwardHorizontal; requestWalkForward = true; }
        if (direction[1] == 1) { moveDirection -= camRightHorizontal; requestWalkLeft = true; }
        if (direction[2] == 1) { moveDirection -= camForwardHorizontal; requestWalkBackward = true; }
        if (direction[3] == 1) { moveDirection += camRightHorizontal; requestWalkRight = true; }
        if (glm::length(moveDirection) > 0.0f) {
            moveDirection = glm::normalize(moveDirection);
            Velocity.x = moveDirection.x * velocity; Velocity.z = moveDirection.z * velocity;
            if (!isAiming) RotationY = glm::degrees(atan2(moveDirection.x, moveDirection.z));
        } else { Velocity.x = 0.0f; Velocity.z = 0.0f; }
    }

     void Dash() {
        if (currentDashCooldown <= 0.0f && !isAiming) {
            float angleRad = glm::radians(RotationY);
            glm::vec3 dashDir;
            dashDir.x = sin(angleRad); dashDir.z = cos(angleRad); dashDir.y = 0.0f;
            dashDir = glm::normalize(dashDir);
            Velocity.x = dashDir.x * DASH_SPEED; Velocity.z = dashDir.z * DASH_SPEED;
            currentDashDuration = DASH_DURATION;
            currentDashCooldown = DASH_COOLDOWN;
        }
    }

    // --- PHYSICS & UPDATE ---

    void Update(float deltaTime, const std::vector<OBB>* levelColliders = nullptr) {
        // Update Dash Timers
        if (currentDashDuration > 0.0f) currentDashDuration -= deltaTime;
        if (currentDashCooldown > 0.0f) currentDashCooldown -= deltaTime;
        if (currentHealCooldown > 0.0f) currentHealCooldown -= deltaTime;

        UpdatePhysics(deltaTime, levelColliders);
        static std::vector<OBB> empty;
        arrowManager.Update(deltaTime, levelColliders ? *levelColliders : empty);
        UpdateAnimationLogic(deltaTime);
    }

    void UpdatePhysics(float deltaTime, const std::vector<OBB>* levelColliders) {
        ApplyGravity(deltaTime);
        Position.y += Velocity.y * deltaTime;
        Position.x += Velocity.x * deltaTime;
        Position.z += Velocity.z * deltaTime;
        SyncColliders();

        if (levelColliders) {
            for (const OBB& wall : *levelColliders) {
                if (TestOBBOBB(Collider, wall)) {
                    // Simple collision resolution
                    Position.x -= Velocity.x * deltaTime;
                    Position.z -= Velocity.z * deltaTime;
                    
                    // Stop dash on wall hit
                    if (currentDashDuration > 0.0f) {
                        currentDashDuration = 0.0f;
                        Velocity.x = 0;
                        Velocity.z = 0;
                    }
                    
                    SyncColliders();
                    break;
                }
            }
        }
        IsGrounded = false;
        CheckTerrainCollision();
    }

    // --- ANIMATION STATE MACHINE (RESTORED) ---
    void UpdateAnimationLogic(float deltaTime) {
        animator->UpdateAnimation(deltaTime);

        Animation* targetIdle = isAiming ? aimIdleAnimation : idleAnimation;
        Animation* targetFwd = isAiming ? aimWalkForwardAnimation : walkForwardAnimation;
        Animation* targetBack = isAiming ? aimWalkBackwardAnimation : walkBackwardAnimation;
        Animation* targetLeft = isAiming ? aimWalkLeftAnimation : walkLeftAnimation;
        Animation* targetRight = isAiming ? aimWalkRightAnimation : walkRightAnimation;

        switch (charState) {
        case SHOOTING_RECOIL:
            recoilTimer += deltaTime;
            {
                float tps = aimRecoilAnimation->GetTicksPerSecond();
                if (tps == 0.0f) tps = 25.0f;
                float durationInSeconds = aimRecoilAnimation->GetDuration() / tps;
                if (recoilTimer >= durationInSeconds) {
                    charState = IDLE;
                    animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
                }
            }
            break;

        case IDLE:
            if (animator->m_CurrentAnimation != targetIdle && animator->m_CurrentAnimation != aimRecoilAnimation) {
                animator->PlayAnimation(targetIdle, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
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
                animator->PlayAnimation(targetIdle, targetFwd, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkForward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(targetFwd, targetIdle, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_FORWARD_IDLE;
            }
            break;

        case WALK_FORWARD:
            if (animator->m_CurrentAnimation != targetFwd) {
                float oldTime = animator->m_CurrentTime;
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
    glm::vec3 startPosition;
    
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
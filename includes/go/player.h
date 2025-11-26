#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

// Includes for Animation
// #include <learnopengl/model.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/animator.h>        // ADDED
#include <learnopengl/model_animation.h> // ADDED
#include <learnopengl/filesystem.h>      // ADDED for paths

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
        WALK_RIGHT
    };

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
    Reticle aimReticle;
    float aimDistance = 2.0f;

    // Shooting Mechanics
    float min_power = 0.5f;
    float max_power = 3.0f;
    float power = min_power;
    glm::vec3 ShootDirection;
    float currentAimDist = 2.0f;

    // --- ANIMATION MEMBERS ---
    Animator* animator;
    Animation* idleAnimation;
    Animation* walkForwardAnimation;
    Animation* walkBackwardAnimation;
    Animation* walkLeftAnimation;
    Animation* walkRightAnimation;

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
        Collider.halfExtents = boxSize * 0.5f;
        HeightOffset = boxSize.y / 2.0f;
        SyncColliders();

        ShootDirection = glm::vec3(0.0f, 0.0f, 1.0f);

        // Initialize Animation Data
        if (model == nullptr) {
            std::cerr << "Player initialize with PlayerModel is NULL. Animation loading will crash. skip all animation loading process" << std::endl;
        }
        else {
            InitAnimations();
        }
    }

    // Destructor to clean up allocated animations
    ~Player() {
        delete animator;
        delete idleAnimation;
        delete walkForwardAnimation;
        delete walkBackwardAnimation;
        delete walkLeftAnimation;
        delete walkRightAnimation;
    }

    // --- INITIALIZATION ---
    void InitAnimations() {
        // Load animations using the paths from your skeletal_animation.cpp
        // Ensure these paths are correct relative to your executable
        idleAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Aim Idle 01.dae"), PlayerModel);
        walkForwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Forward.dae"), PlayerModel);
        walkBackwardAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Back.dae"), PlayerModel);
        walkLeftAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Left.dae"), PlayerModel);
        walkRightAnimation = new Animation(FileSystem::getPath("resources/objects/player/Standing Walk Right.dae"), PlayerModel);

        animator = new Animator(idleAnimation);

        charState = IDLE;
        blendAmount = 0.0f;
        blendRate = 2.0f; // Speed of blending

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

        float pixelToWorldScale = 0.005f;
        currentAimDist = distanceInPixels * pixelToWorldScale;

        if (currentAimDist > 5.0f) currentAimDist = 5.0f;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        // Not used
    }

    void ProcessMouse(int click, float deltaTime) {
        if (click == 1) {
            power += deltaTime * 2.0f;
            power = std::min(max_power, power);
        }
        else if (click == 0) {
            glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
            glm::vec3 launchDir = flatDir;
            launchDir.y = 1.0f;
            launchDir = glm::normalize(launchDir);

            arrowManager.Add_Arrow(
                launchDir * (power * 2.5f),
                Position + glm::vec3(0.0f, HeightOffset, 0.0f)
            );
            power = min_power;
        }
    }

    void ProcessKeyboard(const glm::vec3& camFront, const glm::vec3& camRight, std::vector<int> direction, float deltaTime) {
        // 1. Reset Request Flags
        requestWalkForward = false;
        requestWalkBackward = false;
        requestWalkLeft = false;
        requestWalkRight = false;

        // 2. Map Input Vector to Physics and Animation Flags
        // Assuming direction mapping: [0]=Forward, [1]=Left, [2]=Backward, [3]=Right
        float velocity = PLAYER_SPEED;
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

        // Physics Movement
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
        // 1. Update Physics
        UpdatePhysics(deltaTime, levelColliders);

        // 2. Update Arrows
        arrowManager.Update(deltaTime, levelColliders);

        // 3. Update Animation State Machine
        UpdateAnimationLogic(deltaTime);
    }

    void UpdatePhysics(float deltaTime, const std::vector<OBB>& levelColliders) {
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
    }

    // The Logic transferred from skeletal_animation.cpp
    void UpdateAnimationLogic(float deltaTime) {
        // Update the animator *before* processing input that changes state
        animator->UpdateAnimation(deltaTime);

        switch (charState) {
        case IDLE:
            if (animator->m_CurrentAnimation != idleAnimation || animator->m_CurrentAnimation2 != NULL) {
                animator->PlayAnimation(idleAnimation, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
            if (requestWalkForward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(idleAnimation, walkForwardAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_FORWARD;
            }
            else if (requestWalkBackward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(idleAnimation, walkBackwardAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_BACKWARD;
            }
            else if (requestWalkLeft) {
                blendAmount = 0.0f;
                animator->PlayAnimation(idleAnimation, walkLeftAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_LEFT;
            }
            else if (requestWalkRight) {
                blendAmount = 0.0f;
                animator->PlayAnimation(idleAnimation, walkRightAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_WALK_RIGHT;
            }
            break;

        case IDLE_WALK_FORWARD:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(walkForwardAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_FORWARD;
            }
            else {
                animator->PlayAnimation(idleAnimation, walkForwardAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkForward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(walkForwardAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_FORWARD_IDLE;
            }
            break;

        case WALK_FORWARD:
            if (animator->m_CurrentAnimation != walkForwardAnimation || animator->m_CurrentAnimation2 != NULL) {
                animator->PlayAnimation(walkForwardAnimation, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
            if (!requestWalkForward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(walkForwardAnimation, idleAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_FORWARD_IDLE;
            }
            break;

        case WALK_FORWARD_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(idleAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(walkForwardAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkForward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(idleAnimation, walkForwardAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_FORWARD;
            }
            break;

            // --- BACKWARD ---
        case IDLE_WALK_BACKWARD:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(walkBackwardAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_BACKWARD;
            }
            else {
                animator->PlayAnimation(idleAnimation, walkBackwardAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkBackward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(walkBackwardAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_BACKWARD_IDLE;
            }
            break;

        case WALK_BACKWARD:
            if (animator->m_CurrentAnimation != walkBackwardAnimation || animator->m_CurrentAnimation2 != NULL) {
                animator->PlayAnimation(walkBackwardAnimation, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
            if (!requestWalkBackward) {
                blendAmount = 0.0f;
                animator->PlayAnimation(walkBackwardAnimation, idleAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_BACKWARD_IDLE;
            }
            break;

        case WALK_BACKWARD_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(idleAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(walkBackwardAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkBackward) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(idleAnimation, walkBackwardAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_BACKWARD;
            }
            break;

            // --- LEFT ---
        case IDLE_WALK_LEFT:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(walkLeftAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_LEFT;
            }
            else {
                animator->PlayAnimation(idleAnimation, walkLeftAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkLeft) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(walkLeftAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_LEFT_IDLE;
            }
            break;

        case WALK_LEFT:
            if (animator->m_CurrentAnimation != walkLeftAnimation || animator->m_CurrentAnimation2 != NULL) {
                animator->PlayAnimation(walkLeftAnimation, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
            if (!requestWalkLeft) {
                blendAmount = 0.0f;
                animator->PlayAnimation(walkLeftAnimation, idleAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_LEFT_IDLE;
            }
            break;

        case WALK_LEFT_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(idleAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(walkLeftAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkLeft) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(idleAnimation, walkLeftAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_LEFT;
            }
            break;

            // --- RIGHT ---
        case IDLE_WALK_RIGHT:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(walkRightAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = WALK_RIGHT;
            }
            else {
                animator->PlayAnimation(idleAnimation, walkRightAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (!requestWalkRight) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(walkRightAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = WALK_RIGHT_IDLE;
            }
            break;

        case WALK_RIGHT:
            if (animator->m_CurrentAnimation != walkRightAnimation || animator->m_CurrentAnimation2 != NULL) {
                animator->PlayAnimation(walkRightAnimation, NULL, animator->m_CurrentTime, 0.0f, 0.0f);
            }
            if (!requestWalkRight) {
                blendAmount = 0.0f;
                animator->PlayAnimation(walkRightAnimation, idleAnimation, animator->m_CurrentTime, 0.0f, blendAmount);
                charState = WALK_RIGHT_IDLE;
            }
            break;

        case WALK_RIGHT_IDLE:
            blendAmount += blendRate * deltaTime;
            if (blendAmount >= 1.0f) {
                blendAmount = 1.0f;
                animator->PlayAnimation(idleAnimation, NULL, animator->m_CurrentTime2, 0.0f, 0.0f);
                charState = IDLE;
            }
            else {
                animator->PlayAnimation(walkRightAnimation, idleAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
            }
            if (requestWalkRight) {
                blendAmount = 1.0f - blendAmount;
                animator->PlayAnimation(idleAnimation, walkRightAnimation, animator->m_CurrentTime, animator->m_CurrentTime2, blendAmount);
                charState = IDLE_WALK_RIGHT;
            }
            break;
        }
    }

    // --- RENDER ---

    void Draw(Shader& shader, bool flip = true) {
        // 1. Send Bone Transforms to Shader
        // Get bone transforms from the animator
        auto transforms = animator->GetFinalBoneMatrices();
        for (int i = 0; i < transforms.size(); ++i) {
            shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
        }

        // 2. Draw Player Model
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        float offset = flip ? 180.0f : 0.0f;
        model = glm::rotate(model, glm::radians(RotationY + offset), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(ModelScale));
        shader.setMat4("model", model);
        PlayerModel->Draw(shader);

        // 4. Draw Reticle (Cursor)
        /*glm::vec3 flatDir = glm::normalize(glm::vec3(ShootDirection.x, 0.0f, ShootDirection.z));
        glm::vec3 reticlePos = Position + (flatDir * currentAimDist);

        aimReticle.Draw(shader, reticlePos);*/
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
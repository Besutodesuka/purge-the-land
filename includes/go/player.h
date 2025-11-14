#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include <learnopengl/model.h>
#include <learnopengl/shader_m.h>
#include <go/physic.h>
#include <go/camera_3rd.h>// For the Camera_Movement enum

// Player physics constants
const float GRAVITY = -2.0f; // A bit stronger gravity
const float JUMP_FORCE = 1.0f;
const float PLAYER_SPEED = 1.0f;

class Player {
public:
    // Player state
    glm::vec3 Position;
    glm::vec3 Velocity;
    float RotationY;      // Y-axis rotation (for direction)
    bool IsGrounded;

    // Collision
    AABB CollisionBox;
    glm::vec3 AABBSize;   // The dimensions of the rat's collision box
    float AABBHeightOffset; // How far to shift the box center up

    // Model
    Model* PlayerModel;   // Pointer to the loaded rat model
    float ModelScale;

    // THIS IS THE 4-ARGUMENT CONSTRUCTOR
    Player(Model* model, glm::vec3 startPos, glm::vec3 boxSize, float modelScale)
        : PlayerModel(model), Position(startPos), AABBSize(boxSize),
        Velocity(0.0f), RotationY(0.0f), IsGrounded(false), ModelScale(modelScale)
    {
        // Calculate the center of the AABB (e.g., halfway up its height)
        AABBHeightOffset = boxSize.y / 2.0f;
        CollisionBox.Update(GetBoxCenter(), AABBSize);
    }

    // THIS IS THE GetBoxCenter() METHOD
    glm::vec3 GetBoxCenter() {
        // The box center is the player's position (at the feet) + half the box height
        return Position + glm::vec3(0.0f, AABBHeightOffset, 0.0f);
    }

    // THIS IS THE 4-ARGUMENT ProcessKeyboard METHOD
    void ProcessKeyboard(const glm::vec3& camFront, const glm::vec3& camRight, Camera_Movement direction, float deltaTime) {
        float velocity = PLAYER_SPEED;
        glm::vec3 moveDirection(0.0f);

        // Movement is relative to camera
        glm::vec3 camForwardHorizontal = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        glm::vec3 camRightHorizontal = glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));

        if (direction == FORWARD)
            moveDirection += camForwardHorizontal;
        if (direction == BACKWARD)
            moveDirection -= camForwardHorizontal;
        if (direction == LEFT)
            moveDirection -= camRightHorizontal;
        if (direction == RIGHT)
            moveDirection += camRightHorizontal;

        // Update velocity
        if (glm::length(moveDirection) > 0.0f) {
            moveDirection = glm::normalize(moveDirection);
            Velocity.x = moveDirection.x * velocity;
            Velocity.z = moveDirection.z * velocity;

            // Update rotation to face movement direction
            RotationY = glm::degrees(atan2(moveDirection.x, moveDirection.z));
        }
        else {
            // Stop if no keys are pressed
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

    // Update physics and check for collisions
    void Update(float deltaTime, const std::vector<AABB>& levelColliders) {
        // 1. Apply Gravity
        if (!IsGrounded) {
            Velocity.y += GRAVITY * deltaTime;
        }

        // 2. Simple collision response (check Y-axis, then X/Z axes)
        // This is a common and simple character controller method

        // --- Y-AXIS COLLISION ---
        Position.y += Velocity.y * deltaTime;
        CollisionBox.Update(GetBoxCenter(), AABBSize);
        IsGrounded = false;
        std::cout << "Player at y..." << Position.y << std::endl;

        for (const AABB& levelBox : levelColliders) {
            if (CheckCollision(CollisionBox, levelBox)) {
                // We have a Y-collision
                if (Velocity.y < 0) { // Moving down
                    // Hit the floor
                    // Set player *base* (Position.y) to the top of the box
                    Position.y = levelBox.max.y;
                    Velocity.y = 0.0f;
                    IsGrounded = true;
                }
                else if (Velocity.y > 0) { // Moving up
                    // Hit a ceiling
                    // Set player *base* (Position.y) to bottom of box minus box height
                    Position.y = levelBox.min.y - AABBSize.y;
                    Velocity.y = 0.0f;
                }
                // Update box position after correction
                CollisionBox.Update(GetBoxCenter(), AABBSize);
            }
        }

        // --- X/Z-AXIS COLLISION ---
        Position.x += Velocity.x * deltaTime;
        Position.z += Velocity.z * deltaTime;
        CollisionBox.Update(GetBoxCenter(), AABBSize);

        for (const AABB& levelBox : levelColliders) {
            if (CheckCollision(CollisionBox, levelBox)) {

                bool isFloor = (Velocity.y <= 0) && (levelBox.max.y < GetBoxCenter().y);

                if (!isFloor) {
                    Position.x -= Velocity.x * deltaTime;
                    Position.z -= Velocity.z * deltaTime;
                    break;
                }
                else if (Velocity.y > 0) {
                    // ... (existing code) ...
                                        // --- HIT A CEILING ---
                    Position.y = levelBox.min.y - AABBSize.y; // Snap to bottom of ceiling
                    Velocity.y = 0.0f;                        // Stop upward velocity
                    // ... (existing code) ...
                }
                Position.y = levelBox.max.y; // Snap position to top of floor
                Velocity.y = 0.0f;           // Stop all vertical velocity
                // Hit a wall. Just reset position.
                // A better response would slide along the wall, but this is simplest.
                CollisionBox.Update(GetBoxCenter(), AABBSize);
                 // Stop checking after one hit
            }
        }
    }

    // Draw the player model
    void Draw(Shader& shader, bool flip = true) {
        glm::mat4 model = glm::mat4(1.0f);
        // Translate to the player's position (the base of the model)
        model = glm::translate(model, Position);
        // Rotate to face the direction of movement
        float offset = 0.0f;
        if (flip)
			offset = 180.0f;
        model = glm::rotate(model, glm::radians(RotationY + offset), glm::vec3(0.0f, 1.0f, 0.0f));
        // Scale the model
        model = glm::scale(model, glm::vec3(ModelScale));

        shader.setMat4("model", model);
        PlayerModel->Draw(shader);
    }
};

#endif
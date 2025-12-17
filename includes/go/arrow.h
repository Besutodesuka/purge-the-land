#ifndef ARROW_H
#define ARROW_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm> // for std::min/max

#include <learnopengl/model.h>
#include <go/physic.h>

struct Arrow {
    glm::vec3 velocity;
    glm::vec3 position;
    bool active;
    OBB hitbox;
    float lifeTime;

    // Default constructor
    Arrow() : velocity(0.0f), position(0.0f), active(true) {}

    Arrow(glm::vec3 v, glm::vec3 p, glm::vec3 halfExtents) {
        velocity = v;
        position = p;
        active = true;
        lifeTime = 5.0f;

		// add this if your model is pointing up to sky instead of forward . the obb are based on forward this is need if you need to rotate model
		// Swap Y and Z for vertical to forward alignment
        float temp = halfExtents.y;
        halfExtents.y = halfExtents.z;
        halfExtents.z = temp;

        // 1. Setup static size
        hitbox.halfExtents = halfExtents;


        // 2. Initial Physics State
        UpdateOBB();
    };

    // Consolidated Math Logic: Calculates Axes based on Velocity
    //void UpdateOBB() {
    //    // 1. Set Center
    //    hitbox.center = position;

    //    // 2. Set Rotation (Axes)
    //    // Only update rotation if moving significantly to avoid NaN
    //    if (glm::length(velocity) > 0.01f) {
    //        glm::vec3 direction = glm::normalize(velocity);

    //        // Calculate Yaw (Y-axis) and Pitch (X-axis)
    //        float yaw = atan2(direction.x, direction.z);
    //        float pitch = asin(direction.y);

    //        // Create Rotation Matrix directly
    //        glm::mat4 R = glm::mat4(1.0f);
    //        R = glm::rotate(R, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    //        R = glm::rotate(R, -pitch, glm::vec3(1.0f, 0.0f, 0.0f));

    //        // Extract axes and store them in the OBB
    //        glm::mat3 rotationMat = glm::mat3(R);
    //        hitbox.axes[0] = rotationMat[0]; // Right
    //        hitbox.axes[1] = rotationMat[1]; // Up
    //        hitbox.axes[2] = rotationMat[2]; // Forward
    //    }
    //}

    void UpdateOBB() {
        // Calculate direction first so we can use it for the offset
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f); // Default
        if (glm::length(velocity) > 0.01f) {
            direction = glm::normalize(velocity);
        }

        // [FIX 2] OFFSET THE HITBOX CENTER
        // The 'position' is the Tip (0,0,1). The Hitbox Center is the middle of the shaft.
        // We move the center BACKWARDS along the direction vector by the half-length (z).
        hitbox.center = position - (direction * hitbox.halfExtents.z);

        // 3. Set Rotation (Standard LookAt Logic)
        if (glm::length(velocity) > 0.01f) {
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (abs(direction.y) > 0.99f) up = glm::vec3(1.0f, 0.0f, 0.0f);

            glm::vec3 right = glm::normalize(glm::cross(direction, up));
            glm::vec3 realUp = glm::normalize(glm::cross(right, direction));

            hitbox.axes[0] = right;
            hitbox.axes[1] = realUp;
            hitbox.axes[2] = direction;
        }
    }
};

class ArrowManager {
public:
    Model* ArrowModel;
    std::vector<Arrow> arrows;
    float arrowScale;
    glm::vec3 arrowHalfExtents;

    // Modified Constructor to handle nullptr safety
    ArrowManager(Model* model = nullptr, float scale = 0.1f) {
        Init(model, scale);
    }

    void Init(Model* model, float scale) {
        ArrowModel = model;
        arrowScale = scale;

        if (ArrowModel != nullptr) {
            std::vector<OBB> modelOBBs = GenerateOBBsFromModel(*ArrowModel, glm::mat4(1.0f));
            if (!modelOBBs.empty()) {
                arrowHalfExtents = modelOBBs[0].halfExtents * scale;
            }
            else {
                arrowHalfExtents = glm::vec3(0.1f, 0.1f, 0.5f);
            }
        }
        else {
            arrowHalfExtents = glm::vec3(0.1f, 0.1f, 0.5f);
        }
    }

    void Add_Arrow(glm::vec3 velocity, glm::vec3 position) {
        if (ArrowModel == nullptr) return; // Safety check
        Arrow new_arrow(velocity, position, arrowHalfExtents);
        arrows.push_back(new_arrow);
    }

    // --- PHYSICS LOOP ---
    // Responsible for Position, Gravity, and OBB Updates
    void Update(float deltaTime, const std::vector<OBB>& levelColliders) {
        // 1. Remove dead arrows (inactive or expired)
        arrows.erase(std::remove_if(arrows.begin(), arrows.end(),
            [](const Arrow& a) { return !a.active || a.lifeTime <= 0.0f; }),
            arrows.end());

        // 2. Update remaining arrows
        for (Arrow& a : arrows) {
            a.lifeTime -= deltaTime; // Decrease timer

            a.velocity.y -= 9.8f * deltaTime * 0.5f; 
            a.position += a.velocity * deltaTime;    
            a.UpdateOBB();

            for (const OBB& wall : levelColliders) {
                if (TestOBBOBB(a.hitbox, wall)) {
                    a.active = false; // This will trigger removal next frame
                    break;
                }
            }
        }
    }

    // --- RENDER LOOP ---
    void Render(Shader shader) {
        if (ArrowModel == nullptr) return;
        for (const Arrow& a : arrows) {
            // 1. Only draw active arrows
            if (!a.active) continue; 

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, a.position);
            
            // Rotation logic
            glm::mat4 rotation = glm::mat4(1.0f);
            rotation[0] = glm::vec4(a.hitbox.axes[0], 0.0f);
            rotation[1] = glm::vec4(a.hitbox.axes[1], 0.0f);
            rotation[2] = glm::vec4(a.hitbox.axes[2], 0.0f);
            model = model * rotation;

            // Offsets
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::translate(model, glm::vec3(0.0f, -a.hitbox.halfExtents.z,  0.0f));
            model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(arrowScale));

            shader.setMat4("model", model);
            ArrowModel->Draw(shader);
        }
    }
};
#endif
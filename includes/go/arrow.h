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

    // Default constructor
    Arrow() : velocity(0.0f), position(0.0f), active(true) {}

    Arrow(glm::vec3 v, glm::vec3 p, glm::vec3 halfExtents) {
        velocity = v;
        position = p;
        active = true;

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
        // Use Reference (&) to modify actual arrows
        for (Arrow& a : arrows) {
            if (!a.active) continue;

            // 1. Physics Math
            a.velocity.y -= 9.8f * deltaTime * 0.5f; // Gravity
            a.position += a.velocity * deltaTime;    // Move

            // 2. CRITICAL: Update OBB *before* Collision Check
            // This does the math once per frame
            a.UpdateOBB();

            // 3. Collision Check
            for (const OBB& wall : levelColliders) {
                if (TestOBBOBB(a.hitbox, wall)) {
                    std::vector<glm::vec3> corners = a.hitbox.getVertices();

                    // 2. Pass to your debug shader/line renderer
                    // (Pseudocode depends on your engine)
                    a.active = false;            // Stop gravity
					printf("Arrow hit!\n");
                    a.velocity = glm::vec3(0.0f); // Stop moving
                    break;
                }
            }
        }
    }

    // --- RENDER LOOP ---
    // Responsible ONLY for drawing, using data from the OBB
    void Render(Shader shader) {
        if (ArrowModel == nullptr) return;

        for (const Arrow& a : arrows) {
            glm::mat4 model = glm::mat4(1.0f);

            // 1. Move to the Physics World Position (The Tip)
            model = glm::translate(model, a.position);

            // 2. Rotate to face velocity (using your OBB axes)
            glm::mat4 rotation = glm::mat4(1.0f);
            rotation[0] = glm::vec4(a.hitbox.axes[0], 0.0f);
            rotation[1] = glm::vec4(a.hitbox.axes[1], 0.0f);
            rotation[2] = glm::vec4(a.hitbox.axes[2], 0.0f);
            model = model * rotation;

            // 3. [NEW] Offset the Center of Mass
            // Since you said the tip is at +Z (0,0,1) in the original model,
            // and you want the Tip to be at the pivot point (0,0,0),
            // we shift the mesh BACKWARDS so the tip aligns with the origin.
            // Adjust '1.0f' to match the actual length of your arrow model from center to tip.
            model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

            // be cause we swap y,z axis to make OBB rotate 90 dgree like model, it cause OBB to shift in y axis for z halfextend
            model = glm::translate(model, glm::vec3(0.0f, -a.hitbox.halfExtents.z,  0.0f));

            // 4. Handle your specific Model Adjustments (Scale/Flip)
            // Note: You had a 180 flip. If the model faces +Z, 
            // the 180 flip might be pointing it backwards. 
            // Ensure this flip is actually needed.
            model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(arrowScale));

            shader.setMat4("model", model);
            ArrowModel->Draw(shader);
        }
    }
};

#endif
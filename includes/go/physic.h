#ifndef PHYSICS_H
#define PHYSICS_H

#include <glm/glm.hpp>
#include <algorithm> // for std::min/max
#include <limits>    // for std::numeric_limits
#include <cmath>     // for std::abs, std::swap

// An Axis-Aligned Bounding Box
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    // Default constructor
    AABB() : min(0.0f), max(0.0f) {}

    // Constructor with min/max
    AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}

    // Update the box's position based on a center point and size
    void Update(const glm::vec3& center, const glm::vec3& size) {
        min = center - size / 2.0f;
        max = center + size / 2.0f;
    }
};

// Simple AABB vs AABB collision check
inline bool CheckCollision(const AABB& boxA, const AABB& boxB)
{
    // Check for overlap on all three axes
    bool overlapX = boxA.min.x <= boxB.max.x && boxA.max.x >= boxB.min.x;
    bool overlapY = boxA.min.y <= boxB.max.y && boxA.max.y >= boxB.min.y;
    bool overlapZ = boxA.min.z <= boxB.max.z && boxA.max.z >= boxB.min.z;
    return overlapX && overlapY && overlapZ;
}

// Your Ray-AABB Intersection test
// Checks if a ray intersects an AABB and returns the distance.
inline bool rayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const AABB& box, float& intersectionDistance)
{
    float tMin = 0.0f;
    float tMax = 100000.0f; // "infinity"

    for (int i = 0; i < 3; i++) {
        if (std::abs(rayDir[i]) < 1e-6) { // Ray is parallel
            if (rayOrigin[i] < box.min[i] || rayOrigin[i] > box.max[i]) {
                return false;
            }
        }
        else {
            float ood = 1.0f / rayDir[i];
            float t1 = (box.min[i] - rayOrigin[i]) * ood;
            float t2 = (box.max[i] - rayOrigin[i]) * ood;

            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }

    if (tMin < 0.0f) {
        intersectionDistance = tMax; // Inside box
    }
    else {
        intersectionDistance = tMin; // Outside box
    }

    if (intersectionDistance < 0.0f) return false;
    return true;
}

#endif
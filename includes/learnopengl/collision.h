#ifndef LEARNOPENGL_COLLISION_H
#define LEARNOPENGL_COLLISION_H

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// Axis-aligned bounding box for simple collisions
struct AABB {
	glm::vec3 min; // <-- Uses lowercase 'min'
	glm::vec3 max; // <-- Uses lowercase 'max'

	static AABB fromMinMax(const glm::vec3& mn, const glm::vec3& mx) {
		return AABB{ mn, mx };
	}

	static AABB fromCenterHalfExtents(const glm::vec3& center, const glm::vec3& halfExtents) {
		return AABB{ center - halfExtents, center + halfExtents };
	}

	// FIXED: Removed invisible unicode characters
	bool contains(const glm::vec3& p) const {
		return p.x >= min.x && p.x <= max.x &&
			p.y >= min.y && p.y <= max.y &&
			p.z >= min.z && p.z <= max.z;
	}

	// FIXED: Removed invisible unicode characters
	bool intersects(const AABB& other) const {
		return (min.x <= other.max.x && max.x >= other.min.x) &&
			(min.y <= other.max.y && max.y >= other.min.y) &&
			(min.z <= other.max.z && max.z >= other.min.z);
	}
};

// Compute closest point on an AABB to a point
inline glm::vec3 closestPointOnAABB(const glm::vec3& p, const AABB& box) {
	return glm::vec3(
		std::max(box.min.x, std::min(p.x, box.max.x)),
		std::max(box.min.y, std::min(p.y, box.max.y)),
		std::max(box.min.z, std::min(p.z, box.max.z))
	);
}

// Resolve collision between a sphere (center, radius) and an AABB by pushing the center out minimally.
// Returns true if a penetration was resolved and writes the corrected position to outCorrected.
inline bool resolveSphereAABBCollision(const glm::vec3& center,
	float radius,
	const AABB& box,
	glm::vec3& outCorrected)
{
	glm::vec3 closest = closestPointOnAABB(center, box);
	glm::vec3 delta = center - closest;
	float dist2 = glm::dot(delta, delta);
	float r2 = radius * radius;
	if (dist2 >= r2) {
		outCorrected = center;
		return false;
	}

	// If the center is inside the box (delta ~ 0), push out along smallest penetration axis
	if (dist2 < 1e-8f) {
		float px = std::min(std::abs(center.x - box.min.x), std::abs(box.max.x - center.x));
		float py = std::min(std::abs(center.y - box.min.y), std::abs(box.max.y - center.y));
		float pz = std::min(std::abs(center.z - box.min.z), std::abs(box.max.z - center.z));
		glm::vec3 dir(0.0f);
		if (px <= py && px <= pz) dir = glm::vec3((center.x - (box.min.x + box.max.x) * 0.5f) >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
		else if (py <= px && py <= pz) dir = glm::vec3(0.0f, (center.y - (box.min.y + box.max.y) * 0.5f) >= 0.0f ? 1.0f : -1.0f, 0.0f);
		else dir = glm::vec3(0.0f, 0.0f, (center.z - (box.min.z + box.max.z) * 0.5f) >= 0.0f ? 1.0f : -1.0f);
		outCorrected = center + dir * (radius + 1e-3f);
		return true;
	}

	float dist = std::sqrt(dist2);
	glm::vec3 n = delta / dist;
	float penetration = radius - dist;
	outCorrected = center + n * (penetration + 1e-3f);
	return true;
}

// Check if a sphere is grounded (touching the top surface of an AABB)
// Returns true if the bottom of the sphere is near or touching the top of the box
inline bool isSphereGrounded(const glm::vec3& center, float radius, const AABB& box, float tolerance = 0.1f) {
	float sphereBottom = center.y - radius;
	float boxTop = box.max.y;
	// Check if sphere bottom is near the box top (within tolerance)
	return sphereBottom <= boxTop + tolerance && sphereBottom >= boxTop - tolerance;
}

// Check if an AABB is grounded (touching the top surface of another AABB)
// Returns true if the bottom of the AABB is near or touching the top of the ground box
inline bool isAABBGrounded(const AABB& playerBox, const AABB& groundBox, float tolerance = 0.1f) {
	float playerBottom = playerBox.min.y;
	float groundTop = groundBox.max.y;
	return playerBottom <= groundTop + tolerance && playerBottom >= groundTop - tolerance;
}

// Resolve collision between two AABBs by pushing the player AABB out
// Returns true if collision was resolved
//
// FIXED: This logic is now correct. It finds the axis of *minimum* penetration
// and pushes the player out along that axis.
//
inline bool resolveAABBCollision(const AABB& playerBox, const AABB& obstacleBox, glm::vec3& outPlayerCenter) {
	if (!playerBox.intersects(obstacleBox)) {
		return false;
	}

	// Calculate overlap on each axis
	float overlapX = std::min(playerBox.max.x, obstacleBox.max.x) - std::max(playerBox.min.x, obstacleBox.min.x);
	float overlapY = std::min(playerBox.max.y, obstacleBox.max.y) - std::max(playerBox.min.y, obstacleBox.min.y);
	float overlapZ = std::min(playerBox.max.z, obstacleBox.max.z) - std::max(playerBox.min.z, obstacleBox.min.z);

	// Find the smallest overlap (this is the axis to push out on)
	float minOverlap = std::min(overlapX, std::min(overlapY, overlapZ));

	glm::vec3 playerCenter = (playerBox.min + playerBox.max) * 0.5f;
	glm::vec3 obstacleCenter = (obstacleBox.min + obstacleBox.max) * 0.5f;
	glm::vec3 direction = playerCenter - obstacleCenter;

	// Apply push-out based on the axis of *minimum* overlap
	if (minOverlap == overlapX) {
		// Push along X
		outPlayerCenter.x += (direction.x > 0 ? 1.0f : -1.0f) * (overlapX + 0.001f);
	}
	else if (minOverlap == overlapY) {
		// Push along Y
		outPlayerCenter.y += (direction.y > 0 ? 1.0f : -1.0f) * (overlapY + 0.001f);
	}
	else {
		// Push along Z
		outPlayerCenter.z += (direction.z > 0 ? 1.0f : -1.0f) * (overlapZ + 0.001f);
	}

	return true;
}

// Position an AABB on top of another AABB (for ground placement)
inline void positionAABBOnTop(const AABB& groundBox, const glm::vec3& halfExtents, glm::vec3& outCenter) {
	outCenter.y = groundBox.max.y + halfExtents.y;
}

#endif // LEARNOPENGL_COLLISION_H
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

// --- The Math Helper: Barycentric Interpolation ---
// Calculates the height (y) of a point (pos.x, pos.z) on a 3D triangle defined by p1, p2, p3.
// We ignore the Y component of the vectors for the "area" calculation, effectively projecting to the XZ plane.
float barycentricHeight(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float pX, float pZ) {
    // Calculate the determinant (twice the signed area of the triangle projected on XZ)
    float det = (p2.z - p3.z) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.z - p3.z);

    // Prevent division by zero if the triangle is degenerate (zero area)
    if (std::abs(det) < 0.00001f) return p1.y;

    // Calculate Barycentric weights (l1, l2, l3)
    // These represent how much "influence" each vertex has on the point (pX, pZ)
    float l1 = ((p2.z - p3.z) * (pX - p3.x) + (p3.x - p2.x) * (pZ - p3.z)) / det;
    float l2 = ((p3.z - p1.z) * (pX - p3.x) + (p1.x - p3.x) * (pZ - p3.z)) / det;
    float l3 = 1.0f - l1 - l2;

    // Return the weighted sum of the Y heights
    return l1 * p1.y + l2 * p2.y + l3 * p3.y;
}

class TerrainCollider {
private:
    std::vector<float> heightMap; // We still store just the floats for the look-up array
    int numCols = 0;
    int numRows = 0;

    // Bounds
    float minX = 0, minZ = 0;
    float maxX = 0, maxZ = 0;
    float quadWidth = 0;
    float quadHeight = 0;

public:
    void buildFromMesh(const std::vector<glm::vec3>& vertices) {
        if (vertices.empty()) return;

        // --- 1. Reset Bounds ---
        minX = std::numeric_limits<float>::max();
        minZ = std::numeric_limits<float>::max();
        maxX = -std::numeric_limits<float>::max();
        maxZ = -std::numeric_limits<float>::max();

        // --- 2. Scan Vertices ---
        // We use a set with a custom comparator to filter out "very close" vertices automatically
        auto floatCompare = [](float a, float b) {
            return a < b - 0.01f; // Treat floats within 0.01 as the same bucket
            };

        std::vector<float> xCoords;
        std::vector<float> zCoords;

        // Reserve to prevent reallocations during loop
        xCoords.reserve(vertices.size());
        zCoords.reserve(vertices.size());

        for (const auto& v : vertices) {
            if (v.x < minX) minX = v.x;
            if (v.x > maxX) maxX = v.x;
            if (v.z < minZ) minZ = v.z;
            if (v.z > maxZ) maxZ = v.z;

            xCoords.push_back(v.x);
            zCoords.push_back(v.z);
        }

        // --- 3. Robust Quad Size Calculation ---
        // Sort first
        std::sort(xCoords.begin(), xCoords.end());
        std::sort(zCoords.begin(), zCoords.end());

        // Custom "Unique" that ignores small floating point noise
        auto cleanUnique = [](std::vector<float>& vec) {
            if (vec.empty()) return;
            size_t writeIdx = 0;
            for (size_t i = 1; i < vec.size(); ++i) {
                // If the current value is significantly larger than the last written value
                if (vec[i] > vec[writeIdx] + 0.01f) {
                    writeIdx++;
                    vec[writeIdx] = vec[i];
                }
            }
            vec.resize(writeIdx + 1);
            };

        cleanUnique(xCoords);
        cleanUnique(zCoords);

        // Calculate widths based on the cleaned lists
        if (xCoords.size() > 1) quadWidth = xCoords[1] - xCoords[0];
        else quadWidth = 1.0f;

        if (zCoords.size() > 1) quadHeight = zCoords[1] - zCoords[0];
        else quadHeight = 1.0f;

        // SAFETY CLAMP: Never allow tiny quad sizes
        if (quadWidth < 0.1f) quadWidth = 1.0f;
        if (quadHeight < 0.1f) quadHeight = 1.0f;

        // --- 4. Determine Grid Resolution ---
        numCols = (int)((maxX - minX) / quadWidth + 0.5f) + 1;
        numRows = (int)((maxZ - minZ) / quadHeight + 0.5f) + 1;

        // --- CRITICAL SAFETY CHECK ---
        if (numCols > 5000 || numRows > 5000) {
            std::cerr << "ERROR: Terrain grid too dense! Cols: " << numCols << " Rows: " << numRows
                << ". Check your mesh scale or vertex spacing." << std::endl;
            return; // Stop before crashing
        }

        // --- 5. Fill Heightmap ---
        try {
            heightMap.assign(numCols * numRows, 0.0f);
        }
        catch (const std::bad_alloc& e) {
            std::cerr << "CRASH PREVENTED: " << e.what() << std::endl;
            return;
        }

        // Fill logic remains the same
        for (const auto& v : vertices) {
            int col = (int)((v.x - minX) / quadWidth + 0.5f);
            int row = (int)((v.z - minZ) / quadHeight + 0.5f);

            if (col >= 0 && col < numCols && row >= 0 && row < numRows) {
                heightMap[row * numCols + col] = v.y;
            }
        }

        std::cout << "Terrain Built Successfully: " << numCols << "x" << numRows << std::endl;
    }

    bool isBuilt() const {
        return !heightMap.empty();
	}

    // --- STEP 2: COLLISION CHECK ---
    float getExactHeightAt(const glm::vec3& position) {
        // 1. Convert World Coordinates to Grid Coordinates
        float relativeX = position.x - minX;
        float relativeZ = position.z - minZ;

        int gridX = (int)(relativeX / quadWidth);
        int gridZ = (int)(relativeZ / quadHeight);

        // 2. Boundary Check
        if (gridX < 0 || gridX >= numCols - 1 || gridZ < 0 || gridZ >= numRows - 1) {
            return -std::numeric_limits<float>::infinity();
        }

        // 3. Get fractional position
        float xCoord = (relativeX / quadWidth) - gridX;
        float zCoord = (relativeZ / quadHeight) - gridZ;

        // 4. Retrieve heights
        float h1 = heightMap[gridZ * numCols + gridX];             // Top Left
        float h2 = heightMap[gridZ * numCols + (gridX + 1)];       // Top Right
        float h3 = heightMap[(gridZ + 1) * numCols + gridX];       // Bottom Left
        float h4 = heightMap[(gridZ + 1) * numCols + (gridX + 1)]; // Bottom Right

        // 5. Determine triangle and interpolate
        float finalHeight;

        if (xCoord <= (1.0f - zCoord)) {
            // Top-Left Triangle
            glm::vec3 p1(0, h1, 0);
            glm::vec3 p2(1, h2, 0);
            glm::vec3 p3(0, h3, 1);
            finalHeight = barycentricHeight(p1, p2, p3, xCoord, zCoord);
        }
        else {
            // Bottom-Right Triangle
            glm::vec3 p1(1, h2, 0);
            glm::vec3 p2(1, h4, 1);
            glm::vec3 p3(0, h3, 1);
            finalHeight = barycentricHeight(p1, p2, p3, xCoord, zCoord);
        }

        return finalHeight;
    }
};

#endif
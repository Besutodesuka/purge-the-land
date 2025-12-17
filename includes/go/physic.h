#ifndef PHYSIC_H
#define PHYSIC_H

#include <glm/glm.hpp>
#include <algorithm> // for std::min/max
#include <limits>    // for std::numeric_limits
#include <cmath>     // for std::abs, std::swap
#include <vector>
#include <learnopengl/model.h> 

// An Axis-Aligned Bounding Box
// An Axis-Aligned Bounding Box
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    // New: Configuration for debug drawing
    // Format: R, G, B, Alpha. Default is White with 0.2 opacity.
    glm::vec4 debugColor;

    // Default constructor
    AABB()
        : min(0.0f), max(0.0f), debugColor(1.0f, 1.0f, 1.0f, 0.2f) {
    }

    // Constructor with min/max
    AABB(glm::vec3 min, glm::vec3 max)
        : min(min), max(max), debugColor(1.0f, 1.0f, 1.0f, 0.2f) {
    }

    // Update the box's position based on a center point and size
    void Update(const glm::vec3& center, const glm::vec3& size) {
        min = center - size / 2.0f;
        max = center + size / 2.0f;
    }

    // Helper to change color at runtime if needed
    void setDebugColor(float r, float g, float b, float a) {
        debugColor = glm::vec4(r, g, b, a);
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
    // --- FIX IS HERE ---
    float getExactHeightAt(const glm::vec3& position) {
        // 1. Convert World Coordinates to Grid Coordinates (Float)
        float relativeX = position.x - minX;
        float relativeZ = position.z - minZ;

        float fGridX = relativeX / quadWidth;
        float fGridZ = relativeZ / quadHeight;

        // 2. Use FLOOR to correctly handle negative numbers (Coordinate Mapping Fix)
        int gridX = (int)std::floor(fGridX);
        int gridZ = (int)std::floor(fGridZ);

        // 3. Boundary Check (with Clamping logic for edges)
        if (gridX < 0 || gridX >= numCols - 1 || gridZ < 0 || gridZ >= numRows - 1) {
            return -std::numeric_limits<float>::infinity();
        }

        // 4. Get fractional position [0.0, 1.0] within the cell
        float xCoord = fGridX - (float)gridX;
        float zCoord = fGridZ - (float)gridZ;

        // 5. Retrieve heights for the quad
        float h1 = heightMap[gridZ * numCols + gridX];             // Top Left
        float h2 = heightMap[gridZ * numCols + (gridX + 1)];       // Top Right
        float h3 = heightMap[(gridZ + 1) * numCols + gridX];       // Bottom Left
        float h4 = heightMap[(gridZ + 1) * numCols + (gridX + 1)]; // Bottom Right

        // 6. Barycentric Interpolation
        float finalHeight;
        if (xCoord <= (1.0f - zCoord)) {
            // Triangle 1 (Top Left)
            // p1(0,0), p2(1,0), p3(0,1) -> matching indices h1, h2, h3
            glm::vec3 p1(0, h1, 0);
            glm::vec3 p2(1, h2, 0);
            glm::vec3 p3(0, h3, 1);
            finalHeight = barycentricHeight(p1, p2, p3, xCoord, zCoord);
        }
        else {
            // Triangle 2 (Bottom Right)
            // p1(1,0), p2(1,1), p3(0,1) -> matching indices h2, h4, h3
            glm::vec3 p1(1, h2, 0);
            glm::vec3 p2(1, h4, 1);
            glm::vec3 p3(0, h3, 1);
            finalHeight = barycentricHeight(p1, p2, p3, xCoord, zCoord);
        }

        return finalHeight;
    }
};

// -------------------------------------------------------
// 1. The OBB Struct
// -------------------------------------------------------
struct OBB {
    glm::vec3 center;
    glm::vec3 axes[3];
    glm::vec3 halfExtents;

    // New: Debug Configuration
    glm::vec4 debugColor;

    // Constructor to initialize default axes and color
    OBB() : center(0.0f), halfExtents(1.0f), debugColor(0.0f, 1.0f, 0.0f, 1.0f) {
        axes[0] = glm::vec3(1, 0, 0);
        axes[1] = glm::vec3(0, 1, 0);
        axes[2] = glm::vec3(0, 0, 1);
    }

    void setDebugColor(float r, float g, float b, float a = 1.0f) {
        debugColor = glm::vec4(r, g, b, a);
    }

    // --- New: Helper to get the 8 corners for drawing ---
    // Returns vertices in World Space
    std::vector<glm::vec3> getVertices() const {
        std::vector<glm::vec3> v(8);
        glm::vec3 ax = axes[0] * halfExtents.x;
        glm::vec3 ay = axes[1] * halfExtents.y;
        glm::vec3 az = axes[2] * halfExtents.z;

        // Bottom quad
        v[0] = center - ax - ay - az;
        v[1] = center + ax - ay - az;
        v[2] = center + ax - ay + az;
        v[3] = center - ax - ay + az;
        // Top quad
        v[4] = center - ax + ay - az;
        v[5] = center + ax + ay - az;
        v[6] = center + ax + ay + az;
        v[7] = center - ax + ay + az;
        return v;
    }

    // --- New: Helper to get indices for GL_LINES ---
    // Returns the vertex indices that form the wireframe box
    std::vector<unsigned int> getWireframeIndices() const {
        return {
            0, 1, 1, 2, 2, 3, 3, 0, // Bottom face
            4, 5, 5, 6, 6, 7, 7, 4, // Top face
            0, 4, 1, 5, 2, 6, 3, 7  // Vertical pillars
        };
    }
};

// -------------------------------------------------------
// 2. The SAT Collision Math
// -------------------------------------------------------
bool TestOBBOBB(const OBB& a, const OBB& b) {
    // Compute rotation matrix expressing b in a's coordinate frame
    glm::mat3 R;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R[i][j] = glm::dot(a.axes[i], b.axes[j]);

    // Compute translation vector t
    glm::vec3 t = b.center - a.center;
    // Bring translation into a's coordinate frame
    t = glm::vec3(glm::dot(t, a.axes[0]), glm::dot(t, a.axes[1]), glm::dot(t, a.axes[2]));

    // Compute common subexpressions. Add in an epsilon term to counteract arithmetic errors
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R[i][j] = std::abs(R[i][j]) + 0.000001f;

    float ra, rb;

    // Test axes L = A0, L = A1, L = A2
    for (int i = 0; i < 3; i++) {
        ra = a.halfExtents[i];
        rb = b.halfExtents[0] * R[i][0] + b.halfExtents[1] * R[i][1] + b.halfExtents[2] * R[i][2];
        if (std::abs(t[i]) > ra + rb) return false;
    }

    // Test axes L = B0, L = B1, L = B2
    for (int i = 0; i < 3; i++) {
        ra = a.halfExtents[0] * R[0][i] + a.halfExtents[1] * R[1][i] + a.halfExtents[2] * R[2][i];
        rb = b.halfExtents[i];
        if (std::abs(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]) > ra + rb) return false;
    }

    // Test 9 Edge-Edge Cross Products
    // L = A0 x B0
    ra = a.halfExtents[1] * R[2][0] + a.halfExtents[2] * R[1][0];
    rb = b.halfExtents[1] * R[0][2] + b.halfExtents[2] * R[0][1];
    if (std::abs(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;

    // L = A0 x B1
    ra = a.halfExtents[1] * R[2][1] + a.halfExtents[2] * R[1][1];
    rb = b.halfExtents[0] * R[0][2] + b.halfExtents[2] * R[0][0];
    if (std::abs(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;

    // L = A0 x B2
    ra = a.halfExtents[1] * R[2][2] + a.halfExtents[2] * R[1][2];
    rb = b.halfExtents[0] * R[0][1] + b.halfExtents[1] * R[0][0];
    if (std::abs(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;

    // L = A1 x B0
    ra = a.halfExtents[0] * R[2][0] + a.halfExtents[2] * R[0][0];
    rb = b.halfExtents[1] * R[1][2] + b.halfExtents[2] * R[1][1];
    if (std::abs(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;

    // L = A1 x B1
    ra = a.halfExtents[0] * R[2][1] + a.halfExtents[2] * R[0][1];
    rb = b.halfExtents[0] * R[1][2] + b.halfExtents[2] * R[1][0];
    if (std::abs(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;

    // L = A1 x B2
    ra = a.halfExtents[0] * R[2][2] + a.halfExtents[2] * R[0][2];
    rb = b.halfExtents[0] * R[1][1] + b.halfExtents[1] * R[1][0];
    if (std::abs(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;

    // L = A2 x B0
    ra = a.halfExtents[0] * R[1][0] + a.halfExtents[1] * R[0][0];
    rb = b.halfExtents[1] * R[2][2] + b.halfExtents[2] * R[2][1];
    if (std::abs(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;

    // L = A2 x B1
    ra = a.halfExtents[0] * R[1][1] + a.halfExtents[1] * R[0][1];
    rb = b.halfExtents[0] * R[2][2] + b.halfExtents[2] * R[2][0];
    if (std::abs(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;

    // L = A2 x B2
    ra = a.halfExtents[0] * R[1][2] + a.halfExtents[1] * R[0][2];
    rb = b.halfExtents[0] * R[2][1] + b.halfExtents[1] * R[2][0];
    if (std::abs(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;

    return true; // Collision
}

// -------------------------------------------------------
// 3. The Auto-Generator Function
// -------------------------------------------------------
std::vector<OBB> GenerateOBBsFromModel(const Model& model, const glm::mat4& parentTransform) {
    std::vector<OBB> colliders;

    for (const auto& mesh : model.meshes) {
        if (mesh.vertices.empty()) continue;

        glm::vec3 minAABB(std::numeric_limits<float>::infinity());
        glm::vec3 maxAABB(std::numeric_limits<float>::lowest());

        // Find local bounds
        for (const auto& vertex : mesh.vertices) {
            minAABB.x = std::min(minAABB.x, vertex.Position.x);
            minAABB.y = std::min(minAABB.y, vertex.Position.y);
            minAABB.z = std::min(minAABB.z, vertex.Position.z);

            maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
            maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
            maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
        }

        OBB box; // Uses default constructor now
        glm::vec3 localCenter = (minAABB + maxAABB) * 0.5f;
        glm::vec3 localExtents = (maxAABB - minAABB) * 0.5f;

        // Apply parent transformation
        box.center = glm::vec3(parentTransform * glm::vec4(localCenter, 1.0f));

        glm::mat3 rotationScale = glm::mat3(parentTransform);
        box.axes[0] = glm::normalize(rotationScale[0]);
        box.axes[1] = glm::normalize(rotationScale[1]);
        box.axes[2] = glm::normalize(rotationScale[2]);

        // Handle scale
        glm::vec3 scale;
        scale.x = glm::length(rotationScale[0]);
        scale.y = glm::length(rotationScale[1]);
        scale.z = glm::length(rotationScale[2]);

        box.halfExtents = localExtents * scale;

        // Optional: Set a default debug color per generated box
        box.setDebugColor(0.0f, 1.0f, 0.0f, 1.0f); // Green

        colliders.push_back(box);
    }

    return colliders;
}

OBB GenerateOBBFromMesh(const Mesh& mesh, const glm::mat4& parentTransform) {
    OBB box; // Uses default constructor now
    if (mesh.vertices.empty()) return box;

    glm::vec3 minAABB(std::numeric_limits<float>::infinity());
    glm::vec3 maxAABB(std::numeric_limits<float>::lowest());

    // Find local bounds
    for (const auto& vertex : mesh.vertices) {
        minAABB.x = std::min(minAABB.x, vertex.Position.x);
        minAABB.y = std::min(minAABB.y, vertex.Position.y);
        minAABB.z = std::min(minAABB.z, vertex.Position.z);

        maxAABB.x = std::max(maxAABB.x, vertex.Position.x);
        maxAABB.y = std::max(maxAABB.y, vertex.Position.y);
        maxAABB.z = std::max(maxAABB.z, vertex.Position.z);
    }

    glm::vec3 localCenter = (minAABB + maxAABB) * 0.5f;
    glm::vec3 localExtents = (maxAABB - minAABB) * 0.5f;

    // Apply parent transformation
    box.center = glm::vec3(parentTransform * glm::vec4(localCenter, 1.0f));

    glm::mat3 rotationScale = glm::mat3(parentTransform);
    box.axes[0] = glm::normalize(rotationScale[0]);
    box.axes[1] = glm::normalize(rotationScale[1]);
    box.axes[2] = glm::normalize(rotationScale[2]);

    // Handle scale
    glm::vec3 scale;
    scale.x = glm::length(rotationScale[0]);
    scale.y = glm::length(rotationScale[1]);
    scale.z = glm::length(rotationScale[2]);

    box.halfExtents = localExtents * scale;

    // Optional: Set a default debug color per generated box
    box.setDebugColor(0.0f, 1.0f, 0.0f, 1.0f); // Green


    return box;
}
#endif
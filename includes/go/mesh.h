#pragma once // A modern header guard to prevent the file from being included multiple times

#include <glad/glad.h> // Holds all OpenGL type declarations

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

// A helper structure to define the memory layout of a single vertex.
// This allows the Mesh class to be incredibly flexible and handle any vertex format.
struct VertexAttribute {
    unsigned int location;     // The layout location in the shader (e.g., layout(location = 0))
    int          size;         // The number of components (e.g., 3 for a vec3)
    GLenum       type;         // The data type (e.g., GL_FLOAT)
    GLboolean    normalized;   // Whether the data should be normalized
    unsigned int stride;       // The total byte size of one complete vertex
    const void*  offset;       // The byte offset of this attribute from the start of the vertex
};

class Mesh {
public:
    // Public OpenGL handles
    unsigned int VAO, VBO, EBO;
    
    // Stores the number of indices to draw. If 0, we use glDrawArrays.
    unsigned int indexCount;
	int vertices_count;

    /**
     * @brief Constructor for an indexed mesh (uses an Element Buffer Object).
     * @param vertices The vertex data (e.g., positions, normals, texture coordinates).
     * @param indices The indices that form the faces of the mesh.
     * @param attributes A vector describing the memory layout of the vertex data.
     */
    Mesh::Mesh(
        const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices,
        const std::vector<VertexAttribute>& attributes
    ) {
        this->indexCount = indices.size();
        setupMesh(vertices.data(), vertices.size() * sizeof(float), indices.data(), indices.size() * sizeof(unsigned int), attributes);
    }

    /**
     * @brief Constructor for a non-indexed mesh (drawn with glDrawArrays).
     * @param vertices The vertex data.
     * @param vertexDataSize The total size of the vertex data array in bytes.
     * @param attributes A vector describing the memory layout of the vertex data.
     */
    Mesh(const float *vertices, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes) {
        this->indexCount = 0; // A value of 0 will signify that this is not an indexed mesh.
        vertices_count = vertexDataSize;
        setupMesh(vertices, vertexDataSize, nullptr, 0, attributes);
    }

    //Mesh(const std::vector<float>& vertices, size_t vertexDataSize, const std::vector<VertexAttribute>& attributes) {
    //    this->indexCount = 0; // A value of 0 will signify that this is not an indexed mesh.
    //    vertices_count = vertexDataSize;
    //    setupMesh(vertices.data(), vertexDataSize, nullptr, 0, attributes);
    //}

    /**
     * @brief Destructor that cleans up the GPU buffers to prevent memory leaks.
     */
    ~Mesh() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        if (EBO != 0) { // Only delete EBO if it was created
           glDeleteBuffers(1, &EBO);
        }
    }

    /**
     * @brief Binds the VAO and draws the mesh.
     */
    void Draw() {
        glBindVertexArray(VAO);
        // Check if this is an indexed mesh
        if (indexCount > 0) {
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        } else {
            // This case requires knowing the vertex count. For a simple cube it's 36.
            // A more advanced system would store this, but for now it's handled in the main loop.
            // Example for a cube; adjust as needed
             glDrawArrays(GL_TRIANGLES, 0, this->vertices_count);
        }
        glBindVertexArray(0); // Unbind VAO for good practice
    }

    void updateVertices(const std::vector<float>& vertices) {
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // Use glBufferSubData for efficiency
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    }

private:
    /**
     * @brief Encapsulates the OpenGL setup logic for creating buffers and attributes.
     *        This is called by the constructors.
     */
    void setupMesh(
        const void* vertices,
        size_t vertexDataSize,
        const void* indices,
        size_t indexDataSize,
        const std::vector<VertexAttribute>& attributes
    ) {
        // 1. Generate buffers and vertex array
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO); // Start configuring state for this VAO
        
        // 2. Load vertex data into the Vertex Buffer Object (VBO)
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize, vertices, GL_STATIC_DRAW);

        // 3. If index data is provided, create and load the Element Buffer Object (EBO)
        if (indices != nullptr) {
            glGenBuffers(1, &EBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexDataSize, indices, GL_STATIC_DRAW);
        } else {
            EBO = 0; // Set EBO to 0 to indicate it's not used
        }

        // 4. Set up the vertex attribute pointers based on the provided layout
        for (const auto& attr : attributes) {
            glEnableVertexAttribArray(attr.location);
            glVertexAttribPointer(attr.location, attr.size, attr.type, attr.normalized, attr.stride, attr.offset);
        }

        // Unbind the VAO to prevent accidental modification
        glBindVertexArray(0);
    }
};
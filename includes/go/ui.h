#ifndef UI_H
#define UI_H

#include <glad/glad.h> // Includes OpenGL headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <cmath>

// Assuming you use the standard LearnOpenGL shader class
#include <learnopengl/shader_m.h> 

class Reticle {
public:
    unsigned int VAO;
    unsigned int VBO;
    bool initialized;

    Reticle() : VAO(0), VBO(0), initialized(false) {}

    ~Reticle() {
        if (initialized) {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }
    }

    void Draw(Shader& shader, glm::vec3 position, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f)) {
        if (!initialized) SetupMesh();

        // 1. Enable Blending for Transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Optional: Disable Depth writing so the cursor doesn't glitch through the floor
        glDepthMask(GL_FALSE); 

        // 2. Transform
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::translate(model, glm::vec3(0.0f, 0.05f, 0.0f)); // Lift slightly up
        // Scale it: Make it bigger or smaller here
        model = glm::scale(model, glm::vec3(0.5f)); 

        shader.setMat4("model", model);
        shader.setVec4("color", color); // Use setVec4 for Alpha

        // 3. Draw
        glBindVertexArray(VAO);
        // We draw TRIANGLES now, not lines, to make it thick
        glDrawArrays(GL_TRIANGLES, 0, 12); 
        glBindVertexArray(0);

        // 4. Restore OpenGL State
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

private:
    void SetupMesh() {
        // Define a "+" shape using 2 Rectangles (4 triangles, 12 vertices)
        float thickness = 0.15f; // How thick the lines are
        float length = 1.0f;     // How long the lines are

        // Vertices: x, y, z
        float vertices[] = {
            // Horizontal Bar (Along X)
            -length, 0.0f,  thickness,  // Top Left
             length, 0.0f,  thickness,  // Top Right
             length, 0.0f, -thickness,  // Bottom Right
            
            -length, 0.0f,  thickness,  // Top Left
             length, 0.0f, -thickness,  // Bottom Right
            -length, 0.0f, -thickness,  // Bottom Left

            // Vertical Bar (Along Z)
             thickness, 0.0f, -length,  // Top Left
             thickness, 0.0f,  length,  // Bottom Left
            -thickness, 0.0f,  length,  // Bottom Right

             thickness, 0.0f, -length,  // Top Left
            -thickness, 0.0f,  length,  // Bottom Right
            -thickness, 0.0f, -length,  // Top Right
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        initialized = true;
    }
};

#endif
#ifndef UI_H
#define UI_H

#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <cmath>

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

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); 

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::translate(model, glm::vec3(0.0f, 0.05f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.5f)); 

        shader.setMat4("model", model);
        shader.setVec4("color", color); 

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 12); 
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

private:
    void SetupMesh() {
        float thickness = 0.15f; 
        float length = 1.0f;    

        float vertices[] = {
            -length, 0.0f,  thickness,  
             length, 0.0f,  thickness,  
             length, 0.0f, -thickness,  
            -length, 0.0f,  thickness,  
             length, 0.0f, -thickness,  
            -length, 0.0f, -thickness,  

             thickness, 0.0f, -length,  
             thickness, 0.0f,  length,  
            -thickness, 0.0f,  length,  
             thickness, 0.0f, -length,  
            -thickness, 0.0f,  length,  
            -thickness, 0.0f, -length,  
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

// ==========================================
// PLAYER HUD (Health Bar + Charge Bar)
// ==========================================
class PlayerHUD {
public:
    unsigned int VAO;
    unsigned int VBO;
    bool initialized;

    PlayerHUD() : VAO(0), VBO(0), initialized(false) {}

    ~PlayerHUD() {
        if (initialized) {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }
    }

    // Call this after drawing the 3D world.
    // Shader must have Projection/View set to Identity.
    void Draw(Shader& shader, float currentHealth, float maxHealth, float currentPower, float minPower, float maxPower) {
        if (!initialized) SetupMesh();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST); // Draw on top of everything

        // --- 1. HEALTH BAR ---
        float healthPct = currentHealth / maxHealth;
        if (healthPct < 0.0f) healthPct = 0.0f;

        // Position: Bottom Middle (Y = -0.85), Height = 0.05
        
        // Background (Grey)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -0.85f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.5f, 0.05f, 1.0f)); 
        shader.setMat4("model", model);
        shader.setVec4("color", glm::vec4(0.2f, 0.2f, 0.2f, 0.8f)); 
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Foreground (Green/Red)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -0.85f, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.5f * healthPct, 0.05f, 1.0f)); 
        glm::vec4 barColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.8f); // Green
        if(healthPct < 0.3f) barColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.8f); // Red
        shader.setMat4("model", model);
        shader.setVec4("color", barColor);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // --- 2. POWER BAR (CHARGE) ---
        float powerRange = maxPower - minPower;
        float powerPct = 0.0f;
        if (powerRange > 0.0001f) {
            powerPct = (currentPower - minPower) / powerRange;
        }
        if (powerPct < 0.0f) powerPct = 0.0f;
        if (powerPct > 1.0f) powerPct = 1.0f;

        // Position: Slightly above health bar (Y = -0.80), Thinner (Height = 0.015)
        float powerY = -0.79f;
        float powerHeight = 0.015f;

        // Background (Grey)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, powerY, 0.0f)); 
        model = glm::scale(model, glm::vec3(0.5f, powerHeight, 1.0f)); 
        shader.setMat4("model", model);
        shader.setVec4("color", glm::vec4(0.2f, 0.2f, 0.2f, 0.8f)); 
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Foreground (Yellow/Gold)
        if (powerPct > 0.01f) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, powerY, 0.0f)); 
            model = glm::scale(model, glm::vec3(0.5f * powerPct, powerHeight, 1.0f)); 
            shader.setMat4("model", model);
            // Gold Color
            shader.setVec4("color", glm::vec4(1.0f, 0.8f, 0.0f, 0.9f)); 
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // Restore State
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

private:
    void SetupMesh() {
        // Simple Quad (2D) centered at 0,0
        float vertices[] = {
            -0.5f,  0.5f, 0.0f, 
            -0.5f, -0.5f, 0.0f, 
             0.5f, -0.5f, 0.0f, 

            -0.5f,  0.5f, 0.0f, 
             0.5f, -0.5f, 0.0f, 
             0.5f,  0.5f, 0.0f  
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
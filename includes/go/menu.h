#ifndef MENU_H
#define MENU_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/shader_m.h>
#include <go/text_renderer.h> // Include the new header

struct Button {
    glm::vec2 position; // Center in NDC (-1 to 1)
    glm::vec2 size;     // Width/Height in NDC
    glm::vec4 color;
    
    bool IsClicked(float mouseX_NDC, float mouseY_NDC) {
        float halfW = size.x / 2.0f;
        float halfH = size.y / 2.0f;
        return (mouseX_NDC >= position.x - halfW && mouseX_NDC <= position.x + halfW &&
                mouseY_NDC >= position.y - halfH && mouseY_NDC <= position.y + halfH);
    }
};

class MenuBase {
protected:
    unsigned int VAO, VBO;
    bool initialized;

    MenuBase() : VAO(0), VBO(0), initialized(false) {}

    ~MenuBase() {
        if (initialized) {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }
    }

    void SetupMesh() {
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

    void DrawQuad(Shader& shader, glm::vec2 pos, glm::vec2 size, glm::vec4 color) {
        if (!initialized) SetupMesh();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(pos.x, pos.y, 0.0f));
        model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));
        shader.setMat4("model", model);
        shader.setVec4("color", color);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    // Helper to center text on a button
    // Note: TextRenderer uses Screen Coordinates (0 to Width), Button uses NDC (-1 to 1)
    void DrawButtonText(TextRenderer& tr, Button& btn, std::string text, int screenW, int screenH, float scale = 1.0f) {
        // Convert Button Center NDC to Screen Pixels
        float cx = (btn.position.x + 1.0f) * 0.5f * screenW;
        float cy = (btn.position.y + 1.0f) * 0.5f * screenH;
        
        // Approximate centering offset (assuming standard font size)
        // Adjust these magic numbers based on your font
        float textWidth = text.length() * 15.0f * scale; 
        float textHeight = 20.0f * scale;

        tr.RenderText(text, cx - (textWidth / 2.0f), cy - (textHeight / 2.0f), scale, glm::vec3(1.0f));
    }
};

class GameOverScreen : public MenuBase {
public:
    Button retryBtn;
    Button quitBtn;

    GameOverScreen() {
        retryBtn.position = glm::vec2(0.0f, 0.1f);
        retryBtn.size = glm::vec2(0.4f, 0.15f);
        retryBtn.color = glm::vec4(0.0f, 0.6f, 0.0f, 1.0f); // Darker Green

        quitBtn.position = glm::vec2(0.0f, -0.2f);
        quitBtn.size = glm::vec2(0.4f, 0.15f);
        quitBtn.color = glm::vec4(0.6f, 0.0f, 0.0f, 1.0f); // Darker Red
    }

    void Draw(Shader& shader, TextRenderer& tr, int screenW, int screenH) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        // 1. Draw UI Shapes (using cursorShader/UI Shader)
        shader.use();
        DrawQuad(shader, glm::vec2(0.0f, 0.0f), glm::vec2(2.0f, 2.0f), glm::vec4(0.1f, 0.0f, 0.0f, 0.8f)); // Bg
        DrawQuad(shader, retryBtn.position, retryBtn.size, retryBtn.color);
        DrawQuad(shader, quitBtn.position, quitBtn.size, quitBtn.color);

        // 2. Draw Text (using Text Shader internally)
        // Title
        float titleX = screenW * 0.35f; 
        float titleY = screenH * 0.75f;
        tr.RenderText("GAME OVER", titleX, titleY, 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        // Buttons
        DrawButtonText(tr, retryBtn, "RETRY", screenW, screenH, 1.0f);
        DrawButtonText(tr, quitBtn, "QUIT", screenW, screenH, 1.0f);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    int ProcessClick(float mouseX, float mouseY, int screenWidth, int screenHeight) {
        float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * mouseY) / screenHeight; 
        if (retryBtn.IsClicked(ndcX, ndcY)) return 1;
        if (quitBtn.IsClicked(ndcX, ndcY)) return 2;
        return 0;
    }
};

class PauseScreen : public MenuBase {
public:
    Button resumeBtn;
    Button quitBtn;

    PauseScreen() {
        resumeBtn.position = glm::vec2(0.0f, 0.1f);
        resumeBtn.size = glm::vec2(0.4f, 0.15f);
        resumeBtn.color = glm::vec4(0.0f, 0.3f, 0.8f, 1.0f); // Blue

        quitBtn.position = glm::vec2(0.0f, -0.2f);
        quitBtn.size = glm::vec2(0.4f, 0.15f);
        quitBtn.color = glm::vec4(0.6f, 0.0f, 0.0f, 1.0f); // Red
    }

    void Draw(Shader& shader, TextRenderer& tr, int screenW, int screenH) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        shader.use();
        DrawQuad(shader, glm::vec2(0.0f, 0.0f), glm::vec2(2.0f, 2.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
        DrawQuad(shader, resumeBtn.position, resumeBtn.size, resumeBtn.color);
        DrawQuad(shader, quitBtn.position, quitBtn.size, quitBtn.color);

        // Title
        float titleX = screenW * 0.42f; 
        float titleY = screenH * 0.75f;
        tr.RenderText("PAUSE", titleX, titleY, 2.0f, glm::vec3(1.0f, 1.0f, 1.0f));

        // Buttons
        DrawButtonText(tr, resumeBtn, "RESUME", screenW, screenH, 1.0f);
        DrawButtonText(tr, quitBtn, "QUIT", screenW, screenH, 1.0f);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    int ProcessClick(float mouseX, float mouseY, int screenWidth, int screenHeight) {
        float ndcX = (2.0f * mouseX) / screenWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * mouseY) / screenHeight; 
        if (resumeBtn.IsClicked(ndcX, ndcY)) return 1;
        if (quitBtn.IsClicked(ndcX, ndcY)) return 2;
        return 0;
    }
};

#endif
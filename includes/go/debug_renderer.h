#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <glad/glad.h> 
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <iostream>

#include "physic.h" 

class DebugRenderer {
private:
    unsigned int shaderProgram = 0;
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    bool isInitialized = false;

    // Helper to check for shader compile errors
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }

public:
    // Constructor is now empty to safe-guard against early initialization
    DebugRenderer() {}

    // Destructor to clean up
    ~DebugRenderer() {
        if (isInitialized) {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            glDeleteProgram(shaderProgram);
        }
    }

    // CALL THIS FUNCTION AFTER gladLoadGLLoader()
    void Init() {
        if (isInitialized) return; // Prevent double init

        // -----------------------------------------------------------------
        // 1. Define Simple Shaders directly in code
        // -----------------------------------------------------------------
        const char* vertexShaderSource = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            
            uniform mat4 view;
            uniform mat4 projection;
            
            void main() {
                gl_Position = projection * view * vec4(aPos, 1.0);
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 330 core
            out vec4 FragColor;
            
            uniform vec4 color;
            
            void main() {
                FragColor = color;
            }
        )";

        // -----------------------------------------------------------------
        // 2. Compile Shaders
        // -----------------------------------------------------------------
        unsigned int vertex, fragment;

        // Vertex Shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexShaderSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        // Fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        // Shader Program
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertex);
        glAttachShader(shaderProgram, fragment);
        glLinkProgram(shaderProgram);
        checkCompileErrors(shaderProgram, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        // -----------------------------------------------------------------
        // 3. Setup Buffers (VAO, VBO, EBO)
        // -----------------------------------------------------------------
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // VBO: Dynamic draw
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        // EBO: Constant indices
        unsigned int indices[] = {
            0, 1, 1, 2, 2, 3, 3, 0, // Bottom face loop
            4, 5, 5, 6, 6, 7, 7, 4, // Top face loop
            0, 4, 1, 5, 2, 6, 3, 7  // Connecting vertical lines
        };

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindVertexArray(0); // Unbind

        isInitialized = true;
        std::cout << "DebugRenderer Initialized Successfully." << std::endl;
    }

    void DrawOBBs(const std::vector<OBB>& obbs, const glm::mat4& view, const glm::mat4& projection) {
        if (!isInitialized) return; // Safety check

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);

        for (const auto& box : obbs) {
            std::vector<glm::vec3> vertices = box.getVertices();
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), vertices.data());

            int colorLoc = glGetUniformLocation(shaderProgram, "color");
            glUniform4fv(colorLoc, 1, glm::value_ptr(box.debugColor));

            glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    void DrawOBB(const OBB& obb, const glm::mat4& view, const glm::mat4& projection) {
        if (!isInitialized) return;

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(VAO);

        std::vector<glm::vec3> vertices = obb.getVertices();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), vertices.data());

        int colorLoc = glGetUniformLocation(shaderProgram, "color");
        glUniform4fv(colorLoc, 1, glm::value_ptr(obb.debugColor));

        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glUseProgram(0);
    }
};

#endif
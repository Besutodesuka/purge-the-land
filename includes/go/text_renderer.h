#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <map>
#include <string>
#include <iostream>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <learnopengl/shader_m.h>

struct Character {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2   Size;      // Size of glyph
    glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Offset to advance to next glyph
};

class TextRenderer {
public:
    std::map<char, Character> Characters;
    Shader* TextShader;
    unsigned int VAO, VBO;

    TextRenderer() : TextShader(nullptr) {}

    void Init(int width, int height, const char* vsPath, const char* fsPath, const char* fontPath) {
        // 1. Compile Shader
        TextShader = new Shader(vsPath, fsPath);
        glm::mat4 projection = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
        TextShader->use();
        TextShader->setMat4("projection", projection);
        
        // FIX: Explicitly tell the shader to use Texture Unit 0
        TextShader->setInt("text", 0); 

        // 2. Init FreeType
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            return;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath, 0, &face)) {
            std::cout << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
            return;
        }

        FT_Set_Pixel_Sizes(face, 0, 48); 
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                (unsigned int)face->glyph->advance.x
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        // 3. Config VAO/VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void RenderText(std::string text, float x, float y, float scale, glm::vec3 color) {
        if (!TextShader) return;

        // 1. Setup State for Text Rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        // CRITICAL FIX: Disable Face Culling. 
        // If your 3D game loop enabled this, the text quads might be culled (invisible).
        glDisable(GL_CULL_FACE); 

        // CRITICAL FIX: Disable Depth Test to ensure text is always on top.
        // (Even if the menu disables it, it's safer to enforce it here for the text specifically)
        glDisable(GL_DEPTH_TEST);

        TextShader->use();
        TextShader->setVec3("textColor", color);
        
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        std::string::const_iterator c;
        for (c = text.begin(); c != text.end(); c++) {
            Character ch = Characters[*c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            // Update VBO for each character
            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },

                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            
            // Explicitly bind VBO before updating data (Good practice)
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.Advance >> 6) * scale; 
        }
        
        // 2. Clean up / Restore State
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Re-enable depth test if expected by the rest of your loop
        glEnable(GL_DEPTH_TEST); 
        // (Optionally re-enable Culling if you know you need it immediately, 
        // but typically the 3D loop will just enable it again next frame).
    }
};

#endif
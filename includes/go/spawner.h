#ifndef SPAWNER_H
#define SPAWNER_H

#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <go/enemy.h>
#include <go/arrow.h>
#include <learnopengl/model_animation.h>

class MobSpawner {
public:
    glm::vec3 Position;
    float Scale;
    float CurrentHealth;
    float MaxHealth;
    bool IsDestroyed;
    
    // Logic
    float RespawnTimer;
    float TimeBetweenWaves;
    bool WaveInProgress;
    int NextSpawnCount;

    // Components
    Model* spawnerModel;
    OBB Collider;
    std::vector<Enemy*> myEnemies; // Track enemies spawned by this object

    MobSpawner(Model* model, glm::vec3 pos, float scale) 
        : spawnerModel(model), Position(pos), Scale(scale),
          MaxHealth(300.0f), CurrentHealth(300.0f), IsDestroyed(false),
          RespawnTimer(0.0f), TimeBetweenWaves(10.0f), 
          WaveInProgress(true), NextSpawnCount(4)
    {
        UpdateOBB();
    }

    void InitialSpawn(EnemyManager& enemyMgr) {
        SpawnWave(enemyMgr, 3);
        NextSpawnCount = 4; // Set subsequent waves to 4
    }

    void Update(float deltaTime, EnemyManager& enemyMgr) {
        if (IsDestroyed) return;

        // 1. Clean up list: Remove enemies that are about to be deleted by EnemyManager
        // IMPORTANT: This must be called BEFORE EnemyManager::Update deletes them
        myEnemies.erase(std::remove_if(myEnemies.begin(), myEnemies.end(),
            [](Enemy* e) { return e->MarkedForDeletion; }), myEnemies.end());

        // 2. Check if current wave is wiped out
        if (myEnemies.empty()) {
            RespawnTimer += deltaTime;
            if (RespawnTimer >= TimeBetweenWaves) {
                SpawnWave(enemyMgr, NextSpawnCount);
                RespawnTimer = 0.0f;
            }
        }
    }

    void SpawnWave(EnemyManager& enemyMgr, int count) {
        if (IsDestroyed) return;

        // Simple offset logic to place enemies around the spawner
        float radius = 3.0f;
        for (int i = 0; i < count; i++) {
            float angle = (360.0f / count) * i;
            float rad = glm::radians(angle);
            
            glm::vec3 spawnPos = Position;
            spawnPos.x += cos(rad) * radius;
            spawnPos.z += sin(rad) * radius;
            spawnPos.y = Position.y; // Assumes flat ground for simplicity

            // Spawn and track the pointer
            enemyMgr.Spawn(spawnPos, 0.5f, 100.0f);
            
            // The newly added enemy is the last one in the manager's list
            if (!enemyMgr.enemies.empty()) {
                myEnemies.push_back(enemyMgr.enemies.back());
            }
        }
    }

    void TakeDamage(float amount) {
        if (IsDestroyed) return;
        CurrentHealth -= amount;
        if (CurrentHealth <= 0) {
            CurrentHealth = 0;
            IsDestroyed = true;
            myEnemies.clear(); // Stop tracking, let them live out their lives
        }
    }

    void UpdateOBB() {
        // Approximate a box for the spawner
        Collider.center = Position + glm::vec3(0.0f, 1.5f * Scale, 0.0f);
        Collider.halfExtents = glm::vec3(1.0f, 1.5f, 1.0f) * Scale; 
        
        Collider.axes[0] = glm::vec3(1.0f, 0.0f, 0.0f);
        Collider.axes[1] = glm::vec3(0.0f, 1.0f, 0.0f);
        Collider.axes[2] = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    void Draw(Shader& shader) {
        if (IsDestroyed || !spawnerModel) return;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = glm::scale(model, glm::vec3(Scale));
        shader.setMat4("model", model);
        spawnerModel->Draw(shader);
    }

    // Draw a simple static health bar (Red/Green)
    void DrawHealthBar(Shader& uiShader) {
        if (IsDestroyed) return;
        
        // Reuse the logic from Enemy class for simplicity, or create a simple quad here
        // We will construct a simple VAO on the fly or static
        static unsigned int healthBarVAO = 0;
        if (healthBarVAO == 0) {
            float quadVertices[] = { -0.5f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, -0.5f, 0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f };
            unsigned int vbo;
            glGenVertexArrays(1, &healthBarVAO);
            glGenBuffers(1, &vbo);
            glBindVertexArray(healthBarVAO);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        }

        float healthPct = CurrentHealth / MaxHealth;
        glm::vec3 barPos = Position + glm::vec3(0.0f, Collider.halfExtents.y * 2.0f + 0.5f, 0.0f);
        
        // Red background
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, barPos);
        model = glm::scale(model, glm::vec3(2.0f, 0.2f, 1.0f));
        uiShader.setMat4("model", model);
        uiShader.setVec4("color", glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));
        glBindVertexArray(healthBarVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Green foreground
        model = glm::mat4(1.0f);
        model = glm::translate(model, barPos + glm::vec3(0.0f, 0.0f, 0.01f));
        model = glm::scale(model, glm::vec3(2.0f * healthPct, 0.2f, 1.0f)); 
        uiShader.setMat4("model", model);
        uiShader.setVec4("color", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
};

#endif
#ifndef ENEMY_H
#define ENEMY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <algorithm>

#include <learnopengl/model.h>
#include <go/physic.h>
#include <go/arrow.h> 
#include <go/player.h>

struct FloatingText {
    glm::vec3 position;
    int value;
    float lifeTime;
    float alpha;
};

// ==========================================
// SINGLE ENEMY CLASS
// ==========================================
class Enemy {
public:
    glm::vec3 Position;
    float Scale;
    float RotationY;

    float MaxHealth;
    float CurrentHealth;
    bool IsDead;

    // Attack Stats
    float AttackRange;
    float AttackDamage;
    float AttackCooldown;
    float CurrentCooldown;

    Model* EnemyModel;
    OBB Collider;

    glm::vec3 localColliderCenter;
    glm::vec3 localColliderExtents;

    std::vector<FloatingText> damageNumbers;

    Enemy() : EnemyModel(nullptr), Position(0.0f), Scale(1.0f), IsDead(true) {}

    Enemy(Model* model, glm::vec3 pos, float scale, float health)
        : EnemyModel(model), Position(pos), Scale(scale), RotationY(0.0f),
        MaxHealth(health), CurrentHealth(health), IsDead(false)
    {
        // Default attack stats
        AttackRange = 1.5f; // Close range
        AttackDamage = 10.0f;
        AttackCooldown = 1.0f; // Seconds between hits
        CurrentCooldown = 0.0f;

        InitCollider();
        UpdateOBB();
    }

    void InitCollider() {
        if (!EnemyModel) return;
        std::vector<OBB> tempOBBs = GenerateOBBsFromModel(*EnemyModel, glm::mat4(1.0f));
        if (!tempOBBs.empty()) {
            localColliderCenter = tempOBBs[0].center;
            localColliderExtents = tempOBBs[0].halfExtents;
        }
        else {
            localColliderCenter = glm::vec3(0.0f, 1.0f, 0.0f);
            localColliderExtents = glm::vec3(0.5f, 1.0f, 0.5f);
        }
    }

    void UpdateOBB() {
        Collider.center = Position + (localColliderCenter * Scale);
        Collider.halfExtents = localColliderExtents * Scale;

        glm::mat4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(RotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat3 R = glm::mat3(rotationMatrix);

        Collider.axes[0] = R[0];
        Collider.axes[1] = R[1];
        Collider.axes[2] = R[2];
    }

    void Update(float deltaTime) {
        if (IsDead) return;

        // Update Cooldown
        if (CurrentCooldown > 0.0f) {
            CurrentCooldown -= deltaTime;
        }

        // Floating Text Logic
        for (auto& txt : damageNumbers) {
            txt.lifeTime -= deltaTime;
            txt.alpha = std::max(0.0f, txt.lifeTime);
            txt.position.y += deltaTime * 1.5f; 
        }
        damageNumbers.erase(std::remove_if(damageNumbers.begin(), damageNumbers.end(),
            [](const FloatingText& t) { return t.lifeTime <= 0.0f; }), damageNumbers.end());
    }

    void TakeDamage(int amount) {
        if (IsDead) return;
        CurrentHealth -= amount;

        FloatingText txt;
        txt.position = Position + glm::vec3(0.0f, Collider.halfExtents.y * 2.0f + 0.5f, 0.0f);
        txt.value = amount;
        txt.lifeTime = 1.0f;
        txt.alpha = 1.0f;
        damageNumbers.push_back(txt);

        if (CurrentHealth <= 0.0f) {
            CurrentHealth = 0.0f;
            IsDead = true;
            Collider.halfExtents = glm::vec3(0.0f);
        }
    }

    // --- NEW: ATTACK LOGIC ---
    void CheckAttack(Player& player) {
        if (IsDead || player.IsDead) return;

        // Simple distance check (cylindrical approximation)
        float dist = glm::distance(glm::vec3(Position.x, 0, Position.z), glm::vec3(player.Position.x, 0, player.Position.z));

        if (dist <= AttackRange && CurrentCooldown <= 0.0f) {
            // Attack!
            player.TakeDamage(AttackDamage);
            CurrentCooldown = AttackCooldown; 
            std::cout << "Enemy hit Player! HP: " << player.CurrentHealth << std::endl;
        }
    }

    void Draw(Shader& shader) {
        if (IsDead || !EnemyModel) return;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = glm::rotate(model, glm::radians(RotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(Scale));
        shader.setMat4("model", model);
        EnemyModel->Draw(shader);
    }

    void DrawHealthBar(Shader& uiShader) {
        if (IsDead) return;
        static unsigned int healthBarVAO = 0;
        if (healthBarVAO == 0) {
            float quadVertices[] = {
                -0.5f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f,
                -0.5f, 0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.0f
            };
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
        glm::vec3 barPos = Position + glm::vec3(0.0f, Collider.halfExtents.y * 2.0f + 0.2f, 0.0f);

        DrawBillboard(uiShader, healthBarVAO, barPos, 1.0f, 0.1f, glm::vec3(0.8f, 0.8f, 0.8f));
        DrawBillboard(uiShader, healthBarVAO, barPos + glm::vec3(0.0f, 0.0f, 0.01f), healthPct, 0.1f, glm::vec3(0.0f, 0.8f, 0.0f));
    }

private:
    void DrawBillboard(Shader& shader, unsigned int vao, glm::vec3 pos, float width, float height, glm::vec3 color) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
        shader.setMat4("model", model);
        shader.setVec4("color", glm::vec4(color, 1.0f ));
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
};

// ==========================================
// ENEMY MANAGER
// ==========================================
class EnemyManager {
public:
    std::vector<Enemy> enemies;
    Model* SharedModel; 

    EnemyManager() : SharedModel(nullptr) {}

    void Init(Model* model) {
        SharedModel = model;
    }

    void Spawn(glm::vec3 position, float scale, float health) {
        if (!SharedModel) return;
        enemies.emplace_back(SharedModel, position, scale, health);
    }

    void Update(float deltaTime, ArrowManager& arrowManager, Player& player) {
        for (auto& enemy : enemies) {
            if (enemy.IsDead) continue;

            // 1. Update Enemy Logic
            enemy.Update(deltaTime);

            // 2. Check Attack on Player (NEW)
            enemy.CheckAttack(player);

            // 3. Check Collisions against Arrows
            for (Arrow& arrow : arrowManager.arrows) {
                if (!arrow.active) continue;

                if (TestOBBOBB(enemy.Collider, arrow.hitbox)) {
                    arrow.active = false;      
                    arrow.velocity = glm::vec3(0.0f);
                    enemy.TakeDamage(25);      
                }
            }
        }
    }

    void Render(Shader& shader) {
        for (auto& enemy : enemies) {
            enemy.Draw(shader);
        }
    }

    void RenderUI(Shader& shader) {
        for (auto& enemy : enemies) {
            enemy.DrawHealthBar(shader);
        }
    }

    const std::vector<Enemy>& GetEnemies() const {
        return enemies;
    }
    
    void Reset() {
        enemies.clear();
        // You will re-spawn them in main.cpp
    }
};

#endif
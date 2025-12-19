#ifndef ENEMY_H
#define ENEMY_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <filesystem> // Added for file checking

#include <learnopengl/shader_m.h>
#include <learnopengl/animator.h>        
#include <learnopengl/model_animation.h> 
#include <learnopengl/filesystem.h> 

#include <go/physic.h>
#include <go/arrow.h> 
#include <go/player.h>

struct EnemyResources {
    Model* enemyModel;
    Animation* idleAnim;
    Animation* walkAnim;
    Animation* attackAnim;
    Animation* dieAnim;

    EnemyResources() : enemyModel(nullptr), idleAnim(nullptr), walkAnim(nullptr), attackAnim(nullptr), dieAnim(nullptr) {}
};

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
    enum AnimState {
        STATE_IDLE,
        STATE_CHASE,
        STATE_ATTACK,
        STATE_DEAD
    };

    // Transform
    glm::vec3 Position;
    glm::vec3 Velocity;
    float Scale;
    float RotationY;

    // Stats
    float MaxHealth;
    float CurrentHealth;
    bool IsDead;
    float Speed;

    // Physics
    const float GRAVITY = -9.8f;
    bool IsGrounded;

    // Death Logic
    float DeathTimer;
    bool MarkedForDeletion;
    bool HasHit;

    // Attack Stats
    float AttackRange;
    float DetectionRange;
    float AttackDamage;
    float AttackCooldown;
    float CurrentCooldown;

    // Components
    OBB Collider;
    glm::vec3 localColliderCenter;
    glm::vec3 localColliderExtents;
    std::vector<FloatingText> damageNumbers;

    // Animation
    Animator* animator;
    EnemyResources* resources;
    AnimState currentState;

    // Blending Variables
    Animation* currentAnimPtr; // The animation we are fading OUT of
    Animation* nextAnimPtr;    // The animation we are fading INTO
    float blendAmount;
    float blendRate;           // Lower = Slower blend (e.g., 2.0f = 0.5 seconds)

    Enemy(EnemyResources* res, glm::vec3 pos, float scale, float health)
        : resources(res), Position(pos), Scale(scale), RotationY(0.0f),
        MaxHealth(health), CurrentHealth(health), IsDead(false),
        Velocity(0.0f), IsGrounded(false), Speed(3.0f),
        DeathTimer(0.0f), MarkedForDeletion(false), HasHit(false),
        blendAmount(1.0f), blendRate(2.0f), currentAnimPtr(nullptr), nextAnimPtr(nullptr)
    {
        AttackRange = 1.2f;
        DetectionRange = 25.0f;
        AttackDamage = 15.0f;
        AttackCooldown = 1.8f;
        CurrentCooldown = 0.0f;

        // Initialize Animator - SAFETY CHECK ADDED
        if (resources && resources->idleAnim) {
            nextAnimPtr = resources->idleAnim; // Start fully in idle
            currentAnimPtr = resources->idleAnim;
            animator = new Animator(nextAnimPtr);
            currentState = STATE_IDLE;
        }
        else {
            // If idle anim failed to load, don't crash, just run without animation
            std::cout << "WARNING: Enemy spawned without Idle Animation!" << std::endl;
            animator = nullptr;
            currentState = STATE_IDLE;
        }

        InitCollider();
        UpdateOBB();
    }

    // Disable copying
    Enemy(const Enemy&) = delete;
    Enemy& operator=(const Enemy&) = delete;

    ~Enemy() {
        if (animator) delete animator;
    }

    void InitCollider() {
        if (!resources || !resources->enemyModel) return;
        localColliderCenter = glm::vec3(0.0f, 1.0f, 0.0f);
        localColliderExtents = glm::vec3(0.4f, 0.9f, 0.4f);
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

    void Update(float deltaTime, Player& player, TerrainCollider* ground = nullptr) {
        if (animator) {
            animator->UpdateAnimation(deltaTime);

            if (blendAmount < 1.0f) {
                blendAmount += blendRate * deltaTime;
                if (blendAmount > 1.0f) blendAmount = 1.0f;

                // Safety check for pointers before blending
                if (currentAnimPtr && nextAnimPtr) {
                    animator->PlayAnimation(currentAnimPtr, nextAnimPtr, animator->m_CurrentTime, 0.0f, blendAmount);
                }
            }
        }
        UpdateFloatingText(deltaTime);

        // --- DEATH LOGIC ---
        if (IsDead) {
            DeathTimer += deltaTime;

            // 1. Calculate the exact duration of the death animation in seconds
            if (resources->dieAnim) {
                float ticksPerSecond = resources->dieAnim->GetTicksPerSecond();
                if (ticksPerSecond == 0.0f) ticksPerSecond = 25.0f;
                float duration = resources->dieAnim->GetDuration() / ticksPerSecond;

                // 2. If the timer exceeds duration, Mark for Deletion IMMEDIATELY
                if (DeathTimer >= duration) {
                    MarkedForDeletion = true;
                }
            }
            else {
                // If no animation exists, delete instantly
                MarkedForDeletion = true;
            }
            return; // Exit function so we don't move or attack
        }

        if (CurrentCooldown > 0.0f) CurrentCooldown -= deltaTime;
        UpdateAI(player, deltaTime);

        // Physics
        Velocity.y += GRAVITY * deltaTime;
        Position += Velocity * deltaTime;

        // Ground Clamp
        float floorHeight = 0.0f;
        if (ground && ground->isBuilt()) {
            float h = ground->getExactHeightAt(Position);
            if (h > -500.0f) floorHeight = h;
        }

        if (Position.y < floorHeight) {
            Position.y = floorHeight;
            Velocity.y = 0.0f;
            IsGrounded = true;
        }

        UpdateOBB();
    }

    void UpdateAI(Player& player, float deltaTime) {
        if (player.IsDead) {
            SwitchState(STATE_IDLE);
            return;
        }

        float dist = glm::distance(Position, player.Position);
        glm::vec3 dir = player.Position - Position;
        dir.y = 0;

        if (glm::length(dir) > 0.1f) {
            dir = glm::normalize(dir);
            float targetAngle = glm::degrees(atan2(dir.x, dir.z));
            RotationY = targetAngle;
        }

        if (dist <= AttackRange) {
            Velocity.x = 0;
            Velocity.z = 0;
            SwitchState(STATE_ATTACK);

            if (animator) {
                float tps = (resources->attackAnim) ? resources->attackAnim->GetTicksPerSecond() : 25.0f;
                float hitTimeTicks = 1.2f * tps; // 1.2 second converted to ticks

                if (animator->m_CurrentTime < hitTimeTicks * 0.1f) {
                    HasHit = false;
                }

                if (animator->m_CurrentTime >= hitTimeTicks && !HasHit) {
                    player.TakeDamage(AttackDamage);
                    HasHit = true;
                    std::cout << "Enemy attacks Player!" << std::endl;
                }
            }
        }

        else if (dist <= DetectionRange) {
            Velocity.x = dir.x * Speed;
            Velocity.z = dir.z * Speed;
            SwitchState(STATE_CHASE);
        }
        else {
            Velocity.x = 0;
            Velocity.z = 0;
            SwitchState(STATE_IDLE);
        }
    }

    void SwitchState(AnimState newState) {
        if (currentState == newState) return;

        // Determine the target animation
        Animation* target = nullptr;

        // Safety: Ensure resources exist before accessing
        if (!resources) return;

        switch (newState) {
        case STATE_IDLE:   target = resources->idleAnim; break;
        case STATE_CHASE:  target = resources->walkAnim; break;
        case STATE_ATTACK: target = resources->attackAnim; break;
        case STATE_DEAD:   target = resources->dieAnim; break;
        }

        // Only switch if the target animation actually exists (was loaded successfully)
        if (target && nextAnimPtr != target) {
            currentState = newState;
            currentAnimPtr = nextAnimPtr; // Old 'Next' becomes 'Current' (fade out)
            nextAnimPtr = target;         // New target (fade in)
            blendAmount = 0.0f;           // Start blend from 0

            if (animator) animator->m_CurrentTime = 0.0f; // Reset animation time to 0
            if (newState == STATE_ATTACK) HasHit = false; // Reset attack flag
        }
    }

    void UpdateFloatingText(float deltaTime) {
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
        txt.position = Position + glm::vec3(0.0f, Collider.halfExtents.y * 2.0f, 0.0f);
        txt.value = amount;
        txt.lifeTime = 1.0f;
        txt.alpha = 1.0f;
        damageNumbers.push_back(txt);

        if (CurrentHealth <= 0.0f) {
            CurrentHealth = 0.0f;
            IsDead = true;
            SwitchState(STATE_DEAD);
            Collider.halfExtents = glm::vec3(0.0f); // Remove collision immediately
        }
    }

    void Draw(Shader& shader) {
        if (!resources || !resources->enemyModel) return;

        if (animator) {
            auto transforms = animator->GetFinalBoneMatrices();
            for (int i = 0; i < transforms.size(); ++i) {
                shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
            }
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = glm::rotate(model, glm::radians(RotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(Scale));
        shader.setMat4("model", model);
        resources->enemyModel->Draw(shader);
    }

    void DrawHealthBar(Shader& uiShader) {
        if (IsDead) return;
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

        float healthPct = std::max(0.0f, CurrentHealth / MaxHealth);
        glm::vec3 barPos = Position + glm::vec3(0.0f, Collider.halfExtents.y * 2.0f + 0.4f, 0.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, barPos);
        model = glm::scale(model, glm::vec3(1.0f, 0.1f, 1.0f));
        uiShader.setMat4("model", model);
        uiShader.setVec4("color", glm::vec4(0.5f, 0.0f, 0.0f, 1.0f));
        glBindVertexArray(healthBarVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        model = glm::mat4(1.0f);
        model = glm::translate(model, barPos + glm::vec3(0.0f, 0.0f, 0.01f));
        model = glm::scale(model, glm::vec3(healthPct, 0.1f, 1.0f));
        uiShader.setMat4("model", model);
        uiShader.setVec4("color", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
};

// ==========================================
// ENEMY MANAGER
// ==========================================
class EnemyManager {
public:
    std::vector<Enemy*> enemies;
    EnemyResources resources;
    TerrainCollider* groundRef;

    EnemyManager() : groundRef(nullptr) {}

    void Init(Model* model, TerrainCollider* ground = nullptr) {
        resources.enemyModel = model;
        groundRef = ground;

        std::cout << "Loading Enemy Animations..." << std::endl;

        // --- SAFE LOAD LAMBDA ---
        // Prevents crashes by checking if the file exists before loading
        auto SafeLoad = [&](const std::string& path) -> Animation* {
            std::string fullPath = path;
            if (!std::filesystem::exists(fullPath)) {
                std::cout << "CRITICAL ERROR: Enemy Animation Missing: " << fullPath << std::endl;
                return nullptr; // Returns NULL instead of crashing
            }
            std::cout << "Loading: " << path << std::endl;
            return new Animation(fullPath, model);
            };

        // Load animations safely
        resources.idleAnim = SafeLoad("resources/objects/enemy/Zombie Idle.dae");
        resources.walkAnim = SafeLoad("resources/objects/enemy/Scary Clown Walk.dae");
        resources.attackAnim = SafeLoad("resources/objects/enemy/Standing Melee Attack Downward.dae");
        resources.dieAnim = SafeLoad("resources/objects/enemy/Falling Back Death.dae");

        std::cout << "--- Enemy Animation Init Complete ---" << std::endl;
    }

    ~EnemyManager() {
        Reset();
        if (resources.idleAnim) delete resources.idleAnim;
        if (resources.walkAnim) delete resources.walkAnim;
        if (resources.attackAnim) delete resources.attackAnim;
        if (resources.dieAnim) delete resources.dieAnim;
    }

    void Spawn(glm::vec3 position, float scale, float health) {
        if (!resources.enemyModel) return;
        enemies.push_back(new Enemy(&resources, position, scale, health));
    }

    void Update(float deltaTime, ArrowManager& arrowManager, Player& player) {
        // Iterate with a standard iterator to allow erasure
        for (auto it = enemies.begin(); it != enemies.end(); ) {
            Enemy* enemy = *it;

            enemy->Update(deltaTime, player, groundRef);

            // 1. Check if marked for deletion
            if (enemy->MarkedForDeletion) {
                delete enemy;
                it = enemies.erase(it);
                continue;
            }

            if (!enemy->IsDead) {
                // Collision with arrows
                for (Arrow& arrow : arrowManager.arrows) {
                    if (!arrow.active) continue;

                    if (TestOBBOBB(enemy->Collider, arrow.hitbox)) {
                        arrow.active = false;
                        float dmg = player.GetArrowDamage(arrow.velocity);
                        enemy->TakeDamage(dmg);
                    }
                }
            }
            ++it;
        }
    }

    void Render(Shader& shader) {
        for (auto* enemy : enemies) {
            enemy->Draw(shader);
        }
    }

    void RenderUI(Shader& shader) {
        for (auto* enemy : enemies) {
            enemy->DrawHealthBar(shader);
        }
    }

    const std::vector<Enemy*>& GetEnemies() const { return enemies; }

    void Reset() {
        for (auto* enemy : enemies) {
            delete enemy;
        }
        enemies.clear();
    }
};

#endif
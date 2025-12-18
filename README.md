# Purge the Land

**Purge the Land** is a C++ and OpenGL-based action game where the player battles to reclaim a corrupted world. The project was built from scratch using the graphics pipeline to handle rendering, physics, and game logic. This game is part of CPE494 course at KMUTT.

## Group Members
- Kittipob Borisut 65070503407 
- Rapepong Pitijaroonpong 65070503434

## 1. Introduction

### Game Objective
The land has been corrupted by the influence of **Dark Crystals**, which spawn monsters hostile to everything in their proximity. The player's duty is to destroy these crystals, defeat the skeleton guardians, and reclaim the land from corruption.

### Player Abilities & Mechanics
The game features a combat system balancing offense and resource management:
- **Charged Attack:** Hold **Left Click** to draw the bow. A **Yellow Power Bar** indicates charge level. Releasing shoots an arrow dealing damage based on power.
- **Dash:** Press **Space Bar** to dash and evade attacks (Cooldown indicated by **Blue Bar**).
- **Heal:** Press **E** to recover health (Cooldown indicated by **Red Plus Sign**).
- **Camera:** Zoom in and out using the mouse scroll wheel.

### Controls
| Key | Action |
| :--- | :--- |
| **W, A, S, D** | Player Movement |
| **Left Click (Hold/Release)** | Charge and Shoot Arrow |
| **Space Bar** | Dash (Blue Indicator) |
| **E** | Heal (Red Indicator) |
| **Mouse Scroll** | Zoom In / Out |

---

## 2. Implementation details

The project uses **C++** and **OpenGL** for the graphics engine.

### Libraries Used
Standard *LearnOpenGL* stack:
- **GLFW & GLAD:** For windowing and OpenGL context management.
- **GLM:** For vector and matrix mathematics.
- **Assimp:** For model loading.
- **stb_image:** For texture loading.

### Model Loading & Map Construction
- **Technique:** We utilized **Assimp** to handle model importing.
- **Map Design:** Due to the complexity of manually placing objects using OpenGL translation matrices, the map was constructed entirely within **Blender**. We set the world origin properly in Blender before export to ensure the entire map loads as a single, correctly positioned unit in the engine.

### Collision Detection
- Implemented hitboxes that synchronize with model scaling.
- Logic detects interactions between player projectiles, enemy entities, and environmental obstacles.

### Shaders & Logic
- **Shaders:** Custom GLSL shaders handle the lighting models and the rendering of the UI elements (charge bars).
- **Game Logic:** The main loop manages cooldown timers for abilities and calculates damage based on the arrow's charge duration.

### Drawing UI
- **HUD Elements:**
    - **Yellow Bar:** Dynamic rendering based on mouse-hold duration.
    - **Blue/Red Indicators:** Visual feedback for Dash and Heal cooldown states.

### Asset used
|Object|Link|License|
|--|--|--|
|Player model,animation and arrow|https://www.mixamo.com/#/?page=1&query=erika%20with%20bow&type=Character|Adobe copyright https://www.adobe.com/legal/terms.html|
|Skeleton enemy|https://sketchfab.com/3d-models/skeleton-87e64f068f824c2c8673200911452cf1|CC Attribution|
|Terrain map|https://sketchfab.com/3d-models/free-low-poly-forest-6dc8c85121234cb59dbd53a673fa2b8f|CC Attribution|

---

## 3. Problems During Implementation

### Map & Model Challenges
- **Vertex Balance:** We faced significant difficulty balancing the number of vertices against load times. High-detail models caused performance drops, requiring optimization.
- **Texture Baking:** A lack of experience with texture baking in Blender made getting shaders to interact correctly with imported map textures difficult.
- **Hitbox Synchronization:** Scaling models in OpenGL caused issues where the visual mesh size did not match the invisible hitbox, leading to collision inaccuracies.

### Map Placement Workflow
- Hardcoding object positions (translation/scaling) in raw OpenGL proved too time-consuming.
- **Solution:** We pivoted to creating the map layout in Blender. However, this required strict management of the "World Origin" to ensure the map didn't load offset from the player.

### Library Versioning (Assimp)
- **Challenge:** We attempted to upgrade **Assimp to version 6** (newer build) to natively load `.GLB` models (which would remove the need for manual texture mapping).
- **Outcome:** The upgrade caused significant conflicts and bugs with existing legacy libraries in the project. We were forced to cancel the upgrade and revert to the previous version, continuing with standard texture mapping methods.

---

# Installation & Build

1. Clone the repository:
```bash
   git clone [https://github.com/Besutodesuka/purge-the-land.git](https://github.com/Besutodesuka/purge-the-land.git)
```
## Build
1. Create build folder
2. Open Cmake program
3. Select this repository as source and the build folder as target
4. Click on configure and then generate button
5. Then you shall find the sln file in the build folder

## DEMO
https://github.com/user-attachments/assets/e0b6b6db-e410-4548-a04f-9753b55e5d76

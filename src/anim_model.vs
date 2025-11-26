#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 tex;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec3 bitangent;
layout(location = 5) in ivec4 boneIds;
layout(location = 6) in vec4 weights;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

out vec2 TexCoords;

void main()
{
    vec4 totalPosition = vec4(0.0f);
    float totalWeight = 0.0f; // Track if we actually used any bones

    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
    {
        // Skip invalid bones
        if(boneIds[i] == -1) 
            continue;
        
        // Safety check for array bounds
        if(boneIds[i] >= MAX_BONES) 
        {
            totalPosition = vec4(pos,1.0f);
            totalWeight = 1.0f; // Force weight to 1 to skip fallback
            break;
        }

        vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(pos,1.0f);
        totalPosition += localPosition * weights[i];
        totalWeight += weights[i];
   }
    
    // --- SAFETY NET ---
    // If no bones influenced this vertex (totalWeight is 0), 
    // render it at its original static position.
    if (totalWeight <= 0.001f) {
        totalPosition = vec4(pos, 1.0f);
    }
    // ------------------

    mat4 viewModel = view * model;
    gl_Position =  projection * viewModel * totalPosition;
    TexCoords = tex;
}
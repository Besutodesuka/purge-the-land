#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// Texture Samplers
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_metallic1;
uniform sampler2D texture_roughness1;

// Scene Data
uniform vec3 camPos;

// --- CHANGED: Support Multiple Lights ---
#define MAX_LIGHTS 16
uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS]; 
// ----------------------------------------

const float PI = 3.14159265359;

// ... [Keep your PBR Functions: DistributionGGX, GeometrySchlickGGX, GeometrySmith, fresnelSchlick SAME AS BEFORE] ...
// (I am omitting them here to save space, do not delete them from your file)
float DistributionGGX(vec3 N, vec3 H, float roughness) { /* ... keep existing code ... */ return 0.0; } // Placeholder for brevity
float GeometrySchlickGGX(float NdotV, float roughness) { /* ... keep existing code ... */ return 0.0; }
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) { /* ... keep existing code ... */ return 0.0; }
vec3 fresnelSchlick(float cosTheta, vec3 F0) { /* ... keep existing code ... */ return vec3(0.0); }

void main()
{
    // 1. Sample Textures
    vec3 albedo = pow(texture(texture_diffuse1, TexCoords).rgb, vec3(2.2));
    float metallic = texture(texture_metallic1, TexCoords).r;
    float roughness = texture(texture_roughness1, TexCoords).r;

    // 2. Setup Vectors
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // --- LIGHTING CALCULATION (Loop over all lights) ---
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < numLights; ++i) 
    {
        // Calculate per-light radiance
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        
        float distance = length(lightPositions[i] - WorldPos);
        // Inverse square law for PBR
        float attenuation = 1.0 / (distance * distance); 
        vec3 radiance = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        // Accumulate light
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // --- POST PROCESSING ---
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // HDR Tone Mapping
    color = color / (color + vec3(1.0));
    // Gamma Correction
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}
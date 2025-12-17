#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// Texture Samplers
uniform sampler2D texture_diffuse1;   // Base Color (Albedo)
uniform sampler2D texture_metallic1;  // Metallic Map
uniform sampler2D texture_roughness1; // Roughness Map
// uniform sampler2D texture_normal1; // Optional: Add if you have normal maps

// Scene Data (YOU MUST PASS THESE FROM C++)
uniform vec3 camPos;
uniform vec3 lightPos;   // Position of the light source
uniform vec3 lightColor; // Color/Intensity of the light (e.g., 300.0, 300.0, 300.0)

const float PI = 3.14159265359;

// --- PBR FUNCTIONS ---

// 1. Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001); // Prevent divide by zero
}

// 2. Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// 3. Geometry Smith (Combines Obstruction and Shadowing)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 4. Fresnel Equation (Fresnel-Schlick)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // 1. Sample Textures
    // Note: We raise Albedo to power 2.2 to convert from sRGB to Linear Space
    vec3 albedo = pow(texture(texture_diffuse1, TexCoords).rgb, vec3(2.2));
    float metallic = texture(texture_metallic1, TexCoords).r;
    float roughness = texture(texture_roughness1, TexCoords).r;

    // 2. Setup Vectors
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    // 3. Calculate Reflectance at normal incidence (F0)
    // Non-metals use 0.04, Metals use the Albedo color
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // --- LIGHTING CALCULATION (Single Point Light) ---
    vec3 Lo = vec3(0.0);
    
    // Calculate per-light radiance
    vec3 L = normalize(lightPos - WorldPos);
    vec3 H = normalize(V + L);
    float distance = length(lightPos - WorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = lightColor * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent div by zero
    vec3 specular = numerator / denominator;
    
    // kS is equal to Fresnel
    vec3 kS = F;
    // kD is the remaining energy (diffuse)
    vec3 kD = vec3(1.0) - kS;
    // Multiply kD by inverse metalness such that only non-metals have diffuse lighting
    kD *= 1.0 - metallic;	  

    // Scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);        

    // Final outgoing radiance
    Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
    
    // --- POST PROCESSING ---
    
    // Add a little ambient light so it's not pitch black in shadows
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // HDR Tone Mapping (Reinhard)
    color = color / (color + vec3(1.0));
    
    // Gamma Correction (Convert back to sRGB)
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}
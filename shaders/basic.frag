#version 410 core

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fragPosLightSpace;

out vec4 fColor;

// --- Lighting ---
uniform vec3 lightDir; // Soare
uniform vec3 lightColor;

// Point Light 1 (Lanterna)
uniform vec3 pointLightPos;
uniform vec3 pointLightColor;

// Point Light 2 (Campfire) - NOU
uniform vec3 campfirePos;
uniform vec3 campfireColor;

// --- Textures ---
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D shadowMap;

// --- Material ---
float ambientStrength = 0.2;
float specularStrength = 0.5;
float shininess = 32.0;

// Constante pentru atenuare
float constant = 1.0f;
float linear = 0.14f;
float quadratic = 0.07f;

uniform bool enableFog;

vec3 computeDirLight(vec3 normalEye, vec3 viewDir, vec3 diffTex, vec3 specTex, float shadow)
{
    vec3 lightDirN = normalize(lightDir);
    
    vec3 ambient = ambientStrength * lightColor * diffTex;
    
    float diff = max(dot(normalEye, lightDirN), 0.0);
    vec3 diffuse = diff * lightColor * diffTex;
    
    vec3 reflectDir = reflect(-lightDirN, normalEye);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor * specTex;
    
    return (ambient + (1.0 - shadow) * diffuse) + (1.0 - shadow) * specular;
}

// AM MODIFICAT ACEASTA FUNCTIE SA PRIMEASCA POZITIA SI CULOAREA CA PARAMETRI
vec3 computeCustomPointLight(vec3 lightPos, vec3 lightColor, vec3 normalEye, vec3 viewDir, vec3 diffTex, vec3 specTex)
{
    // Calculam directia si distanta fata de lumina specifica
    vec3 lightDir = normalize(lightPos - fPosition);
    float distance = length(lightPos - fPosition);
    
    // 
    // Calculam atenuarea (cat de repede scade lumina cu distanta)
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));    

    // Diffuse
    float diff = max(dot(normalEye, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normalEye);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    
    // Rezultate
    // Folosim o ambientala foarte mica pentru point lights ca sa nu lumineze totul prea tare
    vec3 ambient = 0.05 * lightColor * diffTex; 
    vec3 diffuse = diff * lightColor * diffTex;
    vec3 specular = specularStrength * spec * lightColor * specTex;
    
    // Aplicam atenuarea
    return (ambient + diffuse + specular) * attenuation;
}

float computeFog()
{
    float fogDensity = 0.03;
    float dist = length(fPosition);
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    return clamp(fogFactor, 0.0, 1.0);
}

float computeShadow()
{
    vec3 normalizedCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    normalizedCoords = normalizedCoords * 0.5 + 0.5;
    if (normalizedCoords.z > 1.0) return 0.0;
    float currentDepth = normalizedCoords.z;
    vec3 normal = normalize(fNormal);
    vec3 lightDirN = normalize(lightDir);
    float bias = max(0.005 * (1.0 - dot(normal, lightDirN)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, normalizedCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{
    vec3 normalEye = normalize(fNormal);
    vec3 viewDir = normalize(-fPosition);
    vec3 texDiffuse = texture(diffuseTexture, fTexCoords).rgb;
    vec3 texSpecular = texture(specularTexture, fTexCoords).rgb;

    float shadow = computeShadow();

    // 1. Lumina Soarelui (Directional)
    vec3 dirLight = computeDirLight(normalEye, viewDir, texDiffuse, texSpecular, shadow);
    
    // 2. Lumina Lanternei (Point Light 1)
    // Apelam functia generica cu pozitia si culoarea lanternei
    vec3 lanternLight = computeCustomPointLight(pointLightPos, pointLightColor, normalEye, viewDir, texDiffuse, texSpecular);

    // 3. Lumina Focului (Point Light 2) - NOU
    // Apelam aceeasi functie, dar cu pozitia si culoarea focului
    vec3 fireLight = computeCustomPointLight(campfirePos, campfireColor, normalEye, viewDir, texDiffuse, texSpecular);

    // Adunam toate luminile
    vec3 finalColor = dirLight + lanternLight + fireLight;

    if (enableFog) {
        float fogFactor = computeFog();
        vec4 fogColor = vec4(0.3, 0.3, 0.3, 1.0);
        fColor = mix(fogColor, vec4(finalColor, 1.0), fogFactor);
    } else {
        fColor = vec4(finalColor, 1.0);
    }
}
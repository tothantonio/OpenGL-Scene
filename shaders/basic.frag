#version 410 core

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fragPosLightSpace;

out vec4 fColor;

// lighting
uniform vec3 lightDir;
uniform vec3 lightColor;

// textures
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D shadowMap;

// material
float ambientStrength = 0.2;
float specularStrength = 0.5;
float shininess = 32.0;

vec3 ambient;
vec3 diffuse;
vec3 specular;

uniform bool enableFog;

void computeDirLight()
{
    vec3 normalEye = normalize(fNormal);
    vec3 lightDirN = normalize(lightDir);

    vec3 viewDir = normalize(-fPosition);

    // ambient
    ambient = ambientStrength * lightColor;

    // diffuse
    float diff = max(dot(normalEye, lightDirN), 0.0);
    diffuse = diff * lightColor;

    // specular
    vec3 reflectDir = reflect(-lightDirN, normalEye);
    float specCoeff = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    specular = specularStrength * specCoeff * lightColor;
}

float computeFog()
{
    float fogDensity = 0.05;
    float dist = length(fPosition);
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    return clamp(fogFactor, 0.0, 1.0);
}

float computeShadow()
{
    vec3 normalizedCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    normalizedCoords = normalizedCoords * 0.5 + 0.5;

    if (normalizedCoords.z > 1.0)
        return 0.0;

    float closestDepth = texture(shadowMap, normalizedCoords.xy).r;

    float currentDepth = normalizedCoords.z;

    vec3 normal = normalize(fNormal);
    vec3 lightDirN = normalize(lightDir);
    float bias = max(0.05 * (1.0 - dot(normal, lightDirN)), 0.005); 
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main()
{
    computeDirLight();

    float shadow = computeShadow();

    vec3 texDiffuse  = texture(diffuseTexture, fTexCoords).rgb;
    vec3 texSpecular = texture(specularTexture, fTexCoords).rgb;

    vec3 color = (ambient + (1.0 - shadow) * diffuse) * texDiffuse + 
                 (1.0 - shadow) * specular * texSpecular;

    if (enableFog) {
        float fogFactor = computeFog();
        vec4 fogColor = vec4(0.5, 0.5, 0.5, 1.0);
        fColor = mix(fogColor, vec4(color, 1.0), fogFactor);
    } else {
        fColor = vec4(color, 1.0);
    }
}

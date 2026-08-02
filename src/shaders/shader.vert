#version 450

layout (location = 0) in vec2 quadPos;
layout (location = 1) in vec3 particlePos;
layout (location = 0) out int particleIndex;
layout (location = 1) out vec2 fragQuadPos;
layout (location = 2) out uint shouldDiscard;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    int numParticles;
    float particleRadius;
    float smoothingRadius;
    float g;
    vec3 minXYZ;
    float dt;
    vec3 maxXYZ;
    float pad0;
    float particleMass;
    float pressureMultiplier;
    float targetDensity;
    float viscosityStrength;
};

void main() {
    // gl_Position = proj * view * model * vec4(particlePos + particleRadius * vec3(quadPos.x, 0, quadPos.y), 1.0);
    mat3 camRot = transpose(mat3(view));

    vec3 cameraRight = camRot[0];
    vec3 cameraUp    = camRot[1];

    vec3 worldPos =
        particlePos +
        particleRadius *
        (quadPos.x * cameraRight +
         quadPos.y * cameraUp);

    gl_Position = proj * view * model * vec4(worldPos, 1.0);

    particleIndex = int(gl_InstanceIndex);
    fragQuadPos = quadPos;
    if (particlePos.x <= minXYZ.x + 2 * particleRadius ||
        particlePos.y <= minXYZ.y + 2 * particleRadius ||
        particlePos.z <= minXYZ.z + 2 * particleRadius ||
        particlePos.x >= maxXYZ.x - 2 * particleRadius ||
        particlePos.y >= maxXYZ.y - 2 * particleRadius
        ) {
        shouldDiscard = 1;
    }
    else {
        shouldDiscard = 0;
    }

    shouldDiscard = 0;
}

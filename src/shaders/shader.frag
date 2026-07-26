#version 450

// layout(location = 0) in vec2 outNDC;
layout (location = 0) flat in int particleIndex;
layout (location = 1) in vec2 fragQuadPos;
layout (location = 2) flat in uint shouldDiscard;
layout(location = 0) out vec4 outColor;

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
struct CellEntry {
    uint key;
    uint particleIndex;
};
layout (binding = 1, rgba32f) uniform writeonly image2D outputImage;
layout (std430, binding = 2) readonly buffer PositionsBuffer           { vec4 positions[]; };
layout (std430, binding = 3) readonly buffer VelocitiesBuffer          { vec4 velocities[]; };
layout (std430, binding = 4) readonly buffer PredictedPositionsBuffer  { vec4 predictedPositions[]; };
layout (std430, binding = 5) readonly buffer PredictedVelocitiesBuffer { vec4 predictedVelocities[]; };
layout (std430, binding = 6) readonly buffer CellEntriesBuffer         { CellEntry cellEntries[]; };
layout (std430, binding = 7) readonly buffer DensitiesBuffer           { float densities[]; };
layout (std430, binding = 8) readonly buffer StartIndices              { int startIndices[]; };
layout (std430, binding = 9) readonly buffer ColorBuffer               { vec4 colors[]; };

int cellCoordToHash(ivec3 cellCoord) {
    return cellCoord.x * 73856093 + cellCoord.y * 19349663 + cellCoord.z * 83492791;
}

int cellHashToKey(int cellHash) {
    return int(mod(cellHash, numParticles));
}

int cellCoordToKey(ivec3 cellCoord) {
    return cellHashToKey(cellCoordToHash(cellCoord));
}

int positionToKey(vec3 pos) {
    return cellCoordToKey(ivec3(floor((pos - minXYZ)/smoothingRadius)));
}

void main() {
    // if (dot(fragQuadPos, fragQuadPos) > 1.0) discard;
    // outColor = vec4(colors[particleIndex].xyz, 1);

    float r2 = dot(fragQuadPos, fragQuadPos);

    if (r2 > 1.0)
        discard;

    float z = sqrt(1.0 - r2);

    vec3 normal = normalize(vec3(
        -fragQuadPos.x,
        -fragQuadPos.y,
        z
    ));

    vec3 lightDir = normalize(vec3(-0.5, -0.5, 1.0));

    float lighting = max(dot(normal, lightDir), 0.0);

    float alpha = 1.0 - smoothstep(0.8, 1.0, sqrt(r2));

    vec3 color = colors[particleIndex].xyz;

    float darknessStrength = 0.3;
    color *= (1.0f - darknessStrength + darknessStrength * lighting);

    outColor = vec4(color, alpha);

    if (shouldDiscard == 1) discard;
}

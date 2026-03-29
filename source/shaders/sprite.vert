#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    mat3  transform;      // offset 0,  size 48 (each column padded to vec4)
    int   layerIndex;     // offset 48, size 4
    vec3  tintColor;      // offset 52, size 12
    float tintIntensity;  // offset 64, size 4
    float opacity;        // offset 68, size 4
    // Total: 72 bytes
} pc;

void main()
{
    outTexCoord = inTexCoord;
    vec3 worldPos = pc.transform * vec3(inPosition, 1.0);
    gl_Position = vec4(worldPos.xy, 0.0, 1.0);
}

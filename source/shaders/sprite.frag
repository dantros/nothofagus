#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2DArray textureSampler;

layout(push_constant) uniform PushConstants {
    mat3  transform;
    int   layerIndex;
    vec3  tintColor;
    float tintIntensity;
    float opacity;
} pc;

void main()
{
    vec4  s          = texture(textureSampler, vec3(inTexCoord, float(pc.layerIndex)));
    vec3  blendColor = pc.tintColor * pc.tintIntensity + s.rgb * (1.0 - pc.tintIntensity);
    outColor = vec4(blendColor, s.a * pc.opacity);
}

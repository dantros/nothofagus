#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform usampler2DArray indexSampler;
layout(set = 0, binding = 1) uniform sampler1D paletteSampler;

layout(push_constant) uniform PushConstants {
    mat3  transform;
    int   layerIndex;
    float tintColor[3];
    float tintIntensity;
    float opacity;
} pc;

void main()
{
    ivec3 texSize    = textureSize(indexSampler, 0);
    uint  colorIndex = texelFetch(indexSampler,
        ivec3(int(inTexCoord.x * float(texSize.x)),
              int(inTexCoord.y * float(texSize.y)),
              pc.layerIndex), 0).r;
    vec4 paletteColor = texelFetch(paletteSampler, int(colorIndex), 0);
    vec3 tintColor    = vec3(pc.tintColor[0], pc.tintColor[1], pc.tintColor[2]);
    vec3 blendColor   = tintColor * pc.tintIntensity + paletteColor.rgb * (1.0 - pc.tintIntensity);
    outColor = vec4(blendColor, paletteColor.a * pc.opacity);
}

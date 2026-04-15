#version 450

layout(location = 0) in  vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

// Binding 0: RGBA tile atlas (2D array — one layer per tile)
layout(set = 0, binding = 0) uniform sampler2DArray atlasSampler;
// Binding 1: R8UI tile-index map (one uint8 per cell)
layout(set = 0, binding = 1) uniform usampler2D     mapSampler;

// Push constants — layout must match SpritePushConstants in vulkan_backend.h.
// mat3 columns are padded to vec4 in Vulkan GLSL (16 bytes each).
// The _unused field occupies the layerIndex slot so the layout stays identical
// to sprite.vert, allowing a single vertex shader for all three pipelines.
layout(push_constant) uniform PushConstants
{
    vec4 col0;           // transform column 0 (xyz used, w padding)
    vec4 col1;           // transform column 1 (xyz used, w padding)
    vec4 col2;           // transform column 2 (xyz used, w padding)
    int  _unused;        // was layerIndex — kept for layout compatibility
    float tintColor[3];
    float tintIntensity;
    float opacity;
} pc;

void main()
{
    // Map UV → cell coordinate
    ivec2 mapSize    = textureSize(mapSampler, 0);
    vec2  cellF      = inTexCoord * vec2(mapSize);
    ivec2 cellI      = clamp(ivec2(cellF), ivec2(0), mapSize - ivec2(1));
    vec2  withinCell = fract(cellF);

    // Integer texture requires texelFetch (not texture())
    uint  tileIdx = texelFetch(mapSampler, cellI, 0).r;

    // Sample the atlas at the resolved tile layer
    vec4  color = texture(atlasSampler, vec3(withinCell, float(tileIdx)));

    // Tint + opacity (identical to the other sprite shaders)
    vec3 tint    = vec3(pc.tintColor[0], pc.tintColor[1], pc.tintColor[2]);
    vec3 blended = tint * pc.tintIntensity + color.rgb * (1.0 - pc.tintIntensity);
    outColor     = vec4(blended, color.a * pc.opacity);
}

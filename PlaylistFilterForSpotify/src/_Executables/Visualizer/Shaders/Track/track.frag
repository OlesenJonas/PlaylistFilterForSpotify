#version 450 core

out vec4 fragmentColor;

in TrackData{
    vec2 uv;
    flat uint layer;
} VertexIn;

layout (binding = 0) uniform sampler2DArray coverArray;

void main()
{
    vec3 coords = vec3(VertexIn.uv, VertexIn.layer);
    fragmentColor = texture(coverArray, coords);
}
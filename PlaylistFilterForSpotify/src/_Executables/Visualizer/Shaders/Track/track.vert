#version 450 core

layout(location = 0) in vec3 positionAttribute;
layout(location = 1) in uint layerAttribute;

layout(location = 0) uniform mat4 model;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 projection;

out TrackData{
    vec3 wpos;
    uint layer;
} VertexOut;

uniform vec2 minMaxX;
uniform vec2 minMaxY;
uniform vec2 minMaxZ;

void main()
{
    VertexOut.layer = layerAttribute;
    vec3 p = positionAttribute;
    vec3 pmin = vec3(minMaxX.x, minMaxY.x, minMaxZ.x);
    vec3 pmax = vec3(minMaxX.y, minMaxY.y, minMaxZ.y);
    p = (p-pmin)/(pmax-pmin);
    p = 2*p-1;
    vec4 pos = model*vec4(p,1.0);
    VertexOut.wpos = pos.xyz;
    gl_Position = view*pos;
}

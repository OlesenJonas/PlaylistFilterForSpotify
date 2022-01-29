#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout(location = 0) uniform mat4 model;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 projection;

uniform float width;

in TrackData{
    vec3 wpos;
    uint layer;
} VertexIn[1];

out TrackData{
    vec2 uv;
    uint layer;
} VertexOut;

void main() 
{
    vec4 p = gl_in[0].gl_Position;
    vec3 absp = abs(VertexIn[0].wpos);
    if(max(max(absp.x,absp.y),absp.z) > 1.0)
    {
        return;
    }

    VertexOut.layer = VertexIn[0].layer;
    VertexOut.uv = vec2(0,1);
    gl_Position = projection*(p + width * vec4(-1, -1, 0, 0));
    EmitVertex();

    VertexOut.layer = VertexIn[0].layer;
    VertexOut.uv = vec2(1,1);
    gl_Position = projection*(p + width * vec4(1, -1, 0, 0));
    EmitVertex();

    VertexOut.layer = VertexIn[0].layer;
    VertexOut.uv = vec2(0,0);
    gl_Position = projection*(p + width * vec4(-1, 1, 0, 0));
    EmitVertex();

    VertexOut.uv = vec2(1,0);
    VertexOut.layer = VertexIn[0].layer;
    gl_Position = projection*(p + width * vec4(1, 1, 0, 0));
    EmitVertex();
    
    EndPrimitive();
}
#version 450 core

layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;

layout(location=3) uniform vec2 aspect;

vec2 toScreenSpace(vec4 v)
{
    return vec2(v.xy/v.w)*aspect;
}

void main() 
{
    vec4 pIn0 = gl_in[0].gl_Position;
    vec4 pIn1 = gl_in[1].gl_Position;
    vec2 p0 = toScreenSpace(pIn0);
    vec2 p1 = toScreenSpace(pIn1);
    float ndcZ0 = pIn0.z/pIn0.w;
    float ndcZ1 = pIn1.z/pIn1.w;

    vec2 dir = normalize(p1-p0);
    vec2 n = vec2(dir.y, -dir.x);

    float width = 0.001;

    gl_Position = vec4( (p0 + width*n)/aspect, ndcZ0, 1.0 );
    EmitVertex();

    gl_Position = vec4( (p0 - width*n)/aspect, ndcZ0, 1.0 );
    EmitVertex();

    gl_Position = vec4( (p1 + width*n)/aspect, ndcZ0, 1.0 );
    EmitVertex();

    gl_Position = vec4( (p1 - width*n)/aspect, ndcZ0, 1.0 );
    EmitVertex();
    
    EndPrimitive();
}
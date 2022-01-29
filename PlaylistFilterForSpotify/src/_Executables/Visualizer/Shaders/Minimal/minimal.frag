#version 450 core

out vec4 fragmentColor;

layout(location=4) uniform vec4 color;

void main()
{
    fragmentColor = color;
}
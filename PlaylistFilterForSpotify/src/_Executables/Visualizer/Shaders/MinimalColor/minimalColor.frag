#version 450 core

out vec4 fragmentColor;

in vec3 col;

void main()
{
    fragmentColor = vec4(col,1);
}
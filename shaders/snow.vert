#version 410 core

layout(location = 0) in vec3 vPosition;

uniform mat4 view;
uniform mat4 projection;
uniform float pointSize;

void main()
{
    gl_Position = projection * view * vec4(vPosition, 1.0);
    gl_PointSize = pointSize;
}
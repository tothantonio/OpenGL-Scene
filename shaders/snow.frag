#version 410 core

out vec4 fColor;

void main()
{
    // Create circular snowflakes
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5)
        discard;
    
    fColor = vec4(1.0, 1.0, 1.0, 0.9); // White with slight transparency
}
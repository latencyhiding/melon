#version 330

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = vec4(position, 0.0f);
}
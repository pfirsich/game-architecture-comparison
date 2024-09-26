#version 330 core

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normal;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord;
layout (location = 3) in vec3 a_color;

out vec2 v_texcoords;
out vec3 v_normal; // view space
out vec3 v_color;

void main() {
    v_texcoords = a_texcoord;
    v_normal = u_normal * a_normal;
    v_color = a_color;
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
}
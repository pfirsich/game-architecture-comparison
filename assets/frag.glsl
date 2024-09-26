#version 330 core

uniform sampler2D u_texture;
uniform vec3 u_light_dir; // view space

in vec2 v_texcoords;
in vec3 v_normal;
in vec3 v_color;

out vec4 frag_color;

void main() {
    vec4 material = texture2D(u_texture, v_texcoords) * vec4(v_color, 1.0);
    float n_dot_l = max(dot(u_light_dir, normalize(v_normal)), 0.0);
    vec3 ambient = vec3(0.2);
    frag_color = vec4(material.rgb * n_dot_l + material.rgb * ambient, material.a);
    // frag_color = frag_color * 0.0001 + vec4(vec3(material.a), 1.0);
    // fragColor = vec4(normal, 1.0);
}
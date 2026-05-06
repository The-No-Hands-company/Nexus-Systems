#version 330 core
layout(location = 0) in vec3 position;

out vec3 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main() {
    TexCoords = position;
    // Remove translation from view matrix (keep rotation only)
    mat4 viewNoTranslation = mat4(mat3(view));
    vec4 pos = projection * viewNoTranslation * vec4(position, 1.0);
    // Ensure skybox is always at max depth
    gl_Position = pos.xyww;
}

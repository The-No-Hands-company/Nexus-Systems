
#version 460
layout(location = 0) out vec2 vUv;
void main() {
    float x = float(gl_VertexIndex & 1) * 4.0 - 1.0;
    float y = float(gl_VertexIndex & 2) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    vUv = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
}

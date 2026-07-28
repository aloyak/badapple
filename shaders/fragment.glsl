#version 330 core
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_frameTexture;

void main() {
    FragColor = texture(u_frameTexture, TexCoord);
}
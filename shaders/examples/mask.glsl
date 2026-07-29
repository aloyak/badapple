// This shader is part of an "extra" set of shaders with different
// effects to the one found in shaders/fragment.glsl

// mask.glsl: You can only see the screen buffer through the mask, and the rest is blacked out

#version 330 core
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_frameTexture;
uniform sampler2D u_screenTexture;

void main() {
    vec2 flippedTexCoord = vec2(TexCoord.x, 1.0 - TexCoord.y);

    vec4 screenColor = texture(u_screenTexture, flippedTexCoord);
    float mask = texture(u_frameTexture, flippedTexCoord).r;

    if (mask > 0.5) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    FragColor = screenColor;
}

// This shader is part of an "extra" set of shaders with different
// effects to the one found in shaders/fragment.glsl

// holo.glsl: the frame is wrapped in a color and whatever is outside
// has a refracting digital glitch effect

#version 330 core
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_frameTexture;
uniform sampler2D u_screenTexture;

void main() {
    vec2 flippedTexCoord = vec2(TexCoord.x, 1.0 - TexCoord.y);

    float mask = texture(u_frameTexture, flippedTexCoord).r;
    
    float maskR = texture(u_frameTexture, flippedTexCoord + vec2(0.005, 0.0)).r;
    float maskU = texture(u_frameTexture, flippedTexCoord + vec2(0.0, 0.005)).r;
    vec2 gradient = vec2(maskR - mask, maskU - mask);
    
    vec2 warpCoord = flippedTexCoord + (gradient * 0.4);
    
    float r = texture(u_screenTexture, warpCoord + vec2(0.015 * mask, 0.0)).r;
    float g = texture(u_screenTexture, warpCoord).g;
    float b = texture(u_screenTexture, warpCoord - vec2(0.015 * mask, 0.0)).b;
    
    vec3 effectColor = vec3(r, g, b);
    
    float scanline = sin(flippedTexCoord.y * 1200.0) * 0.06;
    effectColor -= scanline * mask;
    
    effectColor *= 1.0 + (0.15 * mask);
    
    float edge = length(gradient) * 2.0;
    effectColor += vec3(0.0, 0.9, 1.0) * edge;
    
    vec4 screenColor = texture(u_screenTexture, flippedTexCoord);
    
    vec3 finalColor = mix(screenColor.rgb, effectColor, mask);
    
    FragColor = vec4(finalColor, screenColor.a);
}
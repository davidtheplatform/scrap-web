#version 330

in vec2 fragCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform float time = 0.0;

void main() {
    vec2 coord = (fragCoord + 1.0) * 0.5;
    coord.y = 1.0 - coord.y;

    float pos = time * 3.0 - 1.0;

    //float diff = clamp(1.0 - (pow(coord.x - mouse_pos.x, 2.0) + pow(coord.y - mouse_pos.y, 2.0)) * 16.0, 0.0, 1.0);
    float diff = 1.0 - (coord.x - pos) * (coord.y - pos);
    finalColor = vec4(fragColor.xyz, pow(diff, 12.0));
}

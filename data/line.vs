#version 330

in vec3 vertexPosition;
in vec4 vertexColor;
out vec2 fragCoord;
out vec4 fragColor;

uniform mat4 mvp;

void main()
{
    vec4 pos = mvp * vec4(vertexPosition, 1.0);
    fragCoord = pos.xy;
    fragColor = vertexColor;
    gl_Position = pos;
}

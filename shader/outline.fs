#version 330

in vec2 fragTexCoord;
in float fragFacing;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 outlineParam;

void main() {
    if (fragFacing > outlineParam.y) discard;
    float a = texture(texture0, fragTexCoord).a;
    if (a <= 0.0) discard;
    finalColor = vec4(0.05, 0.05, 0.05, a);
}

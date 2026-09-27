#version 300 es
precision mediump float;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matView;
uniform mat4 matNormal;
uniform vec2 outlineParam;
uniform vec2 screenSize;

out vec2 fragTexCoord;
out float fragFacing;

void main() {
    vec4 clip = mvp * vec4(vertexPosition, 1.0);
    vec3 n = normalize(mat3(matView) * mat3(matNormal) * vertexNormal);
    if (dot(n.xy, n.xy) > 1e-12) {
        vec2 px = outlineParam.x * vertexColor.g * normalize(n.xy);
        clip.xy += px * 2.0 / screenSize * clip.w;
    }
    gl_Position = clip;
    fragTexCoord = vertexTexCoord;
    fragFacing = n.z;
}

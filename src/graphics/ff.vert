precision mediump float;

attribute vec3 position;
attribute vec2 texCoords;
attribute vec4 diffuse;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 textureMatrix;
uniform vec2 invViewport;

varying vec2 interpTexCoords;
varying vec4 interpDiffuse;
varying float viewZ;

void main() {
    interpTexCoords = (textureMatrix * vec4(texCoords, 1.0, 1.0)).xy;
    interpDiffuse = diffuse;

    vec4 viewCoordinates = modelviewMatrix * vec4(position, 1.0);
    viewZ = viewCoordinates.z;

    vec4 finalPos = projectionMatrix * viewCoordinates;
    finalPos.xy += vec2(invViewport.x, -invViewport.y) * finalPos.w;
    gl_Position = finalPos;
}

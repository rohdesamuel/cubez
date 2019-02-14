#version 330 core

attribute vec3 inPos; 
attribute vec2 inTexCoord; 
attribute vec3 inNorm;

uniform mat4 uMvp;

varying vec2 vTexCoord; 

void main() { 
  vTexCoord = inTexCoord; 
  gl_Position = uMvp * vec4(inPos, 1.0); 
}


#version 330 core

uniform int uColorMode;
uniform sampler2D uTexture; 
uniform vec4 uDefaultColor;

varying vec2 vTexCoord; 
 
void main() { 
  switch (uColorMode) {
    case 1:  // Render default color.
	  gl_FragColor = uDefaultColor;
	  break;

	case 2:  // Render texture mode.
	  vec4 texColor = texture2D(uTexture, vTexCoord); 
	  gl_FragColor = texColor; 
	  break;

	default:  // Purposely don't set the '0' case and handle any errors with a 
	          // purple color.
	  gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
	  break;
  }
}

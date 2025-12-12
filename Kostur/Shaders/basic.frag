#version 330 core

in vec2 chTex;
out vec4 outCol;

uniform sampler2D uTex;
uniform vec4 uColor;

void main()
{
    vec4 texColor = texture(uTex, chTex);
    
    // Ako nema teksture (crna tekstura), koristi samo uColor
    if (texColor.r == 0.0 && texColor.g == 0.0 && texColor.b == 0.0 && texColor.a == 0.0) {
        outCol = uColor;
    } 
    // Ako je uColor bela (1,1,1,1), samo prikaži teksturu BEZ množenja
    else if (uColor.r >= 0.99 && uColor.g >= 0.99 && uColor.b >= 0.99 && uColor.a >= 0.99) {
        outCol = texColor; // NE množi, samo prikaži teksturu!
    }
    // Za tinting (npr. pljeskavica koja menja boju)
    else {
        outCol = texColor * uColor;
    }
}
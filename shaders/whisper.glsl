// https://www.shadertoy.com/view/XcdczX
// title: Ethereal Whispers 2
// Made with Hatch.one
// License: MIT

/* Hatch uniforms
uniform float pulseRate; // default: 0.5, min: 0.1, max: 2.0, step: 0.1, title: "Ethereal Pulse"
uniform float voidDepth; // default: 0.7, min: 0.1, max: 2.0, step: 0.1, title: "Void Depth"
uniform float mysteryLevel; // default: 0.6, min: 0.0, max: 1.0, step: 0.01, title: "Mystery Level"
uniform float spiritFlow; // default: 1.0, min: 0.1, max: 3.0, step: 0.1, title: "Spirit Flow"
uniform vec4 auraColor1; // default: #330066, type: Color, title: "Deep Aura"
uniform vec4 auraColor2; // default: #6600ff, type: Color, title: "Spirit Aura"
*/

#define pulseRate 0.5
#define voidDepth 0.9
#define mysteryLevel 0.6
#define spiritFlow 1.0
#define auraColor1 vec4(0.2, 0.0, 0.4, 1.0)
#define auraColor2 vec4(0.4, 0.0, 1.0, 1.0)

float getEtherealField(vec2 uv) {
    // Create a flowing, organic pattern
    float time = iTime * pulseRate;
    vec2 moved = uv + vec2(
        sin(time * 0.5 + uv.y * 4.0) * 0.1,
        cos(time * 0.7 + uv.x * 4.0) * 0.1
    );
    
    // Multiple layers of ethereal waves
    float spirit = sin(moved.x * 6.0 + time) * cos(moved.y * 6.0 + time);
    spirit += sin(length(moved * 8.0 + sin(time * 0.5)) * 4.0) * 0.5;
    spirit += sin(length(moved * 4.0 - cos(time * 0.7)) * 3.0) * 0.25;
    
    // Add mouse influence as mysterious force
    vec2 mouse = iMouse.xy / iResolution.xy;
    float distToMouse = length(uv - mouse);
    float mouseForce = sin(distToMouse * 10.0 - time * 2.0) * exp(-distToMouse * 3.0);
    
    return spirit * 0.5 + mouseForce * mysteryLevel;
}

vec3 getNormal(vec2 uv, float field) {
    vec2 e = vec2(0.01, 0.0);
    float dx = getEtherealField(uv + e.xy) - getEtherealField(uv - e.xy);
    float dy = getEtherealField(uv + e.yx) - getEtherealField(uv - e.yx);
    return normalize(vec3(-dx, -dy, e.x * 2.0));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    
    // Get the ethereal field value
    float field = getEtherealField(uv) * voidDepth;
    
    // Calculate ethereal lighting
    vec3 normal = getNormal(uv, field);
    vec3 lightDir = normalize(vec3(
        sin(iTime * spiritFlow) * 0.5,
        cos(iTime * spiritFlow * 0.7) * 0.5,
        1.0
    ));
    
    // Create mysterious lighting effect
    float diffuse = max(dot(normal, lightDir), 0.0);
    float glow = exp(-length(uv - vec2(0.5)) * 2.0);
    
    // Mix colors based on field value and lighting
    vec3 color1 = auraColor1.rgb;
    vec3 color2 = auraColor2.rgb;
    vec3 finalColor = mix(color1, color2, field * 0.5 + 0.5);
    
    // Add ethereal glow and lighting
    finalColor += vec3(diffuse * 0.8);
    finalColor += vec3(glow * 0.2) * color2;
    
    // Add mysterious shimmer
    float shimmer = sin(iTime * 5.0 + field * 10.0) * 0.1 + 0.9;
    finalColor *= shimmer;
    
    // Add depth-based transparency for ethereal effect
    float alpha = 0.8 + field * 0.2;
    
    fragColor = vec4(finalColor, alpha);
}


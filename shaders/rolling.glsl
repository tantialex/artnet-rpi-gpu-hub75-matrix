// Infection parameters
const float VIRUS_SPEED = 0.1;
const float INFECTION_SPREAD = 4.0;
const float REALITY_FOLD_ITERATIONS = 4.0;
const float DNA_MUTATION_RATE = 8.0;
const vec3 VIRUS_COLOR = vec3(0.9, 0.2, 0.3);

// Helper for viral DNA patterns
vec2 viralDNA(vec2 uv, float time) {
    vec2 dna = uv;
    float t = time * VIRUS_SPEED;
    
    for(float i = 0.0; i < 5.0; i++) {
        dna = abs(dna) / dot(dna,dna) - DNA_MUTATION_RATE;
        dna *= mat2(cos(t), sin(t), -sin(t), cos(t));
        dna += vec2(sin(t * 0.7), cos(t * 0.8)) * 0.2;
    }
    return dna;
}

// Viral growth pattern
float viralGrowth(vec2 p, float time) {
    float growth = 0.0;
    p *= 1.0;
    
    for(float i = 0.0; i < 6.0; i++) {
        p = abs(p) / dot(p,p) - 1.0;
        p = p * mat2(cos(time), sin(time), -sin(time), cos(time));
        growth += exp(-length(p) * 4.0);
    }
    return growth / 6.0;
}

// Reality fold with infection
vec3 foldReality(vec2 uv, float time) {
    vec2 p = uv;
    vec3 infected = vec3(0.0);
    float total = 0.0;
    
    for(float i = 0.0; i < REALITY_FOLD_ITERATIONS; i++) {
        // Mutate space
        p = viralDNA(p, time + i);
        
        // Sample infected reality
        vec2 samplePoint = fract(p * 0.5 + 0.5);
        vec3 texColor = texture(iChannel0, samplePoint).rgb;
        
        // Calculate infection strength
        float infection = viralGrowth(p, time - i);
        float weight = exp(-i * 0.3);
        
        // Infect texColor
        vec3 infectedColor = mix(texColor, VIRUS_COLOR, infection);
        infected += infectedColor * weight;
        total += weight;
    }
    
    return infected / total;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    vec2 texUV = fragCoord/iResolution.xy;
    
    // Get base reality
    vec3 reality = texture(iChannel0, texUV).rgb;
    
    // Generate viral infection
    vec2 dna = viralDNA(uv, iTime);
    float growth = viralGrowth(uv, iTime);
    
    // Create infection spread
    vec2 infected_uv = texUV + dna * growth * INFECTION_SPREAD;
    vec3 infected_reality = foldReality(uv, iTime);
    
    // Viral pulse
    float pulse = sin(length(uv) * 10.0 - iTime * 2.0) * 0.5 + 0.5;
    pulse *= growth;
    
    // Create reality tears
    float tear = length(dna) * 0.5;
    vec3 tear_color = vec3(1.0, 0.2, 0.3) * pulse;
    
    // Combine effects
    vec3 final = mix(reality, infected_reality, growth);
    final += tear_color * tear;
    
    // Add viral glow
    final += VIRUS_COLOR * pulse * 0.3;
    
    // Reality decomposition
    final *= 1.0 + growth * sin(iTime * 5.0) * 0.2;
    
    fragColor = vec4(final, 1.0);
}

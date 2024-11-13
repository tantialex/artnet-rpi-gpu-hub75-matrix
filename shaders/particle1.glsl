// Random function
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

// Returns velocity vector for a particle
vec2 getParticleVelocity(float index, float seed) {
    float angle = random(vec2(index, seed)) * 6.28; // Random angle
    
    // Base speed reduced by 30% with wider variation
    float baseSpeed = 0.56;  // 0.8 * 0.7 = 0.56
    float speedVariation = random(vec2(index, seed + 1.0)) * 0.6 + 0.7; // 0.7 to 1.3 multiplier
    float speed = baseSpeed * speedVariation;
    
    return vec2(cos(angle), sin(angle)) * speed;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalize coordinates
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    // Create explosion seed based on time
    float seed = floor(iTime);
    float t = fract(iTime);
    
    // Initialize color
    vec3 color = vec3(0.0);
    
    // Number of particles
    const int NUM_PARTICLES = 25;
    
    // Gravity effect (pulls particles down over time)
    float gravity = 0.8;
    
    // Generate particles
    for(int i = 0; i < NUM_PARTICLES; i++) {
        float fi = float(i);
        
        // Get random velocity for this particle
        vec2 velocity = getParticleVelocity(fi, seed);
        
        // Apply gravity effect to particle position
        vec2 particlePos = velocity * t * 2.0;
        particlePos.y -= gravity * t * t; // Quadratic fall-off for gravity
        
        // Distance from current pixel to particle
        float dist = length(uv - particlePos);
        
        // Particle size shrinks over time
        float size = 0.05 * (1.0 - t * 0.5);
        
        // Create soft particles with falloff
        float brightness = smoothstep(size, 0.0, dist);
        
        // Fade out over time
        brightness *= 1.0 - t;
        
        // Random color variation for each particle
        vec3 particleColor = mix(
            vec3(1.0, 0.3, 0.1), // Orange/red core
            vec3(1.0, 0.8, 0.3), // Yellow/white hot
            random(vec2(fi, seed + 2.0))
        );
        
        // Add this particle's contribution
        color += particleColor * brightness;
    }
    
    // Add glow effect
    color *= 1.2;
    
    // Output final color
    fragColor = vec4(color, 1.0);
}

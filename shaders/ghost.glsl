// https://www.shadertoy.com/view/Mc3yRs
// Funzione di supporto per smooth min
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalizza le coordinate
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    // Aggiungi un movimento più complesso e variabile
    float time = iTime * 0.5;
    vec2 ghostPos = vec2(
        cos(time * 1.2) * 0.4 + sin(time * 0.9) * 0.1,
        sin(time * 1.1) * 0.4 + cos(time * 0.7) * 0.15
    );
    
    // Aggiungi distorsione ondulata
    uv -= ghostPos;
    uv.x += sin(uv.y * 10.0 + time * 2.0) * 0.02;
    uv.y += cos(uv.x * 8.0 + time * 1.5) * 0.015;
    
    // Forma base più complessa
    float ghost = length(uv - vec2(0.0, 0.1)) - 0.22;
    
    // Corpo più elaborato con multiple onde
    float bodyWave = sin(uv.x * 12.0 + time * 2.5) * 0.03 +
                     sin(uv.x * 16.0 + time * 1.8) * 0.02;
    float bodyPulse = sin(time * 1.8) * 0.02;
    float body = length(vec2(uv.x * (1.0 + sin(uv.y * 4.0) * 0.12), 
                            uv.y + 0.12 + bodyWave + bodyPulse)) - 0.22;
    
    // Sfuma il collegamento tra testa e corpo
    ghost = smin(ghost, body, 0.12);
    
    // Occhi più elaborati con movimenti
    vec2 eyeUV = uv - vec2(-0.06, 0.13);
    float leftEye = length(eyeUV) - 0.035 - sin(time * 3.0) * 0.005;
    float leftPupil = length(eyeUV - vec2(0.01, -0.01 - sin(time * 4.0) * 0.003)) - 0.015;
    
    eyeUV = uv - vec2(0.06, 0.13);
    float rightEye = length(eyeUV) - 0.035 - sin(time * 3.2) * 0.005;
    float rightPupil = length(eyeUV - vec2(-0.01, -0.01 - sin(time * 4.2) * 0.003)) - 0.015;
    
    // Bocca ovale nera con movimento ondulante
    vec2 mouthUV = uv - vec2(0.0, 0.05); 
    float mouth = length(vec2(mouthUV.x * 1.5 + sin(time * 2.8) * 0.01, mouthUV.y)) - 0.02;
    
    // Effetto nebuloso intorno al fantasma
    float fog = 0.0;
    for(int i = 0; i < 8; i++) {
        float angle = float(i) * 3.14159 * 2.0 / 8.0;
        vec2 offset = vec2(cos(angle), sin(angle)) * 0.22;
        fog += smoothstep(0.22, 0.0, length(uv - offset * (0.5 + sin(iTime + float(i)) * 0.25)));
    }
    
    // Parte inferiore più elaborata e nebulosa
    float bottom = -1.0;
    for(int i = 0; i < 7; i++) {
        float x = float(i) * 0.08 - 0.24;
        float wave = sin(iTime * 1.8 + float(i) * 3.14) * 0.05;
        float size = 0.07 + sin(iTime + float(i)) * 0.012;
        bottom = max(bottom, smoothstep(0.035, 0.0, length(uv - vec2(x, -0.18 + wave)) - size));
    }
    
    // Effetti di luce e ombra
    vec3 col = vec3(0.0);
    float ghostShape = smoothstep(0.01, -0.01, ghost);
    ghostShape = max(ghostShape, bottom);
    
    // Gradiente di colore per effetto spettrale
    vec3 ghostColor = mix(
        vec3(0.7, 0.78, 1.0),  // Blu più chiaro
        vec3(0.95, 0.95, 1.0), // Bianco spettrale
        uv.y + 0.5
    );
    
    // Aggiungi luminescenza interna
    float innerGlow = smoothstep(0.35, 0.0, ghost) * 0.6;
    ghostColor += vec3(0.25, 0.35, 0.45) * innerGlow;
    
    // Applica il colore base del fantasma
    col += ghostColor * (ghostShape + fog * 0.24);
    
    // Aggiungi gli occhi e la bocca
    col *= 1.0 - smoothstep(0.01, -0.01, leftEye) * 0.85;
    col *= 1.0 - smoothstep(0.01, -0.01, rightEye) * 0.85;
    col *= 1.0 - smoothstep(0.01, -0.01, mouth) * 1.0;
    
    // Aggiungi le pupille
    col *= 1.0 - smoothstep(0.01, -0.01, leftPupil);
    col *= 1.0 - smoothstep(0.01, -0.01, rightPupil);
    
    // Effetto di bagliore
    float glow = smoothstep(0.35, 0.22, ghost) * 0.8;
    col += vec3(0.45, 0.55, 0.8) * glow * (0.6 + sin(iTime) * 0.15);
    
    // Sfondo con nebbia atmosferica
    vec3 bgColor = vec3(0.08, 0.1, 0.13);
    bgColor += fog * vec3(0.06, 0.08, 0.12);
    
    // Miscela il fantasma con lo sfondo
    float alpha = ghostShape * (0.85 + fog * 0.15);
    col = mix(bgColor, col, alpha);
    
    fragColor = vec4(col, 1.0);
}

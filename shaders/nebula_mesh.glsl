// https://www.shadertoy.com/view/M3SfRG

const float FLIGHT_SPEED = 6.0;

const float FIELD_OF_VIEW = 1.05;


vec3 getRayDirection(vec2 fragCoord, vec3 cameraDirection) {
    vec2 uv = fragCoord.xy / iResolution.xy;
  
    const float screenWidth = 1.0;
    float originToScreen = screenWidth / 2.0 / tan(FIELD_OF_VIEW / 2.0);
    
    vec3 screenCenter = originToScreen * cameraDirection;
    vec3 baseX = normalize(cross(screenCenter, vec3(0, -1.0, 0)));
    vec3 baseY = normalize(cross(screenCenter, baseX));
    
    return normalize(screenCenter + (uv.x - 0.5) * baseX + (uv.y - 0.5) * iResolution.y / iResolution.x * baseY);
}

vec4 getNebulaColor(vec3 globalPosition, vec3 rayDirection) {
    vec3 color = vec3(0.0);
    float spaceLeft = 1.0;
    
    const float layerDistance = 10.0;
    float rayLayerStep = rayDirection.z / layerDistance;
    
    const int steps = 4;
    for (int i = 0; i <= steps; i++) {
      vec3 noiseeval = globalPosition + rayDirection * ((1.0 - fract(globalPosition.z / layerDistance) + float(i)) * layerDistance / rayDirection.z);
      noiseeval.xy += noiseeval.z;
        
        
        float value = 0.06 * texture(iChannel0, fract(noiseeval.xy / 60.0)).r;
         
        if (i == 0) {
            value *= 1.0 - fract(globalPosition.z / layerDistance);
        } else if (i == steps) {
            value *= fract(globalPosition.z / layerDistance);
        }
                
        color += spaceLeft * 2. * vec3(value, value, value) * vec3(.1, .5, 0.3);
        spaceLeft = max(0.0, spaceLeft - value * 2.0);
    }
    return vec4(color, 1.0);
}

#define S(a, b, t) smoothstep(a, b, t)

float distLine(vec2 p, vec2 a, vec2 b){
  vec2 pa = p - a;
    vec2 ba = b - a;
    float t = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*t);
}

float line(vec2 p, vec2 a, vec2 b){
  float d = distLine(p, a, b);
    float m = S(0.03, 0.01, d);
    float d2 =  length(a - b);
    m *= S(1.2, 0.8, d2) * 0.5 + S(0.05, 0.03, abs(d2 - 0.75));
    return m;
}

float distTriangle(in vec2 p, in vec2 p0, in vec2 p1, in vec2 p2 )
{
  vec2 e0 = p1 - p0;
  vec2 e1 = p2 - p1;
  vec2 e2 = p0 - p2;

  vec2 v0 = p - p0;
  vec2 v1 = p - p1;
  vec2 v2 = p - p2;

  vec2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
  vec2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
  vec2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
    
    float s = sign( e0.x*e2.y - e0.y*e2.x );
    vec2 d = min( min( vec2( dot( pq0, pq0 ), s*(v0.x*e0.y-v0.y*e0.x) ),
                       vec2( dot( pq1, pq1 ), s*(v1.x*e1.y-v1.y*e1.x) )),
                       vec2( dot( pq2, pq2 ), s*(v2.x*e2.y-v2.y*e2.x) ));

  return -sqrt(d.x)*sign(d.y);
}

float triangle(vec2 p, vec2 a, vec2 b, vec2 c){
  float d = distTriangle(p, a, b, c);
    float m = S(0.03, 0.01, d);
    float d2 =  length(a - b);
    m *= S(1.2, 0.8, d2) * 0.5 + S(0.05, 0.03, abs(d2 - 0.75));
    return m;
}

float N21(vec2 p){
  p = fract(p * vec2(233.34, 851.73));
    p += dot(p, p + 23.45);
    return fract(p.x * p.y);
}

vec2 N22(vec2 p){
  float n = N21(p);
    return vec2(n, N21(p + n));
}

vec2 getPos(vec2 id, vec2 offset){
    vec2 n = N22(id + offset) * iTime;
    return offset + sin(n) * 0.4;
}

float layer(vec2 uv){
  vec2 gv = fract(uv) - 0.5;
    vec2 id = floor(uv);
    vec2 p[9];
    int i = 0;
    for(float y = -1.0; y <= 1.0; y++){
      for(float x = -1.0; x <= 1.0; x++){
          p[i++] = getPos(id, vec2(x, y));
      }    
    }
    
    
    float t = iTime * 10.0;
    float m = 0.0;
    for(int i = 0; i < 9; i++){
      m += line(gv, p[4], p[i]);
        
        vec2 j = (p[i] - gv) * 20.0;
        float sparkle = 1.0 / dot(j, j);
        
        m += sparkle * (sin(t + fract(p[i].x) * 10.0) * 0.5 + 0.5);
        
        for(int yi= i + 1; yi < 9; yi++){
        for(int zi= yi + 1; zi < 9; zi++){
                
                float len1 = abs(length(p[i] - p[yi]));
                float len2 = abs(length(p[yi] - p[zi]));
                float len3 = abs(length(p[i] - p[zi]));
                
                if((len1 + len2 + len3) < 2.8){
                  m += triangle(gv, p[i], p[yi], p[zi]) * 0.8;
                }
        }
      }
    }
    m += line(gv, p[1], p[3]);
    m += line(gv, p[1], p[5]);
    m += line(gv, p[7], p[3]);
    m += line(gv, p[7], p[5]);

    return m;
}


void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    //nebula
    vec3 movementDirection = normalize(vec3(0.01, 0.0, 1.0));
    
    vec3 rayDirection = getRayDirection(fragCoord, movementDirection);
    
    vec3 globalPosition = vec3(3.14159, 3.14159, 0.0) + (iTime + 1000.0) * FLIGHT_SPEED * movementDirection;
    
    fragColor = getNebulaColor(globalPosition, rayDirection);
    
    // plexus
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    
    float m = 0.0;
    float t = iTime * 0.1;
        
    for(float i = 0.0; i < 1.0; i += 1.0 / 4.0){
        float z = fract(i + t);
        float size = mix(10.0, 0.5, z);
        float fade = S(0.0, 0.1, z) * S(1.0, 0.8, z);
        
        m += layer(uv * size + i * 20.0) * fade;
    }
    
    
    vec3 base = vec3(0.1, 0.5, 0.3);
    vec3 col = m * base * 0.1;
    
    col -= uv.y * 0.5 * base;
        
    fragColor += vec4(col,1.0);
}

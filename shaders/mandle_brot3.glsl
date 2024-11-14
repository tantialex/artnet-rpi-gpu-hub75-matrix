// https://www.shadertoy.com/view/WljyDw
#define TAU 6.28318

#define NUM_COLOR 50.0
#define MAX_ITER 500

int mandlebrot( vec2 c ) {
    
    int i = 0;
 	vec2 z = vec2(0);
    
    for (int i = 0; i < MAX_ITER; i++) {
        z = vec2(z.x*z.x-z.y*z.y, 2.0*z.x*z.y) + c;
        if (length(z) >= 2.0) { return i; }
    }
    
    return -1;
    
}

#define RAINBOW 1
#define COVFEFE 0
#define ZEBRA 1

vec3 color( int scheme, float x ) {
    
    if (scheme == RAINBOW) {
        return sqrt((sin(TAU*(x+vec3(0,1,2)/3.))+1.0)/2.0);
    }
    
    if (scheme == COVFEFE) {
        return 0.5 + 0.5*cos(2.7+x*TAU + vec3(0.0,.6,1.0));
    }
    
    if (scheme == ZEBRA) {
		return 0.5 + 0.5*cos(vec3(x*TAU*NUM_COLOR*0.5));
    }
    
}

vec3 simulate( vec2 coord ) {
    
	int iterations = mandlebrot(coord);
    
    if (iterations < 0) {
        return vec3(0);
    } else {
        return color(ZEBRA, mod(float(iterations), NUM_COLOR) / NUM_COLOR);
    }
    
}

// Frame Coordinates: (zoom, lowerLeft, upperRight, origin)
#define ZR 1.5
#define LL vec2(-2.0, -1.0)
#define UR vec2(1.0, 1.0)
#define OR vec2(0.001643721971153, 0.822467633298876)

vec2 map( vec2 fragCoord, vec2 resolution, float time ) {
    
    vec2 lowerLeft = (LL-OR)*pow(ZR,-time)+OR;
    vec2 upperRight = (UR-OR)*pow(ZR,-time)+OR;
    
    return lowerLeft + (upperRight - lowerLeft) * fragCoord/resolution;
    
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    vec3 avgColor = (
        simulate(map(fragCoord, iResolution.xy, iTime))
        + simulate(map(fragCoord + vec2(0, 0.5), iResolution.xy, iTime))
        + simulate(map(fragCoord + vec2(0.5, 0), iResolution.xy, iTime))
        + simulate(map(fragCoord + vec2(0.5, 0.5), iResolution.xy, iTime))
    ) / 4.0;
        
    fragColor = vec4(avgColor, 1.0);
    
}


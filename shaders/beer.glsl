// https://www.shadertoy.com/view/WtlGzS
// Add more life to previous shader https://www.shadertoy.com/view/WllGRn
// Very similar to https://www.shadertoy.com/view/Wll3RS
    
// Force range [.1, .3]
#define FORCE .35
#define INIT_SPEED 14.
#define AMOUNT 8.
// #define WATER_COL vec3(18,140,200)/255.
#define WATER_COL vec3(135,123,3)/255.

float rand(vec2 co) {
    return fract(sin(dot(co.xy , vec2(12.9898, 78.233))) * 43758.5453);
}

float bubbles( vec2 uv, float size, float speed, float timeOfst, float blur, float time)
{
    vec2 ruv = uv*size  + .05;
    vec2 id = ceil(ruv) + speed;

    float t = (time + timeOfst)*speed;

    ruv.y -= t * (rand(vec2(id.x))*0.5+.5)*.1;
    vec2 guv = fract(ruv) - 0.5;

    ruv = ceil(ruv);
    float g = length(guv);

    float v = rand(ruv)*0.5;
    v *= step(v, clamp(FORCE, .1, .3));

    float m = smoothstep(v,v - blur, g);

    v*=.85;
    m -= smoothstep(v,v- .1, g);

    g = length(guv - vec2(v*.35, v*.35));
    float hlSize = v*.75;
    m += smoothstep(hlSize, 0., g)*.75;

    return m;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = (fragCoord - .5*iResolution.xy)/iResolution.y;

    float m = 0.;

    float sizeFactor = iResolution.y / 10.;

    float fstep = .1/AMOUNT;
    for(float i=-1.0; i<=0.; i+=fstep){
        vec2 iuv = uv + vec2(cos(uv.y*2. + i*20. + iTime*.5)*.1, 0.);
        float size = (i*.15+0.2) * sizeFactor + 2.;
        m += bubbles(iuv + vec2(i*.1, 0.), size, INIT_SPEED + i*5., i*10., .3 + i*.25, iTime) * abs(i);
    }

    vec3 col = WATER_COL + m*.4;

    fragColor = vec4(col,1.0);
}

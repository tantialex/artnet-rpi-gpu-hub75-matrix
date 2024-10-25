// https://www.shadertoy.com/view/XXjBzV


#define iterations 10
#define formuparam 0.53

#define volsteps 20
#define stepsize 0.1

#define zoom   0.800
#define tile   0.850
#define speed  0.000 

#define brightness 0.0015
#define darkmatter 0.300
#define distfading 0.730
#define saturation 0.850


void mainVR( out vec4 fragColor, in vec2 fragCoord, in vec3 ro, in vec3 rd )
{
	//get coords and direction
	vec3 dir=rd;
	vec3 from=ro;
	
	//volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	for (int r=0; r<volsteps; r++) {
		vec3 p=from+s*dir*.5;
		p = abs(vec3(tile)-mod(p,vec3(tile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<iterations; i++) { 
			p=abs(p)/dot(p,p)-formuparam;
            p.xy*=mat2(cos(iTime*0.01), sin(iTime*0.01),-sin(iTime*0.01), cos(iTime*0.01));// the magic formula
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,darkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.1-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*brightness*fade; // coloring based on distance
		fade*=distfading; // distance fading
		s+=stepsize;
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	fragColor = vec4(v*.03,1.);	
}
#define NUM_LAYERS 10.
#define TAU 6.28318
#define PI 3.141592
#define Velocity .025 //modified value to increse or decrease speed, negative value travel backwards
#define StarGlow 0.025
#define StarSize 02.
#define CanvasView 20.

float Star(vec2 uv, float flare){
    float d = length(uv);
  	float m = sin(StarGlow*1.2)/d;  
    float rays = max(0., .5-abs(uv.x*uv.y*1000.)); 
    m += (rays*flare)*2.;
    m *= smoothstep(1., .1, d);
    return m;
}

float Hash21(vec2 p){
    p = fract(p*vec2(123.34, 456.21));
    p += dot(p, p+45.32);
    return fract(p.x*p.y);
}


vec3 StarLayer(vec2 uv){
    vec3 col = vec3(0);
    	
    vec2 gv = fract(uv);
    vec2 id = floor(uv);
    for(int y=-1;y<=1;y++){
        for(int x=-1; x<=1; x++){
            vec2 offs = vec2(x,y);
            float n = Hash21(id+offs);
            float size = fract(n);
            float star = Star(gv-offs-vec2(n, fract(n*34.))+.5, smoothstep(.1,.9,size)*.46);
            vec3 color = sin(vec3(.2,.3,.9)*fract(n*2345.2)*TAU)*.25+.75;
            color = color*vec3(.5,.5,.5+size);
            star *= sin(iTime*.6+n*TAU)*.5+.5;
            col += star*size*color;
        }
    }
    return col;
}

#define M_PI 3.14159265359



vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
    return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v)
{ 
    const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
    const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i  = floor(v + dot(v, C.yyy) );
    vec3 x0 =   v - i + dot(i, C.xxx) ;

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min( g.xyz, l.zxy );
    vec3 i2 = max( g.xyz, l.zxy );

    //   x0 = x0 - 0.0 + 0.0 * C.xxx;
    //   x1 = x0 - i1  + 1.0 * C.xxx;
    //   x2 = x0 - i2  + 2.0 * C.xxx;
    //   x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
    vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

    // Permutations
    i = mod289(i); 
    vec4 p = 
        permute
        (
            permute
            ( 
                permute
                (
                    i.z + vec4(0.0, i1.z, i2.z, 1.0)
                )
                + i.y + vec4(0.0, i1.y, i2.y, 1.0 )
            )
            + i.x + vec4(0.0, i1.x, i2.x, 1.0 )
        );

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_ = 0.142857142857; // 1.0/7.0
    vec3  ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

    vec4 x = x_ *ns.x + ns.yyyy;
    vec4 y = y_ *ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4( x.xy, y.xy );
    vec4 b1 = vec4( x.zw, y.zw );

    //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
    //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
    vec4 s0 = floor(b0)*2.0 + 1.0;
    vec4 s1 = floor(b1)*2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
    vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

    vec3 p0 = vec3(a0.xy,h.x);
    vec3 p1 = vec3(a0.zw,h.y);
    vec3 p2 = vec3(a1.xy,h.z);
    vec3 p3 = vec3(a1.zw,h.w);

    //Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    vec4 m = max(0.5 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3) ) );
}

// p: position
// o: how many layers
// f: frequency
// lac: how fast frequency changes between layers
// r: how fast amplitude changes between layers
float fbm4(vec3 p, float theta, float f, float lac, float r)
{
    mat3 mtx = mat3(
        cos(theta), -sin(theta), 0.0,
        sin(theta), cos(theta), 0.0,
        0.0, 0.0, 1.0);

    float frequency = f;
    float lacunarity = lac;
    float roughness = r;
    float amp = 1.0;
    float total_amp = 0.0;

    float accum = 0.0;
    vec3 X = p * frequency;
    for(int i = 0; i < 4; i++)
    {
        accum += amp * snoise(X);
        X *= (lacunarity + (snoise(X) + 0.1) * 0.006);
        X = mtx * X;

        total_amp += amp;
        amp *= roughness;
    }

    return accum / total_amp;
}

float fbm8(vec3 p, float theta, float f, float lac, float r)
{
    mat3 mtx = mat3(
        cos(theta), -sin(theta), 0.0,
        sin(theta), cos(theta), 0.0,
        0.0, 0.0, 1.0);

    float frequency = f;
    float lacunarity = lac;
    float roughness = r;
    float amp = 1.0;
    float total_amp = 0.0;

    float accum = 0.0;
    vec3 X = p * frequency;
    for(int i = 0; i < 8; i++)
    {
        accum += amp * snoise(X);
        X *= (lacunarity + (snoise(X) + 0.1) * 0.006);
        X = mtx * X;

        total_amp += amp;
        amp *= roughness;
    }

    return accum / total_amp;
}

float turbulence(float val)
{
    float n = 1.0 - abs(val);
    return n * n;
}

float pattern(in vec3 p, inout vec3 q, inout vec3 r)
{
    q.x = fbm4( p + 0.0, 0.0, 1.0, 2.0, 0.33 );
    q.y = fbm4( p + 2.0, 0.0, 1.0, 2.0, 0.33 );

    r.x = fbm8( p + q - 2.4, 0.0, 1.0, 3.0, 0.5 );
    r.y = fbm8( p + q + 8.2, 0.0, 1.0, 3.0, 0.5 );

    q.x = turbulence( q.x );
    q.y = turbulence( q.y );

    float f = fbm4( p + (1.0 * r), 0.0, 1.0, 2.0, 0.5);

    return f;
}
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	//get coords and direction
	vec2 uv=fragCoord.xy/iResolution.xy-.5;
    
	uv.y*=iResolution.y/iResolution.x;
	vec3 dir=vec3(uv*zoom,tan(iTime*0.031));
	float time=iTime*speed+.25;
  // pixel position normalised to [-1, 1]
	vec2 cPos = -1.0 + 2.0 * fragCoord.xy / iResolution.xy;
vec4 o= fragColor;
vec2 F =fragCoord;

vec2 st = fragCoord.xy / iResolution.xy;
    float aspect = iResolution.x / iResolution.y;
    st.x *= aspect;

    vec2 uv2 = st;

    float t = iTime * 0.11;

    vec3 spectrum[4];
    spectrum[0] = vec3(0.94, 0.02, 0.03);
    spectrum[1] = vec3(0.04, 0.04, 0.22);
    spectrum[2] = vec3(0.00, 0.80, 1.00);
    spectrum[3] = vec3(0.20, 0.40, 0.50);

    uv -= 0.5;
    uv *= 5.5;

    vec3 p = vec3(uv.x, uv.y, t);
   
    vec3 q = vec3(0.0);
    vec3 r = vec3(0.0);
    float f = pattern(p, q, r);

    vec3 color = vec3(0.0);
    color = mix(spectrum[1], spectrum[3], pow(length(q), 4.0));
    color = mix(color, spectrum[0], pow(length(r), 1.4));
    color = mix(color, spectrum[2], f);

    color = pow(color, vec3(2.0));

vec2 R = iResolution.xy; 
    o-=o;
    for(float d,t = iTime*.001, i = 0. ; i > -1.; i -= .06 )          	
    {   d = fract( i -3.*t );                                          	
        vec4 c = vec4( ( F - R *.5 ) / R.y *d ,i,0 ) * 28.;            	
        for (int j=0 ; j++ <27; )                                      	
            c.xzyw = abs( c / dot(c,c)                                 	
                    -vec4( 7.-.2*sin(t) , 6.3 , .7 , 1.-cos(t/.8))/7.);	
       o -= c * c.yzww  * d--*d  / vec4(3,5,1,1);                     
    }
    // distance of current pixel from center
	float cLength = length(cPos);

	
	vec2 M = vec2(0);
    M -= vec2(M.x+sin(iTime*0.22), M.y-cos(iTime*0.22));
    M +=(iMouse.xy-iResolution.xy*.5)/iResolution.y;
    float t2 = iTime*Velocity; 
    vec3 col = vec3(0);  
    for(float i=0.; i<1.; i+=1.0/NUM_LAYERS){
        float depth = fract(i+t2);
        float scale = mix(CanvasView, .5, depth);
        float fade = depth*smoothstep(1.,.9,depth);
        col += StarLayer(uv*scale+i*455.2-iTime*.05+M)*fade;}   
    float s = .1;

    float speed2= 1.1;
    float b = .037;
    vec4 noise = texture(iChannel0, uv*s);
    
     dir.xy*= vec2(uv.x + cos(iTime*speed2+uv.x*speed2)*b*noise.r*noise.a, uv.y + sin(iTime*speed2+uv.y*speed2)*b*noise.g*noise.b);

    
	
	vec3 from=vec3(1.,.5,0.5)*col+o.xyz*+color*2.;

	
	mainVR(fragColor, fragCoord, from, dir);
    fragColor*=vec4(col*10.+color*vec3(0.5,0.5,1.5)*2.,1.);
    
}


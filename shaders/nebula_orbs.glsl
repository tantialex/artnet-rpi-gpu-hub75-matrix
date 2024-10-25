// https://www.shadertoy.com/view/4XjfRy

#define iterations 13
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
            p.xy*=mat2(cos(iTime*0.05),sin(iTime*0.05), -sin(iTime*0.05),cos(iTime*0.05));// the magic formula
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,darkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.2-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*brightness*fade; // coloring based on distance
		fade*=distfading; // distance fading
		s+=stepsize;
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	fragColor = vec4(v*.03,1.);	
}
#define time iTime
#define resolution iResolution.xy



mat3 rotX(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat3(
        1, 0, 0,
        0, c, -s,
        0, s, c
    );
}

mat3 rotY(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat3(
        c, 0, -s,
        0, 1, 0,
        s, 0, c
    );
}

float random(vec2 pos) {
    return fract(sin(dot(pos.xy, vec2(13.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 pos) {
    vec2 i = floor(pos);
    vec2 f = fract(pos);
    float a = random(i + vec2(0.0, 0.0));
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}
#define NUM_OCTAVES 6
float fbm(vec2 pos) {
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int i = 0; i < NUM_OCTAVES; i++) {
        float dir = mod(float(i), 2.0) > 0.5 ? 1.0 : -1.0;
        v += a * noise(pos - 0.05 * dir * time);

        pos = rot * pos * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

const float PI = 3.1415;
const float TWOPI = 2.0*PI;
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	//get coords and direction
	vec2 uv=fragCoord.xy/iResolution.xy-.5;
	uv.y*=iResolution.y/iResolution.x;
	vec3 dir=vec3(uv*zoom,1.);
;
vec2 uPos = ( gl_FragCoord.xy / resolution.y );//normalize wrt y axis
	uPos -= vec2((resolution.x/resolution.y)/2.0, 0.5);//shift origin to center
	
	float multiplier = 0.0005; // Grosseur
	const float step = 0.006; //segmentation
	const float loop = 80.0; //Longueur
	const float timeSCale = 0.5; // Vitesse
	
	vec3 blueGodColor = vec3(0.0);
	for(float i=1.0;i<loop;i++){		
		float t = time*0.1*timeSCale-step*i*i;
		vec2 point = vec2(0.75*sin(t), 0.5*sin(t));
		point += vec2(0.75*cos(t*4.0), 0.5*sin(t*3.0));
		point /= 11. * sin(i);
		float componentColor= multiplier/((uPos.x-point.x)*(uPos.x-point.x) + (uPos.y-point.y)*(uPos.y-point.y))/i;
		blueGodColor += vec3(componentColor/3.0, componentColor/3.0, componentColor);
	}
	
    
	
	vec3 color = vec3(0,0,0);
	color += pow(blueGodColor,vec3(0.1,0.3,0.8));
    
    
    
    
    vec2 uPos4 = ( gl_FragCoord.xy / resolution.y );//normalize wrt y axis
	uPos4 -= vec2((resolution.x/resolution.y)/2.0, 0.5);//shift origin to center
	

	
	vec3 blueGodColor2 = vec3(0.0);
	for(float i=1.0;i<loop;i++){		
		float t = time*timeSCale-step*i;
		vec2 point = vec2(0.75*sin(t), 0.5*sin(t));
		point += vec2(0.75*cos(t*4.0), 0.5*sin(t*3.0));
		point /= 2.0;
		float componentColor= multiplier/((uPos.x-point.x)*(uPos.x-point.x) + (uPos.y-point.y)*(uPos.y-point.y))/i;
		blueGodColor2 += vec3(componentColor/3.0, componentColor/3.0, componentColor);
	}
	
	vec3 redGodColor = vec3(0.0);
	for(float i=1.0;i<loop;i++){
		float t = time*timeSCale-step*i;
		vec2 point = vec2(0.5*sin(t*4.0+200.0), 0.75*sin(t+10.0));
		point += vec2(0.85*cos(t*2.0), 0.45*sin(t*3.0));
		point /= 2.0;
		float componentColor= multiplier/((uPos.x-point.x)*(uPos.x-point.x) + (uPos.y-point.y)*(uPos.y-point.y))/i;
		redGodColor += vec3(componentColor, componentColor/3.0, componentColor/3.0);
	}
	
	vec3 greenGodColor = vec3(0.0);
	for(float i=1.0;i<loop;i++){
		float t = time*timeSCale-step*i;
		vec2 point = vec2(0.75*sin(t*3.0+20.0), 0.45*sin(t*2.0+40.0));
		point += vec2(4.35*cos(t*2.0+100.0), 0.5*sin(t*3.0));
		point /= 11.0;
		float componentColor= multiplier/((uPos.x-point.x)*(uPos.x-point.x) + (uPos.y-point.y)*(uPos.y-point.y))/i;
		greenGodColor += vec3(componentColor/3.0, componentColor, componentColor/3.0);
	}
	
	float angle = (atan(uPos.y, uPos.x)+PI) / TWOPI;
	float radius = sqrt(uPos.x*uPos.x + uPos.y*uPos.y);
	
	
	
	
	vec3 color2 ;
	color2 *= blueGodColor2+redGodColor+greenGodColor;
	color2 *= 200.0;
	color2 += blueGodColor2+redGodColor+greenGodColor;
   
   vec2 p = (gl_FragCoord.xy * 3.0 - resolution.xy) / min(resolution.x, resolution.y);
    p -= vec2(12.0, 0.0);

    float t = 0.0, d;

    float time2 = 1.0;

    vec2 q = vec2(0.0);
    q.x = fbm(p + 0.01 * time2);
    q.y = fbm(p + vec2(1.0));
    vec2 r = vec2(0.0);
    r.x = fbm(p + 1.0 * q + vec2(1.7, 1.2) + 0.15 * time2);
    r.y = fbm(p + 1.0 * q + vec2(8.3, 2.8) + 0.126 * time2);
    float f = fbm(p + r);
    
    // DS: hornidev
    vec3 color3 = mix(
        vec3(1.0, 1.0, 2.0),
        vec3(1.0, 1.0, 1.0),
        clamp((f * f) * 5.5, 1.2, 15.5)
    );

    color3 = mix(
        color3,
        vec3(1.0, 1.0, 1.0),
        clamp(length(q), 2.0, 2.0)
    );

    color3 = mix(
        color3,
        vec3(0.3, 0.2, 1.0),
        clamp(length(r.x), 0.0, 5.0)
    );

    color3 = (f * f * f * 1.0 + 0.5 * 1.7 * 0.0 + 0.9 * f) * color3;

    vec2 uv2 = gl_FragCoord.xy / resolution.xy;
    float alpha = 50.0 - max(pow(100.0 * distance(uv.x, -1.0), 0.0), pow(2.0 * distance(uv.y, 0.5), 5.0));
 
	
	vec3 from=vec3(1.,.5,0.5);
dir+=color*color3;
	
	mainVR(fragColor, fragCoord, from, dir);	
    fragColor*=vec4(color,1.);
       fragColor+= vec4(color3 * 1.0+color2, 1.);
}



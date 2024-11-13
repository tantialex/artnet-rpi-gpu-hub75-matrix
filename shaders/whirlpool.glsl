// alternate version of Segmented spiral whirlpool: https://www.shadertoy.com/view/4ctcRl

#define T (iTime/2e2)
#define A(v) mat2(cos((v)*3.1416 + vec4(0, -1.5708, 1.5708, 0)))       // rotate
#define H(v) (cos(((v)+.5)*6.2832 + radians(vec3(0, 60, 120)))*.5+.5)  // hue

float map(vec3 u)
{
    float t = T,   // speed
          l = 4.,  // loop to reduce clipping
          s = .4,  // object radius (max)
          a = 3.,  // amplitude
          f = 1e20, i = 0., y, z;
    
    u.xy = vec2(atan(u.x, u.y), length(u.xy));  // polar transform
    u.x += t*133.;  // counter rotation
    
    vec3 p;
    for (; i++<l;)
    {
        p = u;
        y = round((p.y-i)/l)*l+i;         // segment y & skip rows
        p.x *= y;                         // scale x with y
        p.x -= y*y*t*3.1416;              // move x (denominator of t)
        p.x -= round(p.x/6.2832)*6.2832;  // segment x
        p.y -= y;                         // move y
        z = cos(y*t*6.2832)*.5+.5;        // cos wave
        p.z += z*a;                       // wave z
        f = min(f, length(p) - s*z);      // spheres
    }
    
    return f;
}

void mainImage( out vec4 C, in vec2 U )
{
    float l = 50.,  // raymarch loop
          i = 0., d = i, s, r;
    
    vec2 R = iResolution.xy,
         m = (iMouse.z > 0.) ?  // clicking?
               (iMouse.xy - R/2.)/R.y:  // coords from mouse
               vec2(cos(iTime/4. - vec2(0, 1.5708)))*.2;  // coords from time
    
    vec3 o = vec3(0, -10.*sqrt(1.-abs(m.y*2.)), -90./(m.y+1.)),  // camera
         u = normalize(vec3(U - R/2., R.y)),  // 3d coords
         c = vec3(0), p;
    
    mat2 h = A(m.x/2.), // rotate horizontal
         v = A((m.y+.5)/2.);   // vertical
    
    for (; i++<l;)  // raymarch
    {
        p = u*d + o;
        p.xz *= h;
        p.yz *= v;
        
        s = map(p);
        r = (cos(round(length(p.xy))*T*6.2832)*.7 - 1.8)/2.;  // color gradient
        c += min(s, exp(-s/.05))     // black & white
           * H(-r) * (r+1.27) * 6.;  // color
        
        if (s < 1e-3 || d > 1e3) break;
        d += s*.7;
    }
    
    C = vec4(exp(log(c)/2.2), 1);
}

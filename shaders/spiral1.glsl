/*

    Droste Cyclic Expansion Spiral
    ------------------------------
    
    This is a reasonably well known cyclic expansion spiral effect, which people 
    tend to generalize as a Droste spiral. Droste related spiral effects are not 
    new on Shadertoy, but I really like the aesthetic, so I wanted to post one of 
    my own. I decided to produce a far less common superelliptical spiral, just to 
    show that Droste spirals need not be restricted to circles and squares. 
    
    With spirals in general, the idea is to convert standard UV coordinates to a 
    set of concentric rings (or annulus strips) using basic transforms. Then, for 
    each angular revolution, gradually push the radial distance out by one or more 
    radial units, which forms the spiral. As a side note, the concentric rings are
    normally circular, but they can involve any distance metric you can dream up.
   
    Where this differs from a regular spiral is that the radial distance increases
    in an exponential fashion, which is accounted for by inflating the UV coordinates
    exponentially every turn of the spiral. There is also some cyclic animation and 
    spiral shaping using a distance metric... I"ve skipped over the details, but
    those are contained a single function below.
   
    I like this particular process because it's a great example of basic complex
    analysis operations, and if you wish to see a good example of that, have a look
    at MLA's "Interactive Droste Spiral" shader. However, I didn't want any 
    extraneous transform code, so I forwent the complex math in favor of a simple 
    and intuitive function. With that said, the routine I put together is fully 
    functional... and without confusing unexplained magic numbers. :)
    
    By the way, most of the code is just dressing up -- I decided to transform a 
    highlighted Bauhaus style quadtree texture, for whatever reason. It's possible 
    to produce a basic frameless texture-based Droste example with very little code, 
    since the Droste transform itself is just a few lines, which can be cut down 
    even more using a few tricks here and there.
    
    Anyway, I have a 3D version that I'll post, once I've tweaked and commented it
    a bit. There are some "defines" below, for anyone interested.
 
    
    
    Other examples:
    
    
    // It's a bit hard to post a Droste\Escher related
    // example without referencing this awesome piece of work. :)
    Escher's prentententoonstelling - reinder 
    https://www.shadertoy.com/view/Mdf3zM

    // Fabrice's simple example, which prompted me to dig up one 
    // of my own to post. Like myself, I can see that Fabrice has cut 
    // out specific complex functions and only included the necessary
    // components.
    Droste Zoom 2 - FabriceNeyret2 
    https://www.shadertoy.com/view/4c3cD2
    // Based on:
    Droste Zoom - roywig
    https://www.shadertoy.com/view/Xs3SWj
    
    // Nice, concise, cleanly written code.
    Interactive Droste Spiral - mla
    https://www.shadertoy.com/view/3dSyWV

    // If a logorithmic spiral was all you were interested in,
    // the process becomes much simpler. :)
    Luminescent Logarithmic Spiral - Shane (unlisted).
    https://www.shadertoy.com/view/MddGDr

*/

////////////////////////

// I put this in as an afterthought to show that this process will
// work with natural logarithms (log), and the "power of 2" type (log2).
// Of course, the natural log version looks more natural, but the 
// "power of 2" version has a kind of quirky distorted charm.
//
// Log type: Natural (log): 0, Power of 2 (log2): 1
#define LOG_TYPE 0

// Distance metric: Circle: 0, Square: 1, Superellipxe: 2, 
// Hexagon: 3., Octagon: 4.
#define METRIC 2

// Apply bump mapping.
#define BUMP_MAPPING

// Restrict the subdivided quad rectangles to equal sides, 
// or squares. Commenting this out will produce an interesting
// variation, but I don't think it works as well.
#define EQUAL_SIDES

// Use the London background texture, instead of the pattern.
//#define TEXTURE

///////////////////

// Fabrice's fork of "Integer Hash - III" by IQ: https://shadertoy.com/view/4tXyWN
float hash21(vec2 f){
 
    uvec2 p = floatBitsToUint(f);
    p = 1664525U*(p>>1U^p.yx);
    return float(1103515245U*(p.x^(p.y>>3U)))/float(0xffffffffU);
}

// IQ's standard box function.
float sBoxS(in vec2 p, in vec2 b, float sf){
   
    vec2 d = abs(p) - b + sf;
    return min(max(d.x, d.y), 0.) + length(max(d, 0.)) - sf;
}

// Subdivided rectangle grid.
vec4 getGrid(vec2 p, inout vec2 sc){
    
    //p.y += iTime/8.;
    
    vec2 oP = p;
    //if(mod(floor(p.y/sc.y), 2.)<.5) p.x += sc.x/2.; // Row offset.
    //if(mod(floor(p.x/sc.x), 2.)<.5) p.y += sc.y/2.; // Column offset.
    
    // Block ID.
    vec2 ip;
    
    // Subdivide.
    for(int i = 0; i<3; i++){
        
        // Current block ID.
        ip = floor(p/sc) + .5;
        float fi = float(i)*.017; // Unique loop number.
        #ifdef EQUAL_SIDES
        
        // Squares.
        
        // Random split.
        if(hash21(ip + .243 + fi)<.333){
        //if(mod(ip.x * ip.y + float(i)/2., 2.)<.5){
           sc /= 2.;
           p = oP;
           ip = floor(p/sc) + .5; 
        }
        
        #else
        
        // Powers of two rectangles.
        
        // Random X-split.
        if(hash21(ip + .273 + fi)<.333){
           sc.x /= 2.;
           p.x = oP.x;
           ip.x = floor(p.x/sc.x) + .5;
        }
        // Random Y-split.
        if(hash21(ip + .463 + fi)<.333){
           sc.y /= 2.;
           p.y = oP.y;
           ip.y = floor(p.y/sc.y) + .5;
        }
        
        #endif
         
    }
    
    // Local coordinates and cell ID.
    return vec4(p - ip*sc, ip);

}

// A very simple subdivided checker-based shape pattern.
vec3 pattern(vec2 p){

    
    vec2 sc = vec2(1)/8.; // Scale.
    
    vec4 p4 = getGrid(p, sc); // Local coordinates and cell ID.

    // Minimum cell scale.
    float minSc = min(sc.x, sc.y);
    
    // Inner circles.   
    vec2 q = p4.xy - sc/2.;
    vec2 iq = floor(q/minSc) + .5;
    q -= iq*minSc;
    
    float aRatio = 1./3.; // Ratio of area that the circle should take up.
    float r = minSc*sqrt(1./3.14159265*aRatio); // Radius.
    float cir = length(q) - r; // Circle.
    /////
    
    
    // Box.
    float d = sBoxS(p4.xy, sc/2. - .004, (minSc- .004)/8.);


    // Checker shape inversion -- Either a square with a circle
    // cut out, or a circle.
    if(mod(p4.z + p4.w, 2.)<.5){ 
        d = cir;
    }
    else d = max(d, -cir - 1e-5/minSc);

    // Distance and cell ID.
    return vec3(d, p4.zw);

}
 
/////////////////////////////

// The spiral frame distance metric. Many are possible.
float distMetric(vec2 p, vec2 b){

    p = abs(p); // If not using a circle.
     
    #if METRIC == 0
    return length(p) - b.x; // Circle.
    #elif METRIC == 1
    return max(p.x, p.y) - b.x; // Square
    #elif METRIC == 2
    float sEF = 8.;//1./1.15; // Superellipse factor.
    return pow(dot(pow(abs(p), vec2(sEF)), vec2(1)), 1./sEF) - b.x;
    #elif METRIC == 3
    return max(p.x*.8660254 + p.y*.5, p.y) - b.x; // Hexagon.
    #else
    return max(max(p.x, p.y), (p.x + p.y)*.7071) - b.x; // Octagon.
    #endif
}

// I put this in as an afterthought to show that this process will
// work with natural logarithms (log), and the "power of 2" type (log2).
//
#if LOG_TYPE == 0
#define Log log
#define Exp exp
#else 
#define Log log2
#define Exp exp2
#endif

// 2 times PI.
#define TAU 6.2831853

// The scale. Range [0, 1], with ".5" being the preferred default. 
//
// Visually, the scale is analogous to a winding or twisting factor.
// Lower numbers twist less, and larger ones twist more. For me, it 
// represents the scale between successive spiral blocks.
const float scale = .5;


// The cyclic spiral expansion transform, more commonly referred
// to as the Droste transform.
vec2 DrosteTransform(vec2 uv){
     
    // Log polar coordinates They're like regular polar coordinates,
    // but with a radial logarithmic factor.
    uv = vec2(Log(length(uv)), atan(uv.y, uv.x));
    
    
    // Number of spiral arms. Non-negative integers only. Setting this to zero
    // will result in zero spirals -- Visually, just concentric shapes.
    const float spiralN = 1.;
    
    // The logarithmic radial scale. You can incorporate the spiral arms into
    // this calculation "scF = log(pow(scale, -spiralArms))", but I prefer to 
    // include the spiral arms inside the spiral rotation matrix. 
    //
    // By the way, the value, "scF", is actually negative, so I've negated 
    // the two terms in the middle of the spiral matrix to account for this. 
    // In the end, it doesn't really matter, because reversing signs simply 
    // reverses the spiral direction.
    const float scF = Log(scale); //float scF = Log(pow(scale, -arms)); 
    
    // Log spiral rotation: Oddly enough, the best way to see what this line
    // does is to comment it out. :) 
    // 
    // "scF/TAU" is analogous to one radial unit every time we loop
    // around by TAU (2*PI).
    // 
    uv *= mat2(1, -spiralN*scF/TAU, spiralN*scF/TAU, 1);
   
    // Converting to cartesian coordinates. At the same time, we're
    // animating the radial coordinate from the center to the maximum
    // radial length, "scF", before snapping back to the center. It's 
    // a pretty common infinite zoom animation move.
    uv = Exp(mod(uv.x - iTime, scF))*vec2( cos(uv.y), sin(uv.y));


    // Using the coordinates above do obtain a distance in the form of
    // any distance metric you like. Squares and circles are common, but 
    // I'm using a superellispse, which I rarely see.
    float shape = distMetric(uv, vec2(0));
 
   
    // Determine which block the spirally shaped pixel belongs to 
    // then adjust the scale accordingly. 
    //
    // Obviously, larger shapes correspond to larger images. A smaller 
    // image block will spiral around to meet the next larger one.... 
    // Simple enough to understand, but I'm glad I wasn't someone like 
    // Escher trying to figure this all out by hand - Quite amazing.
    //
    // The coordinates are segmented by non-linear "log(scale)" increments, 
    // so there's some log related division and power scaling involved. 
    // By the way, this all works with other log systems, like "log 2", 
    // but the natural log system looks more natural, strangely enough. :)
    //
    // Log calculation. The natural logarithm, logN, (just "log" in WebGl)
    // is the default.
    float nP = pow(scale, floor(Log(shape)/scF));
    //
    // I've seen people use the following expression, which will 
    // work if you get lucky and the integers line up. In general 
    // though, you should use the above expression.
    //float nP = exp2(floor(log2(shape/scale)));
  
    // We only want half the distance, so take that into account.
    return uv/nP/2.;
    
}


void mainImage(out vec4 fragColor, in vec2 fragCoord){


    // Pixel coordinates.
    vec2 uv = (fragCoord - iResolution.xy*.5)/iResolution.y;

    // Linear radial length. Used for shading.
    float r = length(uv);
    
    // Smoothing factor.
    float sf = 1./iResolution.y;
    sf /= (r*2. + 1e-3);
    
    // Resolution based shadow factor.
    float shF = iResolution.y/450.;
    //shF /= (r*2. + 1e-3);
 
    // Directional light vector
    vec2 ld = normalize(vec2(-1, -2));
    
    
    // Perform the Droste spiral transform.
    vec2 p = DrosteTransform(uv);
    
    //////////////////////
    #ifdef TEXTURE
    // Debug London texture, which needs centering. Ones that
    // wrap don't necessarily need it.
    vec3 tx = texture(iChannel0, p + .5).xyz; tx *= tx;
    vec3 col = tx;
    #else
    // The background pattern. Obviously, you could reduce the 
    // character count and complexity considerably by rendering
    // just a texture, or a much simple pattern minus bump 
    // mapping and so forth. Having said that, this pattern isn't
    // particularly difficult to produce.

    // Pattern and highlight sample.
    vec3 p3 = pattern(p);
    vec3 p3Hi = pattern(p - ld*.002);
    
    float pat = p3.x;
    float patHi = p3Hi.x;
    
    // Cell ID.
    vec2 id = p3.yz;
    
    
    // Rough directional derivative gradient calculation.
    // Beveling. We're working with inside shape negative numbers, which always
    // confuses me, when taking the maximum. :)
    float b = max(clamp(patHi*2., -.02, 0.) - clamp(pat*2., -.02, 0.), 0.)/.002;
    b += max((patHi - pat)/4., 0.)/.002;
    
    // Pattern color. Black and white.
    vec3 gCol = vec3(.85);
    /*
    // Random color. It doesn't really work here, since it distracts from the 
    // frame, but it's here for anyone who's curious as to what it looks like.
    float rnd = hash21(id);
    vec3 gCol = .5 + .45*cos(6.2831*rnd/4. + vec3(0, 1, 2));
    if(hash21(id + .13)<.35) gCol = vec3(.85);
    */
    
    #ifdef BUMP_MAPPING
    // Apply the highlighting.
    gCol *= .85 + b*.5;
    #endif
    
    
    // Apply to the pattern to the background.
    vec3 col = vec3(.03);
    col = mix(col, vec3(0), 1. - smoothstep(0., sf*shF*8., pat));
    col = mix(col, gCol, 1. - smoothstep(0., sf, pat));
    #endif
    
    
    /////////////////////
    // Rendering the frame.
    
    // Here's my contribution to Drost effect science: :D
    // Render the frame on both sides of the spiral.
    // This will allow you to elliminate jaggies, produce
    // drop shadows, fake AO, or whatever else you can 
    // think of.
    
    float frame = distMetric(p, vec2(.5));
    float frame2 = distMetric(p - ld*.002, vec2(.5));
    
    float fw = .0085/(.25 + r);
    float sFr = r;//abs(frame);
    frame = abs(frame);
    // The inner frame is roughly half the size, so we're
    // artificially increasin it's width by widening by "fw".
    frame = min(frame - fw, abs(frame - .5 + scale/2.));
    frame -= fw;
    
    frame2 = abs(frame2);
    frame2 = min(frame2 - fw, abs(frame2 - .5 + scale/2.));
    frame2 -= fw;
     
    // Frame bump mapping.
    float bF = max((frame2 - frame), 0.)/.002;
    
    // Frame color and bump mapping.
    
    #ifdef BUMP_MAPPING
    vec3 fCol = vec3(1, .5, .1);
    fCol *= .5 + bF*2.;
    #else
    vec3 fCol = vec3(.75, .95, .05);
    #endif
     
    // Applying the drop shadows and the frame.
    #ifndef BUMP_MAPPING
    shF /= 2.; // Less shadow when no highlighting is applied.
    #endif
    col = mix(col, col*.3, 1. - smoothstep(0., sf*shF*8., frame));
    col = mix(col, vec3(0), 1. - smoothstep(0., sf, frame));
    col = mix(col, fCol, 1. - smoothstep(0., sf, frame + .005/(.25 + r)));
    //////// 

	
    fragColor = vec4(sqrt(max(col, 0.)), 1);
}

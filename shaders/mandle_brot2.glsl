// https://www.shadertoy.com/view/4tyBWh

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;
    int bitdepth = 4;
    int res = int(pow(2.0, float(bitdepth)));

    // Time varying pixel color
    //vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));
    
    float zoom = 0.5 + (pow(iTime, 3.0) / 8.0);
    //zoom = 0.5;
    float xoffset = -(zoom / 1.0) * (iResolution.x / 2.85);
    float yoffset = -(zoom / 4.0) * (iResolution.x / 18.0);
    yoffset = 1.0;
    
    float c_re = ((fragCoord.x + xoffset) - iResolution.x/2.0)*4.0/(iResolution.x * zoom);
    float c_im = ((fragCoord.y + yoffset) - iResolution.y/2.0)*4.0/(iResolution.x * zoom);
    float x = 0.0;
    float y = 0.0;
    
    int max = int(pow(2.0, float(bitdepth * 3))) - 1;
    
    int iteration = 0;
    while (((x*x+y*y) <= 4.0) && (iteration < max)) {
        float x_new = x*x - y*y + c_re;
        y = 2.0*x*y + c_im;
        x = x_new;
        iteration++;
    }
    
    float b = float(iteration % int(res)) / float(res);
    float g = float((iteration >> bitdepth) % int(res)) / float(res);
    float r = float((iteration >> (bitdepth * 2)) % int(res)) / float(res);
    
    vec3 col = vec3(r,g,b);
        
    //col = vec3(1.0,1.0,1.0) - col;

    fragColor = vec4(col,1.0);    
    
    // Output to screen
    
}

float getCopper(vec2 uv)
{
    uv.x*=3.0;
    float t0=texture(iChannel0,uv.xy*0.4+iTime*vec2(0,1)*0.05).r;
    float t1=texture(iChannel0,uv.xy*0.5+iTime*vec2(0,1)*0.01).r*0.2;
    return smoothstep(0.15,0.25,t0*t1*2.0);
}    

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;

    float eps=0.005;
    float p0=getCopper(uv);
    float p1=getCopper(uv+vec2(0.0,eps));
    float p2=getCopper(uv+vec2(eps,eps));
    vec3 normal=normalize(vec3(p0-p1,p2-p1,0.5));
    
    vec3 lightDir=vec3(0.25,0.75,0.2);
    if(iMouse.z>0.0)lightDir.xy=vec2(iMouse.xy/iResolution.xy);
    lightDir-=vec3((uv*0.5)*2.0,0);
    lightDir.x*=iResolution.x/iResolution.y;
    lightDir=normalize(lightDir);
    
    vec3 color=(pow(dot(lightDir,normal)+0.1,3.0)+0.15)*vec3(1,0.7,0.5);
    
    fragColor=vec4(color,1);
}

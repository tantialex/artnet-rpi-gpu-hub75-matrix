
//License: CC BY 3.0
//Author: Jan Mróz (jaszunio15)

/*
	Originally I wanted to make noise based on Voronoise by Inigo Quilez, 
	but without reading his code. 
	(reference shader: https://www.shadertoy.com/view/Xd23Dh)
	
	My shader quicky became something different. My concept wasn't
	nearly close to what Inigo did. Maybe next time :)
*/

#define TIME (iTime * 5.0)

vec2 hash22(vec2 x)
{
 	return fract(sin(x * mat2(43.37862, 24.58974, 32.37621, 53.32761)) * 4534.3897);
}

float hash12(vec2 x)
{
 	return fract(sin(dot(x, vec2(43.37861, 34.58761))) * 342.538772);
}

vec2 getCellPoint(vec2 cell)
{
    float time = TIME * (hash12(cell + 0.123) - 0.5) * 0.5;
    float c = cos(time), s = sin(time);
    vec2 hash = (hash22(cell) - 0.5) * mat2(c, s, -s, c) + 0.5;;
 	return hash + cell;
}

float getCellValue(vec2 cell)
{
 	return hash12(cell);
}

float makeSmooth(float x)
{
    float mouse = (iMouse.x / iResolution.x);
    if (mouse == 0.0) mouse = 1.0 - (cos(iTime * 0.5) * 0.5 + 0.5);
  	return mix(x * x * (3.0 - 2.0 * x), sqrt(x), mouse);
}

float modifiedVoronoiNoise12(vec2 uv)
{
 	vec2 rootCell = floor(uv);

    float value = 0.0;

    for (float x = -1.0; x <= 1.0; x++)
    {
     	for(float y = -1.0; y <= 1.0; y++)
        {
         	vec2 cell = rootCell + vec2(x, y);
            vec2 cellPoint = getCellPoint(cell);
            float cellValue = getCellValue(cell);
            float cellDist = distance(uv, cellPoint);
            value += makeSmooth(clamp(1.0 - cellDist, 0.0, 1.0)) * cellValue;
        }
    }

    return value * 0.5;
}

float layeredNoise12(vec2 x)
{
 	float sum = 0.0;
    float maxValue = 0.0;

    for (float i = 1.0; i <= 2.0; i *= 2.0)
    {
        float noise = modifiedVoronoiNoise12(x * i) / i;
     	sum += noise;
        maxValue += 1.0 / i;
    }

    return sum / maxValue;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = (fragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    vec2 stretchedUV = (fragCoord.xy - 0.5 * iResolution.xy) / iResolution.xy;
	float vignette = smoothstep(0.55, 0.0, length(stretchedUV));
	uv.y -= TIME * 0.05;
    
	uv *= 6.0;
    
  	//just simplified hue shifting stuff with uv based offset
  	vec4 col = sin(TIME * 0.1 + uv.y * 0.2 + vec4(0,2,4,6)) * 0.5 + 0.5;
  	vec4 col2 = sin(TIME * 0.1 + 0.6 + uv.y * 0.2 + vec4(0,2,4,6)) * 0.5 + 0.5;

    uv += layeredNoise12(uv);
    //uv -= layeredNoise12(uv + 43.0) * 0.5;
  	float noise = layeredNoise12(uv);
  	noise *= vignette;
  	fragColor = mix(col, col2 * 0.2, 1.0 - noise * 2.0);
    fragColor = smoothstep(-0.14, 1.1, fragColor);
}

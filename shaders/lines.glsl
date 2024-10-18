
#define MI 7        // max iteration
#define LW 12.0     // line width
#define LI 1.2     // line indensity
#define AS 0.5      // animation speed
#define BI 1.0      // background indensity

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
	float time = iTime * .05; // slow the time

	vec2 uv = fragCoord.xy / iResolution.xy;

	vec2 p = uv;
	vec2 i = p;
	float bi = BI;

	for (int n = 0; n < MI; n++) {
		float t = time * float(n) * AS;
		i = p + vec2(cos(t - i.x) + sin(t + i.y),
                     sin(t - i.y) + cos(t + i.x));
		bi += length(vec2(
				p.x / (sin(i.x + t) / LI),
				p.y / (cos(i.y + t) / LI)));
	}

	bi /= float(MI);
    bi /= LW;

	vec3 color = vec3(bi);
	vec3 color_base = vec3(0.0, 0.05, 0.2);
	color = clamp(color_base + color, 0.0, 1.0);
	fragColor = vec4(color, 1.0);
}

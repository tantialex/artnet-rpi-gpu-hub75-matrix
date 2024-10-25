// https://www.shadertoy.com/view/MXjBRK

const float pi = 3.1415926535897;

float rand2(vec2 uv) {
    return fract(sin(dot(uv, vec2(13.337, 61.998))) * 48675.75647);
}

vec2 rotate(vec2 uv, float a) {
    return vec2(uv.y * cos(a) + uv.x * sin(a), uv.x * cos(a) - uv.y * sin(a));
}

vec2 rand2x2(vec2 uv) {
    return vec2(rand2(uv), rand2(-uv));
}

vec3 rand2x3(vec2 uv) {
    return vec3(rand2(uv), rand2(-uv), rand2(vec2(-uv.x - 5., uv.y + 1.)));
}

float perl(vec2 uv, float t) {
    vec2 id = floor(uv);
    vec2 loc = fract(uv);
    vec2 sloc = smoothstep(0., 1., loc);
    return mix(
        mix(
            dot(loc, rotate(vec2(1.), rand2(id) * (pi * 2. + t))),
            dot(loc - vec2(1., 0.), rotate(vec2(1.), rand2(id + vec2(1., 0.)) * (pi * 2. + t))),
            sloc.x
        ),
        mix(
            dot(loc - vec2(0., 1.), rotate(vec2(1.), rand2(id + vec2(0., 1.)) * (pi * 2. + t))),
            dot(loc - vec2(1., 1.), rotate(vec2(1.), rand2(id + vec2(1., 1.)) * (pi * 2. + t))),
            sloc.x
        ),
        sloc.y
    );
}

float fperl(vec2 uv, float t, float iter) {
    float o = 0., k = 0.;
    for (float i = 0.; i < iter; i++) {
        o += perl(uv * pow(2., i), t * pow(2., i)) / pow(2., i);
        k += 1. / pow(2., i);
    }
    return o / k;
}

float vor(vec2 uv) {
    vec2 id = floor(uv);
    vec2 loc = fract(uv);
    float o = 100.;
    for (float x = -1.; x <= 1.; x++) {
        for (float y = -1.; y <= 1.; y++) {
            o = min(o, distance(sin(2.5 * pi * rand2x2(id + vec2(x, y))) * 0.8 + 0.2, loc - vec2(x, y)));
        }
    }
    return o;
}

vec3 vorid3(vec2 uv) {
    vec2 id = floor(uv);
    vec2 loc = fract(uv);
    float o = 1000.;
    vec3 ou = vec3(0);
    for (float x = -1.; x <= 1.; x++) {
        for (float y = -1.; y <= 1.; y++) {
            float d = distance(sin(2.5 * pi * rand2x2(id + vec2(x, y))) * 0.8 + 0.2, loc - vec2(x, y));
            if (o > d) {
                o = d;
                ou = rand2x3(id + vec2(x, y));
            }
        }
    }
    return ou;
}

vec3 star(vec2 uv) {
    float val = vor(uv * 3.);
    val = 0.01 / val;
    val = pow(val, 1.7);
    vec3 col = vec3(val) * (vorid3(uv * 3.));
    return col * fperl(uv / 2., 0., 2.);
}

vec3 fstar(vec2 uv, float iter, float t) {
    vec3 o = vec3(0);
    for (float i = 0.; i < iter; i++) {
        o += star(rotate(uv + vec2(t, 0.) / pow(1.5, i), i) * pow(1.5, i));
    }
    return o;
}

float fnebula(vec2 uv, float iter, float t) {
    float o = 0.;
    for (float i = 0.; i < iter; i++) {
        o += fperl(rotate(uv + vec2(t, 0.) / pow(1.5, i), i) * pow(1.5, i) / 2., 0., 6.);
    }
    return o;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    uv = uv * 2. - 1.;
    if (iResolution.x > iResolution.y) {
        uv.x = uv.x * iResolution.x / iResolution.y;
    }
    if (iResolution.y > iResolution.x) {
        uv.y = uv.y * iResolution.y / iResolution.x;
    }

    vec3 col = fstar(uv, 7., iTime / 5.);
    col *= 10.;
    col = pow(col, vec3(1));
    col = col.r * vec3(1, 0.45, 0.4) + col.g * vec3(0.4, 0.4, 1) + col.b * vec3(1);
    col = vec3(0, 0, 0.05) + clamp(vec3(0), vec3(1), vec3(0, 0, 0.03) + col);

    float n = fnebula(uv, 7., iTime / 5.);

    n = n * 0.4;
    n = clamp(n, 0., 1.);

    n = 1. - n;
    n = 0.5 / n;
    n = n - 0.5;

    vec3 vnb = n * vec3(0.7, 0.1, 1);
    vnb = clamp(vnb, vec3(0), vec3(1));
    fragColor = vec4(vnb + col, 1);
}

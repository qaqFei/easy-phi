precision highp float;
varying lowp vec2 uv;

uniform float p;
uniform float seed;
uniform float innerCircRadius;
uniform vec3 color;

float rand(vec2 n) { 
    return fract(sin(dot(n, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    
    float a = rand(ip);
    float b = rand(ip + vec2(1.0, 0.0));
    float c = rand(ip + vec2(0.0, 1.0));
    float d = rand(ip + vec2(1.0, 1.0));
    
    vec2 u = fp * fp * (3.0 - 2.0 * fp);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float circularNoise(vec2 uv, float density, float seed) {
    vec2 center = uv - 0.5;
    float radius = length(center) * density;
    float angle = abs(atan(center.y, center.x));

    if (uv.y > 0.5) {
        angle += sin(angle) * 2.;
    }

    vec2 seedOffset = vec2(seed * 100.0, seed * 100.0);
    vec2 polarCoord = vec2(radius, angle) + seedOffset;
    
    float n = 0.0;
    n += noise(polarCoord) * 0.7;
    n += noise(polarCoord * 2.0) * 0.3;
    n += noise(polarCoord * 4.0) * 0.1;
    
    return n;
}

void main() {
    gl_FragColor.rgb = color;
    gl_FragColor.a = 1.0;
    float l = length(uv - 0.5);

    if (innerCircRadius <= l && l <= 0.5) {
        float n = circularNoise(uv, 50.0, seed);
        gl_FragColor.a *= (n < p) ? 0.0 : 1.0;
    } else {
        gl_FragColor.a = 0.0;
    }
}

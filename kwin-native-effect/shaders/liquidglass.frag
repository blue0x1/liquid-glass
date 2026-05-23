#version 140

uniform sampler2D u_backdrop;
uniform vec2 u_texture_size;
uniform vec2 u_window_size;
uniform float u_time;
uniform float u_opacity;
uniform vec2 u_light_dir;

in vec2 texcoord0;
out vec4 fragColor;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec3 blur9(vec2 uv, vec2 texel, float radius)
{
    vec3 c = texture(u_backdrop, uv).rgb * 0.18;
    c += texture(u_backdrop, uv + vec2( 1.0,  0.0) * texel * radius).rgb * 0.10;
    c += texture(u_backdrop, uv + vec2(-1.0,  0.0) * texel * radius).rgb * 0.10;
    c += texture(u_backdrop, uv + vec2( 0.0,  1.0) * texel * radius).rgb * 0.10;
    c += texture(u_backdrop, uv + vec2( 0.0, -1.0) * texel * radius).rgb * 0.10;
    c += texture(u_backdrop, uv + vec2( 0.707,  0.707) * texel * radius * 0.78).rgb * 0.07;
    c += texture(u_backdrop, uv + vec2(-0.707,  0.707) * texel * radius * 0.78).rgb * 0.07;
    c += texture(u_backdrop, uv + vec2( 0.707, -0.707) * texel * radius * 0.78).rgb * 0.07;
    c += texture(u_backdrop, uv + vec2(-0.707, -0.707) * texel * radius * 0.78).rgb * 0.07;
    return c;
}

void main()
{
    vec2 uv = clamp(texcoord0, vec2(0.001), vec2(0.999));
    vec2 aspect = vec2(u_window_size.x / max(u_window_size.y, 1.0), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    float dist = length(p);
    float rim = pow(smoothstep(0.30, 0.98, dist), 2.6);
    float lens = (noise(p * 5.0 + vec2(u_time * 0.06, u_time * 0.02)) - 0.5) * 0.010;
    vec2 normal = normalize(p + 0.0001);
    vec2 refracted = clamp(uv + normal * (rim * 0.028 + lens), vec2(0.001), vec2(0.999));
    vec2 texel = 1.0 / max(u_texture_size, vec2(1.0));

    vec3 glass = blur9(refracted, texel, 7.0 + rim * 10.0);
    float luma = dot(glass, vec3(0.2126, 0.7152, 0.0722));
    glass = mix(vec3(luma), glass, 0.58);
    glass = mix(glass, vec3(0.05, 0.075, 0.11), 0.34);

    float frost = noise(refracted * u_texture_size * 0.050 + u_time * 0.12);
    vec3 n = normalize(vec3(normal * 0.84, 1.0));
    vec3 l = normalize(vec3(u_light_dir, 0.85));
    vec3 v = vec3(0.0, 0.0, 1.0);
    vec3 h = normalize(l + v);
    float diff = max(dot(n, l), 0.0);
    float spec = pow(max(dot(n, h), 0.0), 48.0);
    float fresnel = pow(1.0 - max(dot(n, v), 0.0), 4.0);
    float topGlow = smoothstep(0.92, 0.10, uv.y) * 0.10;
    vec3 sheen = vec3(0.86, 0.95, 1.0) * (topGlow + rim * 0.12 + diff * 0.04 + spec * 0.62 + fresnel * 0.20);
    vec3 grain = vec3(frost - 0.5) * 0.015;

    vec3 color = glass + sheen + grain;
    float alpha = clamp(0.18 + u_opacity * 0.60 + rim * 0.09 + topGlow * 0.10 + fresnel * 0.05, 0.18, 0.88);
    fragColor = vec4(color, alpha);
}

#version 450

layout(location = 0) in vec2 outNDC;
layout(location = 0) out vec4 outColor;

// using the sampler instead
// layout (binding = 1, rgba32f) uniform readonly image2D outputImage;
layout (binding = 25) uniform sampler2D outputImage;

float luminance(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = outNDC * 0.5 + 0.5;

    vec2 texel = 1.0 / vec2(textureSize(outputImage, 0));

    vec3 rgbNW = texture(outputImage, uv + vec2(-1,-1)*texel).rgb;
    vec3 rgbNE = texture(outputImage, uv + vec2( 1,-1)*texel).rgb;
    vec3 rgbSW = texture(outputImage, uv + vec2(-1, 1)*texel).rgb;
    vec3 rgbSE = texture(outputImage, uv + vec2( 1, 1)*texel).rgb;
    vec3 rgbM  = texture(outputImage, uv).rgb;

    float lumaNW = luminance(rgbNW);
    float lumaNE = luminance(rgbNE);
    float lumaSW = luminance(rgbSW);
    float lumaSE = luminance(rgbSE);
    float lumaM  = luminance(rgbM);

    float lumaMin = min(lumaM,
                    min(min(lumaNW,lumaNE),
                    min(lumaSW,lumaSE)));

    float lumaMax = max(lumaM,
                    max(max(lumaNW,lumaNE),
                    max(lumaSW,lumaSE)));

    vec2 dir;

    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) *
        (0.25 * (1.0/8.0)),
        1.0/128.0);

    float rcpDirMin = 1.0 /
        (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = clamp(dir * rcpDirMin,
                vec2(-8.0),
                vec2(8.0)) * texel;

    vec3 rgbA =
        0.5 * (
            texture(outputImage, uv + dir * (1.0/3.0 - 0.5)).rgb +
            texture(outputImage, uv + dir * (2.0/3.0 - 0.5)).rgb);

    vec3 rgbB =
        rgbA * 0.5 +
        0.25 * (
            texture(outputImage, uv + dir * -0.5).rgb +
            texture(outputImage, uv + dir * 0.5).rgb);

    float lumaB = luminance(rgbB);

    if (lumaB < lumaMin || lumaB > lumaMax)
        outColor = vec4(rgbA,1.0);
    else
        outColor = vec4(rgbB,1.0);
}
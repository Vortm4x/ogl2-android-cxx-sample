precision highp float;
uniform vec3 colorMask[7];
uniform vec2 screenResolution;
uniform vec2 offset;
uniform float zoom;
uniform int maxIterations;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float mandelbrot(vec2 c)
{
    vec2 z = vec2(0.);

    for(int i = 0; i < maxIterations; ++i)
    {
        z = vec2(z.x * z.x - z.y * z.y, 2. * z.x * z.y) + c;

        if(length(z) >= 2.)
        {
            return float(i) + 1. - log(log2(length(z)));
        }
    }

    return float(maxIterations);
}

void main()
{
    vec2 uv = (gl_FragCoord.xy - .5 * screenResolution.xy) / screenResolution.y;
    vec2 pos = offset.xy / screenResolution.y;
    vec2 beg = 3. * (uv / zoom + pos);


    float mandel = mandelbrot(beg);
    float maxMandel = float(maxIterations);

    float hue = mandel / maxMandel;
    float val = (mandel < maxMandel) ? 1. : 0.;
    float sat = 1.;

    vec3 resHsv = vec3(hue, sat, val);
    vec3 resRgb =  hsv2rgb(resHsv);

    gl_FragColor = vec4(resRgb, 1.);
}

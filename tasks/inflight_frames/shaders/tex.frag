#version 430

layout(location = 0) out vec4 resultColor;

vec2 iResolution = vec2(100, 100);

float hash( in ivec2 p )
{

    // 2D -> 1D
    int n = p.x*3 + p.y*113;

    // 1D hash by Hugo Elias
	n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return -1.0+2.0*float( n & 0x0fffffff)/float(0x0fffffff);
}

float noise( in vec2 p )
{
    ivec2 i = ivec2(floor( p ));
    vec2 f = fract( p );
	
    // cubic interpolant
    vec2 u = f*f*(3.0-2.0*f);

    return mix( mix( hash( i + ivec2(0,0) ), 
                     hash( i + ivec2(1,0) ), u.x),
                mix( hash( i + ivec2(0,1) ), 
                     hash( i + ivec2(1,1) ), u.x), u.y);
}


void mainImage(out vec4 pixelColor, in vec2 fragCoord) {
    vec2 uvCoords = fragCoord / iResolution.xy;
    float noiseValue = 0.0;

    uvCoords *= 8.0;
    mat2 transformMatrix = mat2(1.6, 1.2, -1.2, 1.6);

    noiseValue  = 0.5000 * noise(uvCoords); uvCoords = transformMatrix * uvCoords;
    noiseValue += 0.2500 * noise(uvCoords); uvCoords = transformMatrix * uvCoords;
    noiseValue += 0.1250 * noise(uvCoords); uvCoords = transformMatrix * uvCoords;
    noiseValue += 0.0625 * noise(uvCoords); uvCoords = transformMatrix * uvCoords;

    noiseValue = 0.5 * (1.0 + noiseValue);

    pixelColor = vec4(vec3(noiseValue), 1.0);
}

void main() {
    mainImage(resultColor, gl_FragCoord.xy);
}

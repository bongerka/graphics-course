#version 430


layout(binding = 0) uniform sampler2D imageFile;
layout(binding = 1) uniform sampler2D iChannel1;

layout(location = 0) out vec4 resultColor;

layout(push_constant) uniform params_t
{
  float iTime;
} params;

vec2 iResolution = vec2(1280, 720);
vec3 LIGHT_POS = vec3(0, 0, -100.0);
float LIGHT_POW = 10000.0;

struct sphr {
    vec3 center;
    float radius;
    vec3 color;
};


sphr bodies[3] = sphr[](
    sphr(vec3(0, 0, 0), 2.0, vec3(1.0, 1.0, 0.0)),
    sphr(vec3(0, 0, 0), 0.7, vec3(0.0, 0.5, 1.0)),
    sphr(vec3(0, 0, 0), 0.2, vec3(0.5, 0.5, 0.5))
);


float sdf(in vec3 point) {
    float result = 10000.0;
    for (int i = 0; i < 3; ++i) {
        float dist = length(point - bodies[i].center) - bodies[i].radius;
        result = min(dist, result);
    }
    return result;
}


vec3 sampleSphereTexture(in sphr sphere, in vec3 pos) {
    vec3 localPos = (pos - sphere.center) / sphere.radius;
    localPos = localPos * 0.5 + vec3(0.5);

    vec3 weight = normalize(abs(localPos));
    vec3 texColor = 
        weight.x * texture(iChannel1, localPos.yz).rgb +
        weight.y * texture(iChannel1, localPos.xz).rgb +
        weight.z * texture(iChannel1, localPos.xy).rgb;

    return texColor;
}

vec3 determineBaseColor(in vec3 pos) {
    vec3 closestColor = bodies[0].color * sampleSphereTexture(bodies[0], pos);
    float shortestDist = length(pos - bodies[0].center) - bodies[0].radius;

    for (int idx = 1; idx < 3; ++idx) {
        float testDist = length(pos - bodies[idx].center) - bodies[idx].radius;
        if (testDist < shortestDist) {
            closestColor = bodies[idx].color * sampleSphereTexture(bodies[idx], pos);
            shortestDist = testDist;
        }
    }
    return closestColor;
}


void castRay(in vec3 direction, in vec3 origin, out vec3 hitPoint, out bool rayDidHit) {
    vec3 currentPos = origin;
    
    for (int i = 0; i < 256; i++) { 
        float stepSize = abs(sdf(currentPos));
        
        if (stepSize < 0.01) { 
            hitPoint = currentPos;
            rayDidHit = true;
            return;
        } else if (stepSize > 100.0) {
            rayDidHit = false;
            return;
        } else {
            currentPos += direction * stepSize;
        }
    }
    
    rayDidHit = false;
    return;
}


vec3 computeGradient(in vec3 pos, in float epsilon) {
    float refValue = sdf(pos);

    vec3 shiftX = vec3(epsilon, 0.0, 0.0);
    vec3 shiftY = vec3(0.0, epsilon, 0.0);
    vec3 shiftZ = vec3(0.0, 0.0, epsilon);

    vec3 sampledValues = vec3(
        sdf(pos + shiftX),
        sdf(pos + shiftY),
        sdf(pos + shiftZ)
    );

    return (sampledValues - refValue) / epsilon;
}


vec3 computeLighting(in vec3 pos, out vec3 norm, out vec3 lightVec, out float intensity) {
    norm = normalize(computeGradient(pos, 0.0001));
    lightVec = LIGHT_POS - pos;
    intensity = LIGHT_POW / pow(length(lightVec), 2.0);
    return normalize(lightVec);
}


vec3 scatteredLight(in vec3 location, in vec3 baseCol) {
    vec3 normDir, lightVector;
    float lightInt;
    vec3 lightUnit = computeLighting(location, normDir, lightVector, lightInt);
    
    vec3 finalDiffuse = baseCol * max(dot(normDir, lightUnit), 0.0) * lightInt;
    return finalDiffuse;
}


vec3 reflectedLight(in vec3 viewPos, in vec3 hitPos, in vec3 baseCol) {
    vec3 normDir, lightVector;
    float lightInt;
    vec3 lightUnit = computeLighting(hitPos, normDir, lightVector, lightInt);
    
    vec3 eyeVector = normalize(viewPos - hitPos);
    vec3 reflectedRay = -reflect(lightVector, normDir);
    
    float specFactor = pow(max(dot(eyeVector, normalize(reflectedRay)), 0.0), 20.0);
    return vec3(specFactor * lightInt);
}


void simulate_planet_rotation() {
    // Sun remains stationary at the origin
    bodies[0].center = vec3(0.0, 0.0, 10.0);

    // Earth orbits the Sun
    float earth_orbit_radius = 5.0; // Distance from the Sun
    float earth_orbit_speed = 1.5;  // Speed of Earth's orbit
    float earth_angle = params.iTime * earth_orbit_speed;
    bodies[1].center = bodies[0].center + vec3(cos(earth_angle) * earth_orbit_radius, sin(earth_angle) * earth_orbit_radius, 0.0);

    // Moon orbits the Earth
    float moon_orbit_radius = 1.5; // Distance from the Earth
    float moon_orbit_speed = earth_orbit_speed * 365.0 / 28.0;  // Speed of Moon's orbit
    float moon_angle = params.iTime * moon_orbit_speed;
    bodies[2].center = bodies[1].center + vec3(cos(moon_angle) * moon_orbit_radius, sin(moon_angle) * moon_orbit_radius, 0.0);
}


void processRay(in vec3 camPos, in vec3 direction, out vec3 hitPoint, out vec3 finalColor) {
    finalColor = texture(imageFile, direction.xy).rgb;
    bool rayDidHit;
    castRay(direction, camPos, hitPoint, rayDidHit);
    if (rayDidHit) {
        vec3 baseCol = determineBaseColor(hitPoint);
        finalColor = reflectedLight(camPos, hitPoint, baseCol) + scatteredLight(hitPoint, baseCol);
    }
}


void mainImage(out vec4 pixelColor, in vec2 pixelCoord) {
    simulate_planet_rotation();

    vec2 normCoord = (pixelCoord - iResolution.xy * 0.5) / min(iResolution.x, iResolution.y);

    vec3 eye = vec3(0.0, 0.0, -1.0);
    vec3 direction = normalize(vec3(normCoord, 0.0) - eye);
    
    vec3 impactPoint, computedColor;
    processRay(eye, direction, impactPoint, computedColor);
    
    pixelColor = vec4(computedColor, 1.0);
    //pixelColor = texture(iChannel1, normCoord);
}


void main()
{
  ivec2 uv = ivec2(gl_FragCoord.xy);

  vec4 color;
  mainImage(resultColor, uv * ivec2(1, -1) + ivec2(0, iResolution.y));
}

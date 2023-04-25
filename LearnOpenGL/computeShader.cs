

#version 430 core

layout(local_size_x = 10, local_size_y = 10, local_size_z = 1) in;

struct Material
{
    vec4 color;
    vec4 emissionColor;
    vec4 specularColor;

    vec4 data; //{emissionStrength, smoothness, specularProbability, unused}
}globalMat;

struct Sphere
{
    vec4 data;
    vec4 materialData;
};

layout (std140, binding = 4) buffer GeometryBlock {
    Sphere spheres [5];
};

layout (std140, binding = 5) buffer MaterialBlock {
    Material materials [5];
};

// ----------------------------------------------------------------------------
//
// uniforms
//
// ----------------------------------------------------------------------------

layout(rgba32f, binding = 0) uniform image2D imgOutput;

layout(location = 0) uniform float t;                 /* Time */
layout(location = 1) uniform float frame;

// ----------------------------------------------------------------------------
//
// functions
//
// ----------------------------------------------------------------------------


// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash(uint x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct(uint m)
{
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

float random(float seed) { return floatConstruct(hash(floatBitsToUint(seed))); }

vec3 random_unit_vector(float seed)
{
    float safety = 100.0;
    vec3 ret;
    for(float i = 0; i < safety; i += 1.0)
    {
        ret = vec3(random(seed + i * 2298) * 2.0 - 1.0, random(seed * 18273 + i * 3487) * 2.0 - 1.0, random(seed * 1388.38 + i * 823769) * 2.0 - 1.0);
        if(length(ret) <= 1.0)
        {
            return normalize(ret);
        }
    }
    return vec3(1.0, 0.0, 0.0);
}

float ACESFilm(float val)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    float tone_mapped = (val * (a * val + b)) / (val * (c * val + d) + e);
    return clamp(tone_mapped, 0.0, 1.0);
}

vec4 ACESFilmCol(vec3 val)
{
    return vec4(ACESFilm(val.r), ACESFilm(val.g), ACESFilm(val.b), 1.0);
}

vec3 getEnvironmentLight(vec3 ray_o, vec3 ray_d)
{
    return vec3(0.0);
}

float hit_sphere(vec3 ray_o, vec3 ray_d, int sphere_index)
{
    vec3 sphere_p = spheres[sphere_index].data.xyz;
    float sphere_r = spheres[sphere_index].data.w;

    vec3 oc = ray_o - sphere_p;
    float a = dot(ray_d, ray_d);
    float b = 2.0 * dot(oc, ray_d);
    float c = dot(oc, oc) - sphere_r * sphere_r;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
    {
        return -1.0;
    }
    else
    {
        return (-b - sqrt(discriminant)) / (2.0 * a);
    }
}

void calculateRayCollision(vec3 ray_o, vec3 ray_d, inout vec3 normal, inout vec3 hitPoint, inout bool hit, inout int spherei)
{
    float t = 1. / 0.;

    for (int sphere_index = 0; sphere_index < 10; sphere_index++) {
        float hit_t = hit_sphere(ray_o, ray_d, sphere_index);
        if (hit_t > 0.0001 && hit_t < t)
        {
            hit = true;
            t = hit_t;
            normal = normalize(ray_o + ray_d * hit_t - spheres[sphere_index].data.xyz);
            hitPoint = ray_o + ray_d * hit_t;
            spherei = sphere_index;
        }
    }
}

vec3 Trace(vec3 ray_o, vec3 ray_d, float seed)
{
    int maxBounceCount = 10;

    vec3 incomingLight = vec3(0.0);
    vec3 rayColor = vec3(1.0);

    //running values

    vec3 normal = vec3(0.0);
    vec3 hitPoint = vec3(0.0);
    bool hit = false;
    int spherei = 0;

    //--------------
    for (int i = 0; i < maxBounceCount; i++)
    {

        calculateRayCollision(ray_o, ray_d, normal, hitPoint, hit, spherei);

        if (hit)
        {
            ray_o = hitPoint;
            vec3 diffuseDir = normalize(normal + random_unit_vector(seed + i * 129837.828));
            vec3 specularDir = reflect(ray_d, normal);

            Material hit_mat = materials[int(spheres[spherei].materialData[0])];
            float specularProbability = hit_mat.data.z;
            float smoothness = hit_mat.data.y;
            float emissionStrength = hit_mat.data.x;

            float isSpecularBounce = 0.0;
            if (specularProbability > random(seed + i * 420883.387))
            {
                isSpecularBounce = 1.0;
            }

            ray_d = mix(diffuseDir, specularDir, smoothness * isSpecularBounce);

            vec3 emittedLight = hit_mat.emissionColor.rgb * emissionStrength;
            incomingLight += emittedLight * rayColor;
            rayColor = rayColor * mix(hit_mat.color.rgb, hit_mat.specularColor.rgb, isSpecularBounce);
        }
        else
        {
            incomingLight += getEnvironmentLight(ray_d, ray_o);
            break;
        }
    }

    return incomingLight;
}

void main()
{
    int raysPerPixel = 10;

    vec3 pixel = vec3(0.0, 0.0, 0.0);
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    ivec2 dims = imageSize(imgOutput);

    vec3 up = vec3(0.0, 0.0, 0.8);
    vec3 right = vec3(0.8, 0.0, 0.0);

    vec3 forward = vec3(0.0, 1.0, 0.0);

    float x = float(pixel_coords.x) / dims.x - 0.5;
    float z = float(pixel_coords.y) / dims.y - 0.5 ;

    vec3 cam_o = vec3(0.0, 0.0, 0.0);
    vec3 ray_o = forward + right * x + up * z;
    vec3 ray_d = normalize(ray_o - cam_o);

    for (int rays = 0; rays < raysPerPixel; rays++) {
        pixel += Trace(ray_o, ray_d, /*frame * 10938.873 + */rays * 927 + pixel_coords.y * 109736 + pixel_coords.x * 128737);
    }
    pixel /= float(raysPerPixel);
    vec4 final_color = ACESFilmCol(pixel);

    //if(random(t * 2837.8263 + pixel_coords.y * 109736 + pixel_coords.x * 128737) > 0.5) { pixel = vec3(1.0); }
    imageStore(imgOutput, pixel_coords, final_color);
}


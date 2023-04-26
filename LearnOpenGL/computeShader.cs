

#version 430 core

layout(local_size_x = 10, local_size_y = 10, local_size_z = 1) in;

struct Material
{
    vec4 color;
    vec4 emissionColor;
    vec4 specularColor;

    vec4 data; //{emissionStrength, smoothness, specularProbability, unused}
};

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
layout(location = 1) uniform int frame;
layout(location = 2) uniform int accumulate;

// ----------------------------------------------------------------------------
//
// functions
//
// ----------------------------------------------------------------------------


uint NextRandom(inout uint state)
{
    state = state * 747796405 + 2891336453;
    uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    result = (result >> 22) ^ result;
    return result;
}

float random(inout uint state)
{
    return NextRandom(state) / 4294967295.0; // 2^32 - 1
}

vec3 random_unit_vector(inout uint state)
{
    float safety = 100.0;
    vec3 ret;
    for(float i = 0; i < safety; i += 1.0)
    {
        float x = random(state) * 2.0 - 1.0;
        float y = random(state) * 2.0 - 1.0;
        float z = random(state) * 2.0 - 1.0;
        ret = vec3(x,y,z);
        if(length(ret) <= 1.0)
        {
            return normalize(ret);
        }
    }
    return normalize(ret);
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

float hit_sphere(vec3 ray_o, vec3 ray_d, int sphere_ind)
{
    vec3 sphere_p = spheres[sphere_ind].data.xyz;
    float sphere_r = spheres[sphere_ind].data.w;

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
        return ((-b - sqrt(discriminant)) / (2.0 * a));
    }
}

void calculateRayCollision(vec3 ray_o, vec3 ray_d, inout vec3 normal, inout vec3 hitPoint, inout bool hit, out int materialIndex)
{
    float t = 1. / 0.;

    for (int sphere_index = 0; sphere_index < 5; sphere_index++) {
        float hit_t = hit_sphere(ray_o, ray_d, sphere_index);
        if (hit_t > 0.000001 && hit_t < t)
        {
            vec3 sphere_p = spheres[sphere_index].data.xyz;
            vec3 potential_normal = normalize(ray_o + (hit_t * ray_d) - sphere_p);
            if(dot(potential_normal, ray_d) > 0.0) { continue; }
            hit = true;
            t = hit_t;
            normal = potential_normal;
            hitPoint = ray_o + hit_t * ray_d;
            materialIndex = int(spheres[sphere_index].materialData.x);
        }
    }

    return;
}

vec3 Trace(vec3 ray_o, vec3 ray_d, inout uint state)
{
    int maxBounceCount = 5;

    vec3 incomingLight = vec3(0.0);
    vec3 rayColor = vec3(1.0);

    //running values

    vec3 normal = vec3(0.0);
    vec3 hitPoint = vec3(0.0);
    bool hit = false;
    int materialInd;

    //--------------
    for (int i = 0; i <= maxBounceCount; i++)
    {
        hit = false;
        calculateRayCollision(ray_o, ray_d, normal, hitPoint, hit, materialInd);

        if (hit)
        {
            ray_o = hitPoint;
            vec3 diffuseDir = normalize(normal + random_unit_vector(state));

            vec3 specularDir = normalize(reflect(ray_d, normal));

            Material hit_mat = materials[materialInd];
            float specularProbability = hit_mat.data.z;
            float smoothness = hit_mat.data.y;
            float emissionStrength = hit_mat.data.x;

            float isSpecularBounce = 0.0;
            if (specularProbability > random(state))
            {
                isSpecularBounce = 1.0;
            }

            ray_d = mix(diffuseDir, specularDir, (smoothness * isSpecularBounce));

            vec3 emittedLight = hit_mat.emissionColor.rgb * emissionStrength;
            incomingLight += emittedLight * rayColor;
            rayColor = rayColor * mix(hit_mat.color.rgb, hit_mat.specularColor.rgb, isSpecularBounce);
        }
        else
        {
            break;
        }
    }

    return incomingLight;
}

void main()
{
    int raysPerPixel = 60;

    vec3 pixel = vec3(0.0, 0.0, 0.0);
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    ivec2 dims = imageSize(imgOutput);

    uint pixelIndex = pixel_coords.y * dims.x + pixel_coords.x;
    uint randomState = pixelIndex + frame * 719393;

    vec3 up = vec3(0.0, 0.0, dims.y / 1000.0);
    vec3 right = vec3(dims.x / 1000.0, 0.0, 0.0);

    vec3 forward = vec3(0.0, 1.0, 0.0);

    for (int rays = 0; rays < raysPerPixel; rays++) {
        float x = float(pixel_coords.x + random(randomState)) / dims.x - 0.5;
        float z = float(pixel_coords.y + random(randomState)) / dims.y - 0.5;

        vec3 cam_o = vec3(0.0, 0.0, 0.0);
        vec3 ray_o = forward + right * x + up * z;
        vec3 ray_d = normalize(ray_o - cam_o);

        pixel += Trace(cam_o, ray_d, randomState);
    }
    pixel /= float(raysPerPixel);
    vec4 final_color = ACESFilmCol(pixel);

    vec4 previous_color = vec4(0.0);

    if (accumulate == 1) {
        previous_color = imageLoad(imgOutput, pixel_coords).rgba;
    }

    imageStore(imgOutput, pixel_coords, final_color + previous_color);
}


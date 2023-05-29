

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

struct Triangle
{
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec4 materialData;
};

struct BVH
{
    vec4 minPoint;
    vec4 maxPoint;
    vec4 data; // {triangleIndex1, triangleIndex2, hit, miss}
};

layout (std140, binding = 4) buffer SphereBlock {
    Sphere spheres [10000];
};

layout(std140, binding = 5) buffer TriangleBlock
{
    Triangle triangles [100000];
};

layout (std140, binding = 6) buffer MaterialBlock {
    Material materials [100];
};

layout(std140, binding = 7) buffer CameraInfo
{
    vec4 camera_position;
    vec4 camera_direction;
    vec4 camera_data; //fov
};

layout(std140, binding = 8) buffer BVHBlock
{
    BVH heirarchy [100000];
};

layout(rgba32f, binding = 0) uniform image2D imgOutput;

layout(location = 0) uniform float t;                 /* Time */
layout(location = 1) uniform int frame;
layout(location = 2) uniform int numSpheres;
layout(location = 3) uniform int numTriangles;
layout(location = 4) uniform int numMaterials;
layout(location = 5) uniform int numNodes;
layout(location = 6) uniform int accumulate;
layout(location = 7) uniform int displayMode;


int maxBounceCount = 5;

const float PI = 3.141592;
const bool render_triangles = true;
const bool render_spheres = true;
const bool antiAlias = true;

// Environment Settings
bool EnvironmentEnabled = true;




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

float GPURnd(inout vec4 state)
{
    const vec4 q = vec4( 1225.0, 1585.0, 2457.0, 2098.0);
    const vec4 r = vec4( 1112.0, 367.0, 92.0, 265.0);
    const vec4 a = vec4( 3423.0, 2646.0, 1707.0, 1999.0);
    const vec4 m = vec4(4194287.0, 4194277.0, 4194191.0, 4194167.0);

    vec4 beta = floor(state / q);
    vec4 p = a * (state - beta * q) - beta * r;
    beta = (sign(-p) + vec4(1.0)) * vec4(0.5) * m;
    state = (p + beta);

    return fract(dot(state / m, vec4(1.0, -1.0, 1.0, -1.0)));
}

float RandomValueNormalDistribution(inout uint state)
{
    float theta = 2 * 3.1415926 * random(state);
    float rho = sqrt(-2 * log(random(state)));
    return rho * cos(theta);
}

vec3 random_unit_vector(inout uint state)
{
    vec3 ret = vec3(
        RandomValueNormalDistribution(state),
        RandomValueNormalDistribution(state), 
        RandomValueNormalDistribution(state));
    return normalize(ret);
}

vec3 getEnvironmentLight(vec3 ray_d)
{
    if (!EnvironmentEnabled) {
        return vec3(0);
    }

    vec3 dir = normalize(ray_d);
    float t = 0.5 * (dir.z + 1.0);
    return (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
}

// Experimenting
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x * 34.0) + 1.0) * x); }


float snoise(vec2 v)
{

    // Precompute values for skewed triangular grid
    const vec4 C = vec4(0.211324865405187,
                        // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,
                        // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626,
                        // -1.0 + 2.0 * C.x
                        0.024390243902439);
    // 1.0 / 41.0

    // First corner (x0)
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);

    // Other two corners (x1, x2)
    vec2 i1 = vec2(0.0);
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 x1 = x0.xy + C.xx - i1;
    vec2 x2 = x0.xy + C.zz;

    // Do some permutations to avoid
    // truncation effects in permutation
    i = mod289(i);
    vec3 p = permute(
            permute(i.y + vec3(0.0, i1.y, 1.0))
                + i.x + vec3(0.0, i1.x, 1.0));

    vec3 m = max(0.5 - vec3(
                        dot(x0, x0),
                        dot(x1, x1),
                        dot(x2, x2)
                        ), 0.0);

    m = m * m;
    m = m * m;

    // Gradients:
    //  41 pts uniformly over a line, mapped onto a diamond
    //  The ring size 17*17 = 289 is close to a multiple
    //      of 41 (41*7 = 287)

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt(a0*a0 + h*h);
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

    // Compute final noise value at P
    vec3 g = vec3(0.0);
    g.x = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * vec2(x1.x, x2.x) + h.yz * vec2(x1.y, x2.y);
    return (130.0 * dot(m, g)) * 0.5 + 0.5;
}
// End Experimenting

float hit_sphere(vec3 ray_o, vec3 ray_d, int sphere_ind)
{
    vec3 sphere_p = spheres[sphere_ind].data.xyz;
    float sphere_r = spheres[sphere_ind].data.w;

    vec3 oc = ray_o - sphere_p;
    float a = dot(ray_d, ray_d);
    float half_b = dot(oc, ray_d);
    float c = dot(oc, oc) - sphere_r * sphere_r;
    float discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0f)
    {
        return -1.0;
    }else{
        return ((-half_b - sqrt(discriminant)) / a);
    }
}

float RayIntersectsTriangle(vec3 ray_o,
    vec3 ray_d,
    int triangle_ind,
    out vec3 normal)
{
    const float EPSILON = 0.0000001;
    Triangle test = triangles[triangle_ind];
    vec3 vertex0 = test.v0.xyz;
    vec3 vertex1 = test.v1.xyz;
    vec3 vertex2 = test.v2.xyz;
    vec3 edge1, edge2, h, s, q;
    float a, f, u, v;
    edge1 = vertex1 - vertex0;
    edge2 = vertex2 - vertex0;
    h = cross(ray_d, edge2);
    a = dot(edge1, h);

    if (a > -EPSILON && a < EPSILON)
        return -1.0;    // This ray is parallel to this triangle.

    f = 1.0 / a;
    s = ray_o - vertex0;
    u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return -1.0;

    q = cross(s, edge1);
    v = f * dot(ray_d, q);

    if (v < 0.0 || u + v > 1.0)
        return -1.0;

    // At this stage we can compute t to find out where the intersection point is on the line.
    float t = f * dot(edge2, q);

    if (t > EPSILON) // ray intersection
    {
        //intersection_point = ray_o + ray_d * t;
        normal = normalize(cross(edge1, edge2));
        return t;
    }
    else // This means that there is a line intersection but not a ray intersection.
        return -1.0;
}

float hit_triangle(vec3 ray_o, vec3 ray_d, int triangle_ind, out vec3 normal)
{
    Triangle test = triangles[triangle_ind];

    vec3 v0 = test.v0.xyz;
    vec3 v1 = test.v1.xyz;
    vec3 v2 = test.v2.xyz;

    vec3 a = v1 - v0; // edge 0
    vec3 b = v2 - v0; // edge 1
    vec3 n = cross(a, b); // this is the triangle's normal
    n = normalize(n);

    normal = n;

    float d = -dot(n, v0);
    float t = -(dot(n, ray_o) + d) / dot(n, ray_d);

    if (t < 0) { return -1.0; }

    vec3 p = ray_o + t * ray_d;

    vec3 edge0 = v1 - v0;
    vec3 edge1 = v2 - v1;
    vec3 edge2 = v0 - v2;
    vec3 C0 = p - v0;
    vec3 C1 = p - v1;
    vec3 C2 = p - v2;
    if (dot(n, cross(edge0, C0)) > 0 &&
        dot(n, cross(edge1, C1)) > 0 &&
        dot(n, cross(edge2, C2)) > 0) return t; // P is inside the triangle

    return -1.0;
}

bool bvh_intersect(BVH b, vec3 ray_o, vec3 ray_d, float cur_t)
{
    float tmin = (b.minPoint.x - ray_o.x) / ray_d.x;
    float tmax = (b.maxPoint.x - ray_o.x) / ray_d.x;

    if (tmin > tmax)
    {
        float temp = tmin;
        tmin = tmax;
        tmax = temp;
        //swap(tmin, tmax);
    }

    float tymin = (b.minPoint.y - ray_o.y) / ray_d.y;
    float tymax = (b.maxPoint.y - ray_o.y) / ray_d.y;

    if (tymin > tymax)
    {
        float temp = tymin;
        tymin = tymax;
        tymax = temp;
    }

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    float tzmin = (b.minPoint.z - ray_o.z) / ray_d.z;
    float tzmax = (b.maxPoint.z - ray_o.z) / ray_d.z;

    if (tzmin > tzmax)
    {
        float temp = tzmin;
        tzmin = tzmax;
        tzmax = temp;
    }

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    if (tzmax < tmax)
        tmax = tzmax;

    if (tmin > cur_t) {
        return false;
    }

    return true;
}

void calculateRayCollision(vec3 ray_o, vec3 ray_d, inout vec3 normal, inout vec3 hitPoint, inout bool hit, out int materialIndex)
{
    float t = 1. / 0.;

    if (render_spheres) {
        for (int sphere_index = 0; sphere_index < numSpheres; sphere_index++) {
            float hit_t = hit_sphere(ray_o, ray_d, sphere_index);
            if (hit_t > 0.0001 && hit_t < t)
            {
                vec3 sphere_p = spheres[sphere_index].data.xyz;
                vec3 potential_normal = normalize(ray_o + (hit_t * ray_d) - sphere_p);
                if (dot(potential_normal, ray_d) > 0.0f) { potential_normal = -1.0 * potential_normal; }//continue; }
                hit = true;
                t = hit_t;
                normal = potential_normal;
                hitPoint = ray_o + hit_t * ray_d;
                materialIndex = int(spheres[sphere_index].materialData.x);
            }
        }
    }


    if(!render_triangles){return;}

    vec3 running_normal, running_normal2;
    //for(int bvh_ind = numNodes - 1; bvh_ind > -1;)
    for (int bvh_ind = 0; bvh_ind > -1;)
    {
        BVH b = heirarchy[bvh_ind];

        bool hit_box = bvh_intersect(b, ray_o, ray_d, t);

        int next_index;
        if (hit_box){
            next_index = int(b.data.z);
        }else{
            next_index = int(b.data.w);
        }

        if(hit_box && (b.data.x > -1))
        {
            float hit_t = hit_triangle(ray_o, ray_d, int(b.data[0]), running_normal);
            float hit_t2 = hit_triangle(ray_o, ray_d, int(b.data[1]), running_normal2);

            if (hit_t > 0.0001 && hit_t < t && (hit_t < hit_t2 || hit_t2 < 0.0001))
            {
                if (dot(running_normal, ray_d) > 0.0) { running_normal = -1.0 * running_normal; }
                hit = true;
                t = hit_t;
                normal = running_normal;
                hitPoint = ray_o + (hit_t * ray_d);
                materialIndex = int(triangles[int(b.data[0])].materialData.x);
            }
            else if (hit_t2 > 0.0001 && hit_t2 < t)
            {
                if (dot(running_normal2, ray_d) > 0.0) { running_normal2 = -1.0 * running_normal2; }
                hit = true;
                t = hit_t2;
                normal = running_normal2;
                hitPoint = ray_o + (hit_t2 * ray_d);
                materialIndex = int(triangles[int(b.data[1])].materialData.x);
            }
        }
        bvh_ind = next_index;
    }
}

vec3 Trace(vec3 ray_o, vec3 ray_d, inout uint state)
{
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

        if (hit && length(rayColor) > 0.01) //dark colors will not gain from more bounces
        {
            if(displayMode == 2)
            {
                vec3 normalColor = (normal + 1.0) * 0.5;
                return normalColor;
            }
            if(displayMode == 4)
            {
                float s = length(hitPoint - ray_o);
                float distance = (1.0 - sqrt(s + 1) / (s + 1.0));
                vec3 distanceColor = vec3(distance * distance);
                return distanceColor;
            }

            ray_o = hitPoint;
            vec3 diffuseDir = normalize(normal + random_unit_vector(state));


            vec3 specularDir = normalize(reflect(ray_d, normal));

            Material hit_mat = materials[materialInd];
            float specularProbability = hit_mat.data.z;
            float smoothness = hit_mat.data.y;
            float emissionStrength = hit_mat.data.x;

            if (displayMode == 3) {
                vec3 objectColor = hit_mat.color.rgb;
                return objectColor;
            }

            float isSpecularBounce = 0.0;
            if (specularProbability > random(state))
            {
                isSpecularBounce = 1.0;
            }

            ray_d = mix(diffuseDir, specularDir, (smoothness * isSpecularBounce));

            vec3 emittedLight = hit_mat.emissionColor.rgb * emissionStrength;
            incomingLight += emittedLight * rayColor;
            rayColor = rayColor * mix(hit_mat.color.rgb, hit_mat.specularColor.rgb, isSpecularBounce);
        }else{
            incomingLight += getEnvironmentLight(ray_d) * rayColor;
            break;
        }
    }

    return incomingLight;
}



void main()
{
    int raysPerPixel = 1;

    vec3 pixel = vec3(0.0, 0.0, 0.0);
    ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

    ivec2 dims = imageSize(imgOutput);

    uint pixelIndex = pixel_coords.y * 831266 + pixel_coords.x * 923766;
    uint randomState = pixelIndex + frame * 719393;

    float zoom = 10.0;

    vec3 forward = normalize(camera_direction.xyz);
    vec3 up = vec3(0.0, 0.0, 1.0);
    vec3 camRight = normalize(cross(forward, up));
    vec3 camUp = normalize(cross(camRight, forward)) * float(dims.y) / float(dims.x);

    //vec3 up = vec3(0.0, 0.0, float(dims.y) / (dims.x * zoom));
    //vec3 right = vec3(float(dims.x) / (dims.x * zoom) , 0.0, 0.0);

    vec3 cam_o = camera_position.xyz;//vec3(0.0, -6.0, 1.0);

    for (int rays = 0; rays < raysPerPixel; rays++) {
        float antiAX = 0, antiAY = 0;
        if (antiAlias)
        {
            antiAX = random(randomState);
            antiAY = random(randomState);
        }
        float x = float(pixel_coords.x + antiAX) / dims.x - 0.5;
        float z = float(pixel_coords.y + antiAY) / dims.y - 0.5;

        vec3 ray_d = forward + camRight * x + camUp * z;
        ray_d = normalize(ray_d);

        pixel += Trace(cam_o, ray_d, randomState);
    }
    pixel /= float(raysPerPixel);

    vec4 final_color = vec4(pixel, 1.0); 

    if (accumulate == 1) {
        vec4 previous_color = imageLoad(imgOutput, pixel_coords).rgba;
        final_color = (previous_color * ((float(frame) - 1.0) / float(frame))) + (vec4(pixel, 1.0) / float(frame));
    }

    imageStore(imgOutput, pixel_coords, final_color);
}


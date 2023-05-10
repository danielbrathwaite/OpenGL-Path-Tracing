

#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D tex;

//layout(location = 1) uniform int var_name;

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

void main()
{
    vec3 texCol = texture(tex, TexCoords).rgb;
    vec4 color_graded = ACESFilmCol(texCol);
    FragColor = color_graded;
}


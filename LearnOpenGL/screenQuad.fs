

#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D tex;

layout(location = 1) uniform int frame;

void main()
{             
    vec3 texCol = texture(tex, TexCoords).rgb / float(frame);      
    FragColor = vec4(texCol, 1.0);
}


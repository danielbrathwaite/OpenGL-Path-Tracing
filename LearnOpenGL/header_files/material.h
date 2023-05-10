#ifndef MATERIAL_H
#define MATERIAL_H


struct Material
{
	glm::vec4 color;
	glm::vec4 emissionColor;
	glm::vec4 specularColor;

	glm::vec4 data; //{ emissionStrength, smoothness, specularProbability, unused }
};


#endif
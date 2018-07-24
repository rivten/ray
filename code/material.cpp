#pragma once

struct material
{
	v3 Albedo;
	float Attenuation;
	float Scatter; // NOTE(hugo): 0 : No scatter (full specular), 1 : full diffuse
	bool IsLight;
	v3 Emissivity;
};


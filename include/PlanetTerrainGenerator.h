#pragma once

#include <glm/glm.hpp>

#include "PlanetRenderer.h"

namespace PlanetTerrainGenerator
{
    float terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
}

#ifndef CORE_CAMERA_HPP
#define CORE_CAMERA_HPP

#include <glm/glm.hpp>
#include <util/types.h>
#include <util/random.hpp>
#include <core/ray.hpp>

namespace core {

struct Camera {
    glm::vec3 origin, lowerLeft, horizontal, vertical;
    glm::vec3 u, v, w;
    float32   lensRadius;
};

Camera makeCamera(glm::vec3 lookFrom, glm::vec3 lookAt, glm::vec3 up,
                  float32 vfov, float32 aspect,
                  float32 aperture  = 0.0f,
                  float32 focusDist = 1.0f);

Ray generateRay(const Camera& cam, float32 s, float32 t, PCG32& rng);

} // namespace core

#endif // CORE_CAMERA_HPP

#ifndef CORE_RAY_HPP
#define CORE_RAY_HPP

// Lib includes
#include <glm/glm.hpp>

// Local includes
#include <types.h>

namespace core {

struct Ray {
    glm::vec3 org, dir;
    Ray() = default;
    Ray(const glm::vec3& org, const glm::vec3& dir);
    glm::vec3 at(float32 t) const;
};

} // namespace core

#endif // CORE_RAY_HPP
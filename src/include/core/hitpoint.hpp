#ifndef CORE_HITPOINT_HPP
#define CORE_HITPOINT_HPP

// Lib includes
#include <glm/glm.hpp>

// Local includes
#include <types.h>

namespace core {

struct Hitpoint {
    float32 t;
    glm::vec3 p, n;
    bool exit;
    void setNormal(const glm::vec3& rayDir, const glm::vec3& normal);
};

} // namespace core

#endif // CORE_HITPOINT_HPP
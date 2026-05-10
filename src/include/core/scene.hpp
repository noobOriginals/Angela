#ifndef CORE_SCENE_HPP
#define CORE_SCENE_HPP

#include <vector>
#include <util/types.h>
#include <core/object.hpp>
#include <core/material.hpp>

namespace core {

struct Scene {
    std::vector<Object>   objects;
    std::vector<Material> materials;

    int32 addMaterial(const Material& m);
    void  addObject  (const Object& o);
    bool  hit(const Ray&, HitRecord&, float32 tMin, float32 tMax) const;
};

} // namespace core

#endif // CORE_SCENE_HPP

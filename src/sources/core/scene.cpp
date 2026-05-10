#include <core/scene.hpp>

namespace core {

int32 Scene::addMaterial(const Material& m) {
    materials.push_back(m);
    return (int32)materials.size() - 1;
}

void Scene::addObject(const Object& o) {
    objects.push_back(o);
}

bool Scene::hit(const Ray& ray, HitRecord& hr, float32 tMin, float32 tMax) const {
    HitRecord temp;
    bool      hitAny  = false;
    float32   closest = tMax;

    for (const Object& obj : objects) {
        if (hitObject(ray, temp, tMin, closest, obj)) {
            hitAny  = true;
            closest = temp.t;
            hr      = temp;
        }
    }

    return hitAny;
}

} // namespace core

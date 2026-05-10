#include <core/scene.hpp>

#include <iostream>
#include <stb_image/stb_image.h>

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

int32 Scene::addImageTexture(const std::string& path) {
    auto it = textureCache.find(path);
    if (it != textureCache.end()) return it->second;

    int w, h, c;
    uint8* raw = stbi_load(path.c_str(), &w, &h, &c, 0);
    if (!raw) {
        std::cerr << "Failed to load texture: " << path << "\n";
        // Solid magenta fallback to make missing textures visible
        Texture tex{};
        tex.type    = TEX_SOLID;
        tex.solid[0] = 1.0f; tex.solid[1] = 0.0f; tex.solid[2] = 1.0f;
        textures.push_back(tex);
        int32 idx = (int32)textures.size() - 1;
        textureCache[path] = idx;
        return idx;
    }

    texturePixels.emplace_back(raw, raw + (size_t)w * h * c);
    stbi_image_free(raw);

    Texture tex{};
    tex.type     = TEX_IMAGE;
    tex.width    = w;
    tex.height   = h;
    tex.channels = c;
    tex.pixels   = texturePixels.back().data();

    textures.push_back(tex);
    int32 idx = (int32)textures.size() - 1;
    textureCache[path] = idx;
    return idx;
}

} // namespace core

#ifndef CORE_SCENE_HPP
#define CORE_SCENE_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <util/types.h>
#include <core/object.hpp>
#include <core/material.hpp>
#include <core/texture.hpp>
#include <core/bvh.hpp>

namespace core {

struct Scene {
    std::vector<Object>   objects;
    std::vector<Material> materials;
    std::vector<Texture>  textures;

    // BVH — populated by buildBVH(). Must be called before rendering.
    std::vector<BVHNode> bvhNodes;
    std::vector<int32>   primIndices;

    int32 addMaterial(const Material& m);
    void  addObject  (const Object& o);
    bool  hit(const Ray&, HitRecord&, float32 tMin, float32 tMax) const;

    void  buildBVH();

    // Loads image from disk (stb_image). Returns existing index if path was seen before.
    int32 addImageTexture(const std::string& path);

private:
    std::vector<std::vector<uint8>>      texturePixels;
    std::unordered_map<std::string,int32> textureCache;
};

} // namespace core

#endif // CORE_SCENE_HPP

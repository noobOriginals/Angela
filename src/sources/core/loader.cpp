#include <core/loader.hpp>

#include <algorithm>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

namespace core {

bool loadOBJ(Scene& scene, const std::string& objPath, const std::string& mtlDir) {
    tinyobj::ObjReaderConfig config;
    config.mtl_search_path = mtlDir;
    config.triangulate     = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objPath, config)) {
        std::cerr << "OBJ error: " << reader.Error() << "\n";
        return false;
    }
    if (!reader.Warning().empty())
        std::cerr << "OBJ warning: " << reader.Warning() << "\n";

    const auto& attrib    = reader.GetAttrib();
    const auto& shapes    = reader.GetShapes();
    const auto& objMats   = reader.GetMaterials();

    // Build material map: tinyobj index → scene material index
    std::vector<int32> matMap((int32)objMats.size(), 0);
    for (size_t i = 0; i < objMats.size(); ++i) {
        const auto& m = objMats[i];

        Material mat;
        setDiffuse(mat, glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]));

        if (!m.diffuse_texname.empty()) {
            std::string texPath = mtlDir + "/" + m.diffuse_texname;
            // Normalise Windows-style separators from the MTL
            std::replace(texPath.begin(), texPath.end(), '\\', '/');
            mat.albedoTexture = scene.addImageTexture(texPath);
        }

        matMap[i] = scene.addMaterial(mat);
    }

    // Default material (fallback for faces with matId == -1)
    Material defaultMat;
    setDiffuse(defaultMat, glm::vec3(0.8f));
    int32 defaultMatIdx = scene.addMaterial(defaultMat);

    // Build triangle objects
    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            // triangulate=true guarantees 3 vertices per face
            glm::vec3 verts[3];
            glm::vec2 uvs[3];

            for (int v = 0; v < 3; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                verts[v] = glm::vec3(
                    attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 2]
                );

                if (idx.texcoord_index >= 0) {
                    uvs[v] = glm::vec2(
                        attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                        attrib.texcoords[2 * size_t(idx.texcoord_index) + 1]
                    );
                } else {
                    uvs[v] = glm::vec2(0.0f);
                }
            }

            int matId = shape.mesh.material_ids[f];
            int32 sceneMat = (matId >= 0 && matId < (int)matMap.size())
                             ? matMap[matId]
                             : defaultMatIdx;

            Object tri;
            setTriangle(tri, verts[0], verts[1], verts[2],
                        uvs[0], uvs[1], uvs[2], sceneMat);
            scene.addObject(tri);

            indexOffset += 3;
        }
    }

    std::cout << "Loaded " << scene.objects.size() << " triangles, "
              << scene.materials.size() << " materials, "
              << scene.textures.size()  << " textures\n";
    return true;
}

} // namespace core

#include <iostream>

#include <core/scene.hpp>
#include <core/camera.hpp>
#include <core/loader.hpp>
#include <renderer/pathtracer.hpp>
#include <lib/image.hpp>

int main() {
    // Low resolution / spp — 262k triangles without BVH make full-res very slow.
    // Raise these after Phase 4 (BVH) is in place.
    const int32 W = 400;
    const int32 H = 225;

    core::Scene scene;
    if (!core::loadOBJ(scene, "assets/Sponza/sponza.obj", "assets/Sponza"))
        return 1;

    // Looking along the long axis of the atrium from near one end
    core::Camera cam = core::makeCamera(
        glm::vec3(-1100.0f, 200.0f,  0.0f),
        glm::vec3(  500.0f, 100.0f,  0.0f),
        glm::vec3(    0.0f,   1.0f,  0.0f),
        60.0f, (float32)W / (float32)H
    );

    RenderConfig cfg;
    cfg.width      = W;
    cfg.height     = H;
    cfg.spp        = 4;
    cfg.maxDepth   = 6;
    cfg.tileSize   = 32;
    cfg.numThreads = 0;

    Image out(W, H);
    CPUPathTracer renderer;

    std::cout << "Rendering " << W << "x" << H
              << " @ " << cfg.spp << " spp  depth=" << cfg.maxDepth << "\n";
    renderer.render(scene, cam, cfg, out);

    out.savePNG("render.png", cfg.spp);
    std::cout << "Saved render.png\n";

    return 0;
}

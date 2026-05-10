#include <iostream>

#include <core/scene.hpp>
#include <core/camera.hpp>
#include <renderer/pathtracer.hpp>
#include <lib/image.hpp>

int main() {
    const int32 W = 800;
    const int32 H = 450;

    // Scene
    core::Scene scene;

    core::Material groundMat, sphereMat;
    core::setDiffuse(groundMat, glm::vec3(0.5f, 0.5f, 0.5f));
    core::setDiffuse(sphereMat, glm::vec3(0.8f, 0.3f, 0.3f));
    int32 groundIdx = scene.addMaterial(groundMat);
    int32 sphereIdx = scene.addMaterial(sphereMat);

    core::Object ground, sphere;
    core::setSphere(ground, glm::vec3(0.0f, -100.5f, -1.0f), 100.0f, groundIdx);
    core::setSphere(sphere, glm::vec3(0.0f,    0.0f, -1.0f),   0.5f, sphereIdx);
    scene.addObject(ground);
    scene.addObject(sphere);

    // Camera
    core::Camera cam = core::makeCamera(
        glm::vec3(0.0f, 0.5f,  2.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f,  0.0f),
        45.0f, (float32)W / (float32)H
    );

    // Render
    RenderConfig cfg;
    cfg.width      = W;
    cfg.height     = H;
    cfg.spp        = 64;
    cfg.maxDepth   = 16;
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

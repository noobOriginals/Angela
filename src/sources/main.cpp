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

    // Materials
    core::Material groundMat, leftMat, centerMat, rightMat, backMat, triMat;
    core::setDiffuse   (groundMat, glm::vec3(0.48f, 0.48f, 0.48f));
    core::setDiffuse   (leftMat,   glm::vec3(0.8f,  0.2f,  0.2f));
    core::setDielectric(centerMat, 1.5f);
    core::setMetal     (rightMat,  glm::vec3(0.8f,  0.7f,  0.3f), 0.05f);
    core::setDiffuse   (backMat,   glm::vec3(0.3f,  0.6f,  0.8f));
    core::setDiffuse   (triMat,    glm::vec3(0.2f,  0.8f,  0.3f));

    int32 groundIdx = scene.addMaterial(groundMat);
    int32 leftIdx   = scene.addMaterial(leftMat);
    int32 centerIdx = scene.addMaterial(centerMat);
    int32 rightIdx  = scene.addMaterial(rightMat);
    int32 backIdx   = scene.addMaterial(backMat);
    int32 triIdx    = scene.addMaterial(triMat);

    // Objects
    core::Object ground, left, center, right, backWall, tri;

    // Ground — large sphere
    core::setSphere(ground, glm::vec3(0.0f, -100.5f, -1.0f), 100.0f, groundIdx);

    // Three spheres: diffuse / glass / metal
    core::setSphere(left,   glm::vec3(-1.1f, 0.0f, -1.0f), 0.5f, leftIdx);
    core::setSphere(center, glm::vec3( 0.0f, 0.0f, -1.0f), 0.5f, centerIdx);
    core::setSphere(right,  glm::vec3( 1.1f, 0.0f, -1.0f), 0.5f, rightIdx);

    // Back quad — blue backdrop
    core::setQuad(backWall,
        glm::vec3(0.0f, 0.5f, -2.5f),   // center
        glm::vec3(4.0f, 0.0f,  0.0f),   // u  (horizontal)
        glm::vec3(0.0f, 2.0f,  0.0f),   // v  (vertical)
        backIdx);

    // Floating triangle — green
    core::setTriangle(tri,
        glm::vec3(-0.35f, 0.6f, -0.6f),
        glm::vec3( 0.35f, 0.6f, -0.6f),
        glm::vec3( 0.0f,  1.2f, -0.6f),
        triIdx);

    scene.addObject(ground);
    scene.addObject(left);
    scene.addObject(center);
    scene.addObject(right);
    scene.addObject(backWall);
    scene.addObject(tri);

    // Camera
    core::Camera cam = core::makeCamera(
        glm::vec3(0.0f, 0.5f,  2.5f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f,  0.0f),
        40.0f, (float32)W / (float32)H
    );

    // Render
    RenderConfig cfg;
    cfg.width      = W;
    cfg.height     = H;
    cfg.spp        = 128;
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

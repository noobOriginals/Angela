#include <iostream>

#include <core/scene.hpp>
#include <core/camera.hpp>
#include <core/loader.hpp>
#include <renderer/pathtracer.hpp>
#include <lib/image.hpp>

// ---------------------------------------------------------------------------
// Cornell-box-style scene to exercise emissive materials and NEE+MIS.
// All geometry is added directly — no OBJ loading needed.
//
// Layout (units):
//   Floor / ceiling / back wall at y=0, y=555, z=555
//   Left (red) wall at x=0, right (green) wall at x=555
//   Light: small emissive quad flush with the ceiling
//   Two diffuse spheres on the floor
//
// Switch the #if to 1 for Sponza, 0 for this test scene.

static void makeCornellBox(core::Scene& scene) {
    core::Material white, red, green, light;
    core::setDiffuse(white, glm::vec3(0.73f));
    core::setDiffuse(red,   glm::vec3(0.65f, 0.05f, 0.05f));
    core::setDiffuse(green, glm::vec3(0.12f, 0.45f, 0.15f));
    core::setEmissive(light, glm::vec3(1.0f, 0.9f, 0.7f), 15.0f);

    int32 mW = scene.addMaterial(white);
    int32 mR = scene.addMaterial(red);
    int32 mG = scene.addMaterial(green);
    int32 mL = scene.addMaterial(light);

    // Floor — two triangles
    glm::vec3 f0(0,0,0), f1(555,0,0), f2(555,0,555), f3(0,0,555);
    core::Object t; core::setTriangle(t, f0, f1, f2, mW); scene.addObject(t);
                    core::setTriangle(t, f0, f2, f3, mW); scene.addObject(t);

    // Ceiling — two triangles
    glm::vec3 c0(0,555,0), c1(555,555,0), c2(555,555,555), c3(0,555,555);
    core::setTriangle(t, c0, c2, c1, mW); scene.addObject(t);
    core::setTriangle(t, c0, c3, c2, mW); scene.addObject(t);

    // Back wall (z=555)
    glm::vec3 b0(0,0,555), b1(555,0,555), b2(555,555,555), b3(0,555,555);
    core::setTriangle(t, b0, b1, b2, mW); scene.addObject(t);
    core::setTriangle(t, b0, b2, b3, mW); scene.addObject(t);

    // Left wall — red (x=0)
    glm::vec3 l0(0,0,0), l1(0,0,555), l2(0,555,555), l3(0,555,0);
    core::setTriangle(t, l0, l1, l2, mR); scene.addObject(t);
    core::setTriangle(t, l0, l2, l3, mR); scene.addObject(t);

    // Right wall — green (x=555)
    glm::vec3 r0(555,0,0), r1(555,555,0), r2(555,555,555), r3(555,0,555);
    core::setTriangle(t, r0, r1, r2, mG); scene.addObject(t);
    core::setTriangle(t, r0, r2, r3, mG); scene.addObject(t);

    // Ceiling light (two triangles, slightly below ceiling at y=554)
    glm::vec3 p0(213,554,227), p1(343,554,227), p2(343,554,332), p3(213,554,332);
    core::setTriangle(t, p0, p1, p2, mL); scene.addObject(t);
    core::setTriangle(t, p0, p2, p3, mL); scene.addObject(t);

    // Two spheres
    core::Object s;
    core::setSphere(s, glm::vec3(165, 100, 165), 100.0f, mW); scene.addObject(s);
    core::setSphere(s, glm::vec3(390, 100, 390), 100.0f, mW); scene.addObject(s);
}

int main() {
#if 0
    // --- Sponza ---
    const int32 W = 1920, H = 1080;
    core::Scene scene;
    if (!core::loadOBJ(scene, "assets/Sponza/sponza.obj", "assets/Sponza")) return 1;
    core::Camera cam = core::makeCamera(
        glm::vec3(-1100.0f, 200.0f, 0.0f),
        glm::vec3(  500.0f, 100.0f, 0.0f),
        glm::vec3(    0.0f,   1.0f, 0.0f),
        60.0f, (float32)W / (float32)H);
#else
    // --- Cornell box (emissive test) ---
    const int32 W = 800, H = 800;
    core::Scene scene;
    makeCornellBox(scene);
    core::Camera cam = core::makeCamera(
        glm::vec3(278.0f, 278.0f, -800.0f),
        glm::vec3(278.0f, 278.0f,    0.0f),
        glm::vec3(  0.0f,   1.0f,    0.0f),
        40.0f, (float32)W / (float32)H);
#endif

    std::cout << "Building BVH...\n";
    scene.buildBVH();
    std::cout << "BVH: " << scene.bvhNodes.size() << " nodes, "
              << scene.emissiveIds.size() << " emissives\n";

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
    out.savePNG("renders/render.png", cfg.spp);
    std::cout << "Saved render.png\n";
    return 0;
}

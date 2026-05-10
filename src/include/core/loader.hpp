#ifndef CORE_LOADER_HPP
#define CORE_LOADER_HPP

#include <string>
#include <core/scene.hpp>

namespace core {

// Loads an OBJ + its MTL into scene. mtlDir is the directory containing the MTL
// and the base path for relative texture references.
bool loadOBJ(Scene& scene, const std::string& objPath, const std::string& mtlDir);

} // namespace core

#endif // CORE_LOADER_HPP

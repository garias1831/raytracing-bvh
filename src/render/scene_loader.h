#ifndef SCENE_LOADER_H
#define SCENE_LOADER_H

#include "raytrace/hittables/hittable_list.h"
#include "renderer.h"

class SceneLoader {
    public:
        SceneLoader();

        /// @brief Load a scene with the specified ID.
        /// @param scene_id ID of the scene to load. Raises an error if larger than the max defined scene ID. 
        /// @param[out] world list of objects to be rendered
        /// @param[out] renderer renderer for the desired scene 
        void load(int scene_id, HittableList& world, Renderer& renderer) const;
};

#endif
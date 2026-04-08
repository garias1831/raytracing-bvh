#include "scene_loader.h"

#include "util/raytrace.h"
#include "raytrace/hittables/circle.h"


SceneLoader::SceneLoader() {}

void starter_circles(HittableList& world, Renderer& renderer) {
    uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	/* Specify custom lightsource location */
	renderer.set_source_loc(Point2(window_width / 2, window_height / 2));

	// Test Circles (Objects)
	world.add(make_shared<Circle>(Point2(70, 70), 30.0));
	world.add(make_shared<Circle>(Point2(150, 150), 50.0));
	world.add(make_shared<Circle>(Point2(350, 300), 50.0));
	world.add(make_shared<Circle>(Point2(500, 200), 60));
	world.add(make_shared<Circle>(Point2(550, 100), 10));
}

void SceneLoader::load(int scene_id, HittableList& world, Renderer& renderer) const {
    switch (scene_id) {
        case 1:
            starter_circles(world, renderer);
            break;
        default:
            throw std::invalid_argument("Exceeded maximum scene_id");
    }

}
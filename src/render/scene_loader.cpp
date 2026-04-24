#include "scene_loader.h"

#include "util/raytrace.h"
#include "raytrace/hittables/circle.h"


SceneLoader::SceneLoader() {}

void random_n(HittableList& world, Renderer& renderer, int n) {
    uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	/* Specify custom lightsource location */
    auto source_loc = Point2(window_width / 2, window_height / 2);
	renderer.set_source_loc(source_loc);
    
    int cx, cy;
    for (int c = 0; c < n; c++) {
        while(true) {
            cx = int(random_double(0, window_width));
            cy = int(random_double(0, window_height));
        
            // Don't generate circles too close to the light origin
            if (!Interval(0, source_loc.x() - 20).surrounds(cx) && 
                !Interval(source_loc.x() + 20, window_width).surrounds(cx)) {
                continue;
            }

            if (!Interval(0, source_loc.y() - 20).surrounds(cy) &&
                !Interval(source_loc.y() + 20, window_height).surrounds(cy)) {
                continue;
            }

            break;
        }

        world.add(make_shared<Circle>(Circle(Point2(cx, cy), 10)));
    }
}

void random_100(HittableList& world, Renderer& renderer) {
    random_n(world, renderer, 100);
}

void random_1k(HittableList& world, Renderer& renderer) {
    random_n(world, renderer, 1000);
}

void random_10k(HittableList& world, Renderer& renderer) {
    random_n(world, renderer, 10000);
}

void random_100k(HittableList& world, Renderer& renderer) {
    random_n(world, renderer, 100000);
}


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
        case 2:
            random_100(world, renderer);
            break;
        case 3:
            random_1k(world, renderer);
            break;
        case 4:
            random_10k(world, renderer);
            break;
        case 5:
            random_100k(world, renderer);
            break;
        default:
            throw std::invalid_argument("Exceeded maximum scene_id");
    }
}
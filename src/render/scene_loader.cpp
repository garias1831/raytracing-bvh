#include "scene_loader.h"

#include "util/raytrace.h"
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/rectangle.h"


SceneLoader::SceneLoader() {}

void configure_standard_window(Renderer& renderer) {
    renderer.set_window_width(700);
}

void configure_large_window(Renderer& renderer) {
    renderer.set_window_width(1400);
}

void random_100(HittableList& world, Renderer& renderer) {
    configure_standard_window(renderer);

    uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	/* Specify custom lightsource location */
    auto source_loc = Point2(window_width / 2, window_height / 2);
	renderer.set_source_loc(source_loc);
    
    int cx, cy;
    for (int c = 0; c < 100; c++) {
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

void random_dense(HittableList& world, Renderer& renderer) {
    configure_large_window(renderer);

    uint window_width = renderer.get_window_width();
    uint window_height = renderer.get_window_height();

    auto source_loc = Point2(window_width / 2, window_height / 2);
    renderer.set_source_loc(source_loc);

    int cx, cy;
    for (int c = 0; c < 2000; c++) {
        while (true) {
            cx = int(random_double(0, window_width));
            cy = int(random_double(0, window_height));

            // Keep the light origin from being immediately occluded.
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

        world.add(make_shared<Circle>(Circle(Point2(cx, cy), 4 + random_double(0, 10))));
    }
}

void random_10000(HittableList& world, Renderer& renderer) {
    configure_large_window(renderer);

    uint window_width = renderer.get_window_width();
    uint window_height = renderer.get_window_height();

    auto source_loc = Point2(window_width / 2, window_height / 2);
    renderer.set_source_loc(source_loc);

    int cx, cy;
    for (int c = 0; c < 10000; c++) {
        while (true) {
            cx = int(random_double(0, window_width));
            cy = int(random_double(0, window_height));

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

        world.add(make_shared<Circle>(Circle(Point2(cx, cy), 3 + random_double(0, 7))));
    }
}

void mixed_shapes(HittableList& world, Renderer& renderer) {
    configure_standard_window(renderer);
    random_100(world, renderer);

    world.add(make_shared<Rectangle>(Point2(120, 90), Point2(260, 180)));
    world.add(make_shared<Rectangle>(Point2(420, 220), Point2(620, 300)));
    world.add(make_shared<Rectangle>(Point2(860, 120), Point2(1040, 210)));
}

void starter_circles(HittableList& world, Renderer& renderer) {
    configure_standard_window(renderer);

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
            random_dense(world, renderer);
            break;
        case 4:
            mixed_shapes(world, renderer);
            break;
        case 5:
            random_10000(world, renderer);
            break;
        default:
            throw std::invalid_argument("Exceeded maximum scene_id");
    }

}

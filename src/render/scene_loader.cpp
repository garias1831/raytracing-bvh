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

void populate_random_circles(
    HittableList& world,
    Renderer& renderer,
    int primitive_count,
    double min_radius,
    double max_radius,
    bool use_large_window
) {
    if (use_large_window) {
        configure_large_window(renderer);
    } else {
        configure_standard_window(renderer);
    }

    uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	/* Specify custom lightsource location */
    auto source_loc = Point2(window_width / 2, window_height / 2);
	renderer.set_source_loc(source_loc);
    
    int cx, cy;
    for (int c = 0; c < primitive_count; c++) {
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

        world.add(make_shared<Circle>(Circle(Point2(cx, cy), min_radius + random_double(0, max_radius - min_radius))));
    }
}

void random_100(HittableList& world, Renderer& renderer) {
    populate_random_circles(world, renderer, 100, 10.0, 10.0, false);
}

void random_dense(HittableList& world, Renderer& renderer) {
    populate_random_circles(world, renderer, 2000, 4.0, 14.0, true);
}

void random_10000(HittableList& world, Renderer& renderer) {
    populate_random_circles(world, renderer, 10000, 3.0, 10.0, true);
}

void benchmark_random_scene(
    HittableList& world,
    Renderer& renderer,
    int primitive_count
) {
    double min_radius = 1.5;
    double max_radius = 5.0;

    if (primitive_count <= 1000) {
        min_radius = 3.0;
        max_radius = 9.0;
    } else if (primitive_count <= 10000) {
        min_radius = 2.0;
        max_radius = 6.0;
    } else if (primitive_count <= 25000) {
        min_radius = 1.5;
        max_radius = 5.0;
    } else if (primitive_count <= 50000) {
        min_radius = 1.25;
        max_radius = 4.0;
    } else {
        min_radius = 1.0;
        max_radius = 3.0;
    }

    populate_random_circles(world, renderer, primitive_count, min_radius, max_radius, true);
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
        case 6:
            benchmark_random_scene(world, renderer, 1000);
            break;
        case 7:
            benchmark_random_scene(world, renderer, 10000);
            break;
        case 8:
            benchmark_random_scene(world, renderer, 25000);
            break;
        case 9:
            benchmark_random_scene(world, renderer, 50000);
            break;
        case 10:
            benchmark_random_scene(world, renderer, 100000);
            break;
        default:
            throw std::invalid_argument("Exceeded maximum scene_id");
    }

}

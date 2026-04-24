#include "util/raytrace.h"
#include "util/util_timer.h"

#include <SFML/Graphics.hpp>
#include "raytrace/hittables/bvh/bvh_sequential.h"
#include "raytrace/hittables/bvh/bvh_slicing.h"
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "render/renderer.h"
#include "render/scene_loader.h"

// Select a BVH implementation
shared_ptr<Hittable> make_bvh(HittableList world, int impl) {
	UtilTimer timer("BVH Construction Time");
	switch (impl) {
		case 0:
			return make_shared<BvhNodeSequential>(world);
			break;
		case 1:
			return make_shared<BvhNodeSlicing>(world);
			break;
		default:
			throw std::invalid_argument("Invalid BVH impl ID");
			break;
	}
}

int main() {	
	/* Set Renderer properties here ... */

	auto renderer = Renderer();

	// Load the desired scene
	auto scene_loader = SceneLoader();
	HittableList world;
	scene_loader.load(2, world, renderer);

	// Create the sfml window
	uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar
	);

	// Create the sfml graphics repr for each hittable in the world
	auto world_graphics = renderer.world_graphics(world);

	// Before ray rendering, initialize the BVH
	// TODO: probably want a commandline option to select BVH implementations
	world = HittableList(make_bvh(world, 1));
	
	// Create the pixelmap where we render rays
	auto pixels = renderer.pixel_map(world);

	while (window.isOpen())
	{
		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>() )
				window.close();
		}

		window.clear();
		window.draw(*pixels);
		for (const auto& shape : world_graphics) {
			window.draw(*shape);
		}
		window.display();
	}
}

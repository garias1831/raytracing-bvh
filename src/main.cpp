#include "util/raytrace.h"

#include <SFML/Graphics.hpp>
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "render/renderer.h"
#include "render/scene_loader.h"

int main() {	
	/* Set Renderer properties here ... */

	auto renderer = Renderer();

	// Load the desired scene
	auto scene_loader = SceneLoader();
	HittableList world;
	scene_loader.load(1, world, renderer);

	// Create the sfml window
	uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar
	);

	// Create the sfml graphics repr for each hittable in the world
	auto world_graphics = renderer.world_graphics(world);
	
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

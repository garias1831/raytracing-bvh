#include "util/raytrace.h"

#include <SFML/Graphics.hpp>
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "render/renderer.h"

int main() {	
	/* Set Renderer properties here ... */

	auto renderer = Renderer();

	uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	/* Specify custom lightsource location */
	renderer.set_source_loc(Point2(window_width / 2, window_height / 2));

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar
	);

	// Test Circles (Objects)
	HittableList world;
	world.add(make_shared<Circle>(Point2(70, 70), 30.0));
	world.add(make_shared<Circle>(Point2(150, 150), 50.0));
	world.add(make_shared<Circle>(Point2(350, 300), 50.0));
	world.add(make_shared<Circle>(Point2(500, 200), 60));
	world.add(make_shared<Circle>(Point2(550, 100), 10));


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

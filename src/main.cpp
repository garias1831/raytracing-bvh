#include <SFML/Graphics.hpp>
#include "raytrace/vec2.h"
#include "raytrace/ray.h"
#include "raytrace/color.h"
#include <iostream>

Color ray_color(const Ray& r) {
	Vec2 unit_direction = unit_vector(r.direction());
	auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*Color(1.0, 1.0, 1.0) + a*Color(0.5, 0.7, 1.0);
}

int main()
{	
	// Window Dimensions
	const uint window_width = 700;
	const auto aspect_ratio = (16.0 / 9.0);
	uint window_height = window_width / aspect_ratio;
	window_height = (window_height >= 1) ? window_height : 1; // Ensure height at least 1

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar
	);

	// Specify the ray origin
	int srci = window_width / 2;
	int srcj = window_height / 2;
	Point2 source(srci + 0.5, srcj + 0.5); // Pixel center

	// Initialize the pixelmap
	sf::VertexArray pixels(sf::PrimitiveType::Points, window_width * window_height);
	int p;
	for (int j = 0; j < window_height; j++) {
		for (int i = 0; i < window_width; i++) {
			
			auto pixel_center = Point2(i + 0.5, j + 0.5);
			auto ray_direction = pixel_center - source;

			Ray r = Ray(source, ray_direction);
			Color r_color = ray_color(r);

			p = window_width * j + i;
			pixels[p].position = sf::Vector2(float(i), float(j));
			pixels[p].color = sf::Color(r_color.ir(), r_color.ig(), r_color.ib());
		}
	}
	pixels[window_width * srcj + srci].color = sf::Color::White;

	while (window.isOpen())
	{
		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>() )
				window.close();
		}

		window.clear();
		window.draw(pixels);
		window.display();
	}
}

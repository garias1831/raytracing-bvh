#include <SFML/Graphics.hpp>
#include "raytrace/vec2.h"
#include "raytrace/ray.h"
#include "raytrace/color.h"
#include "raytrace/hittables/circle.h"
#include <iostream>

const double infinity = std::numeric_limits<double>::infinity();

const double circle_radius = 90.0;
const double circle_pos = 150.0;

// Assume the pixel grid is inset by 1/2 the pixel to pixel distance (1.0f)
const Point2 pixel00_loc = Point2(0.5, 0.5);  

Color ray_color(const Ray& r) {
	Circle c(pixel00_loc + Point2(circle_pos, circle_pos), circle_radius);
	HitRecord rec;
	if (c.hit(r, 0, infinity, rec)) {
		return Color(1.0, 0, 0);
	}

	Vec2 unit_direction = unit_vector(r.direction());
	auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*Color(1.0, 1.0, 1.0) + a*Color(0.5, 0.7, 1.0);
}

int main() {	
	// Window Dimensions
	const uint window_width = 700;
	const auto aspect_ratio = (16.0 / 9.0);
	uint window_height = window_width / aspect_ratio;
	window_height = (window_height >= 1) ? window_height : 1; // Ensure height at least 1

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar
	);

	// Test Circle
	sf::CircleShape shape(circle_radius);
	shape.setFillColor(sf::Color::Green);
	shape.setOrigin(sf::Vector2f(circle_radius, circle_radius)); // Set origin to center of circle
	shape.setPosition(sf::Vector2f(circle_pos, circle_pos));

	// Specify the ray origin
	int srci = window_width / 2;
	int srcj = window_height / 2;
	Point2 source = pixel00_loc + Point2(srci, srcj); // Pixel center

	// Initialize the pixelmap
	sf::VertexArray pixels(sf::PrimitiveType::Points, window_width * window_height);
	int p;
	for (int j = 0; j < window_height; j++) {
		for (int i = 0; i < window_width; i++) {
			
			auto pixel_center = pixel00_loc + Point2(i, j);
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
		window.draw(shape);
		window.display();
	}
}

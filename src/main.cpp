#include "util/raytrace.h"

#include <SFML/Graphics.hpp>
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"

// Assume the pixel grid is inset by 1/2 the pixel to pixel distance (1.0f)
const Point2 pixel00_loc = Point2(0.5, 0.5);  

Color ray_color(const Ray& r, const HittableList& world) {
	HitRecord rec;
	if (world.hit(r, Interval(0, infinity), rec)) {
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

	// Test Circles (Objects)
	HittableList world;
	world.add(make_shared<Circle>(Point2(70, 70), 30.0));
	world.add(make_shared<Circle>(Point2(150, 150), 50.0));
	world.add(make_shared<Circle>(Point2(350, 300), 50.0));

	// Create the sfml graphics repr for each hittable in the world
	std::vector<std::unique_ptr<sf::Shape>> world_graphics;
	auto drawcolor = Color(0.231, 0.776, 0.859);
	for (const auto& obj : world.get_objects()) {
		world_graphics.push_back(obj->to_sf(drawcolor));
	}

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
			Color r_color = ray_color(r, world);

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
		for (const auto& shape : world_graphics) {
			window.draw(*shape);
		}
		window.display();
	}
}

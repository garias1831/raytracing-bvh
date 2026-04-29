#include "util/raytrace.h"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "raytrace/hittables/bvh/bvh_sequential.h"
#include "raytrace/hittables/bvh/bvh_topdown.h"
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "render/renderer.h"
#include "render/scene_loader.h"

enum class BvhMode {
	Sequential,
	TopDown,
};

const char* bvh_mode_name(BvhMode mode) {
	switch (mode) {
		case BvhMode::Sequential:
			return "Sequential";
		case BvhMode::TopDown:
			return "Topdown";
	}

	return "unknown";
}

// choosing between the two modes
bool parse_bvh_mode(const std::string& value, BvhMode& mode) {
	if (value == "sequential") {
		mode = BvhMode::Sequential;
		return true;
	}

	if (value == "topdown") {
		mode = BvhMode::TopDown;
		return true;
	}

	return false;
}

shared_ptr<Hittable> benchmark_bvh(const HittableList& world, BvhMode mode) {
	const auto start = std::chrono::steady_clock::now();
	shared_ptr<Hittable> bvh;

	switch (mode) {
		case BvhMode::Sequential:
			bvh = make_shared<BvhNodeSequential>(world);
			break;
		case BvhMode::TopDown:
			bvh = make_shared<BvhNodeTopDown>(world);
			break;
	}

	const auto end = std::chrono::steady_clock::now();

	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	std::cout << bvh_mode_name(mode) << " BVH Construction Time: " << elapsed.count() << " ms\n";

	return bvh;
}

// renders the BVH rectangles
std::vector<sf::RectangleShape> make_bvh_visuals(const BvhNodeTopDown& topdown_bvh) {
	std::vector<sf::RectangleShape> visuals;

	for (const auto& node : topdown_bvh.get_bvh()) {
		double width = node.bbox.x.max - node.bbox.x.min;
		double height = node.bbox.y.max - node.bbox.y.min;
		if (width <= 0.0 || height <= 0.0) {
			continue;
		}

		sf::RectangleShape shape{sf::Vector2f(float(width), float(height))};
		shape.setPosition(sf::Vector2f(float(node.bbox.x.min), float(node.bbox.y.min)));
		shape.setFillColor(sf::Color::Transparent);
		shape.setOutlineThickness(1.0f);
		shape.setOutlineColor(node.object_count > 0 ? sf::Color(255, 170, 70, 180) : sf::Color(80, 220, 120, 140));
		visuals.push_back(shape);
	}

	return visuals;
}

int main(int argc, char* argv[]) {	
	/* Set Renderer properties here ... */

	// Setting up the command line parser
	
	// Defaults 
	bool show_bvh = false;
	int scene_id = 2;
	
	BvhMode bvh_mode = BvhMode::TopDown;
	
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--show-bvh") {
			show_bvh = true;
		} 
		else if (arg == "--bvh") {
			if (i + 1 >= argc) {
				std::cerr << "not a correct mode, out of bounds\n";
				return 1;
			}
			if (!parse_bvh_mode(argv[++i], bvh_mode)) {
				std::cerr << "invlaid bvh node\n";
				return 1;
			}
		} 
		else if (arg == "--scene") {
			if (i + 1 >= argc) {
				std::cerr << "missing scene id\n";
				return 1;
			}
			scene_id = std::stoi(argv[++i]);
		} 
		else {
			std::cout << "usage: ./raytrace.exe [--show-bvh] [--scene <id>] [--bvh <sequential/topdown>]\n";
			return 0;
		}
	}

	auto renderer = Renderer();

	// Load the desired scene
	auto scene_loader = SceneLoader();
	HittableList world;
	scene_loader.load(scene_id, world, renderer);

	// Create the sfml window
	uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar | sf::Style::Close
	);

	// Create the sfml graphics repr for each hittable in the world
	auto world_graphics = renderer.world_graphics(world);


	// BVL LOGIC GOES HERE, BENCHMARK THE BUILDING PHASE
	auto bvh = benchmark_bvh(world, bvh_mode);
	std::vector<sf::RectangleShape> bvh_visuals;
	auto topdown_bvh = std::dynamic_pointer_cast<BvhNodeTopDown>(bvh);
	if (topdown_bvh) {
		bvh_visuals = make_bvh_visuals(*topdown_bvh);
	} 
	else if (show_bvh) {
		std::cout << "BVH doesn't visualizer for the serial case, it takes too long to render\n";
	}

	// Before ray rendering, initialize the BVH
	world = HittableList(bvh);
	
	// Create the pixelmap where we render rays without blocking the window thread
	auto pixels_future = std::async(std::launch::async, [&renderer, &world]() {
		return renderer.pixel_map(world);
	});
	std::unique_ptr<sf::VertexArray> pixels;

	while (window.isOpen())
	{
		while (const std::optional event = window.pollEvent())
		{
			if (event->is<sf::Event::Closed>() )
				window.close();
		}

		if (!pixels && pixels_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			pixels = pixels_future.get();
		}

		window.clear();
		if (pixels) {
			window.draw(*pixels);
		}
		for (const auto& shape : world_graphics) {
			window.draw(*shape);
		}
		// drawing bvh
		if (show_bvh) {
			for (const auto& shape : bvh_visuals) {
				window.draw(shape);
			}
		}
		window.display();
	}
}

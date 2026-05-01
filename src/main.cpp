#include "util/raytrace.h"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "raytrace/hittables/bvh/bvh_sequential.h"
#include "raytrace/hittables/bvh/bvh_slicing.h"
#include "raytrace/hittables/bvh/bvh_topdown.h"
#include "raytrace/hittables/circle.h"
#include "raytrace/hittables/hittable.h"
#include "raytrace/hittables/hittable_list.h"
#include "render/renderer.h"
#include "render/scene_loader.h"

enum class BvhMode {
	Sequential,
	Slicing,
	TopDown,
};

bool parse_split_heuristic(const std::string& value, BvhSplitHeuristic& heuristic) {
	if (value == "median") {
		heuristic = BvhSplitHeuristic::Median;
		return true;
	}

	if (value == "sah") {
		heuristic = BvhSplitHeuristic::Sah;
		return true;
	}

	return false;
}

const char* bvh_mode_name(BvhMode mode) {
	switch (mode) {
		case BvhMode::Sequential:
			return "Sequential";
		case BvhMode::Slicing:
			return "Slicing";
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

	if (value == "slicing") {
		mode = BvhMode::Slicing;
		return true;
	}

	if (value == "topdown") {
		mode = BvhMode::TopDown;
		return true;
	}

	return false;
}

struct BenchmarkResult {
	shared_ptr<Hittable> bvh;
	long long build_elapsed_ms;
	long long render_elapsed_ms;
};

BenchmarkResult benchmark_bvh(const HittableList& world, const Renderer& renderer, BvhMode mode, bool print_result = true) {
	const auto start = std::chrono::steady_clock::now();
	shared_ptr<Hittable> bvh;

	switch (mode) {
		case BvhMode::Sequential:
			bvh = make_shared<BvhNodeSequential>(world);
			break;
		case BvhMode::Slicing:
			bvh = make_shared<BvhNodeSlicing>(world);
			break;
		case BvhMode::TopDown:
			bvh = make_shared<BvhNodeTopDown>(world);
			break;
	}

	const auto end = std::chrono::steady_clock::now();
	const auto build_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	HittableList render_world(bvh);
	const auto render_start = std::chrono::steady_clock::now();
	auto pixels = renderer.pixel_map(render_world);
	const auto render_end = std::chrono::steady_clock::now();
	const auto render_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(render_end - render_start);
	(void) pixels;

	if (print_result) {
		std::cout << bvh_mode_name(mode) << " BVH Construction Time: " << build_elapsed.count() << " ms\n";
		std::cout << bvh_mode_name(mode) << " Render Time: " << render_elapsed.count() << " ms\n";
		std::cout << bvh_mode_name(mode) << " Total Time: " << (build_elapsed.count() + render_elapsed.count()) << " ms\n";
	}

	return {bvh, build_elapsed.count(), render_elapsed.count()};
}

struct TopDownMetricsAggregate {
	int runs = 0;
	double frontier_levels = 0.0;
	double total_sort_ms = 0.0;
	double total_copy_sync_ms = 0.0;
	std::vector<double> task_counts_per_level;
	std::vector<double> avg_primitives_per_task_per_level;
	std::vector<double> min_primitives_per_task_per_level;
	std::vector<double> max_primitives_per_task_per_level;
	std::vector<double> level_total_ms;
	std::vector<double> level_sort_ms;
	std::vector<double> level_copy_sync_ms;
};

struct SlicingMetricsAggregate {
	int runs = 0;
	double levels = 0.0;
	double initial_leaf_write_ms = 0.0;
	double total_sort_ms = 0.0;
	double total_merge_ms = 0.0;
	double total_write_ms = 0.0;
	double total_copy_sync_ms = 0.0;
	std::vector<double> boxes_per_level;
	std::vector<double> level_total_ms;
	std::vector<double> level_sort_ms;
	std::vector<double> level_merge_ms;
	std::vector<double> level_write_ms;
	std::vector<double> level_copy_sync_ms;
};

void accumulate_topdown_metrics(TopDownMetricsAggregate& aggregate, const TopDownBvhMetrics& metrics) {
	aggregate.runs++;
	aggregate.frontier_levels += metrics.frontier_levels;
	aggregate.total_sort_ms += metrics.total_sort_ms;
	aggregate.total_copy_sync_ms += metrics.total_copy_sync_ms;

	const int levels = metrics.frontier_levels;
	if (aggregate.task_counts_per_level.size() < size_t(levels)) {
		aggregate.task_counts_per_level.resize(levels, 0.0);
		aggregate.avg_primitives_per_task_per_level.resize(levels, 0.0);
		aggregate.min_primitives_per_task_per_level.resize(levels, 0.0);
		aggregate.max_primitives_per_task_per_level.resize(levels, 0.0);
		aggregate.level_total_ms.resize(levels, 0.0);
		aggregate.level_sort_ms.resize(levels, 0.0);
		aggregate.level_copy_sync_ms.resize(levels, 0.0);
	}

	for (int level = 0; level < levels; ++level) {
		aggregate.task_counts_per_level[level] += metrics.task_counts_per_level[level];
		aggregate.avg_primitives_per_task_per_level[level] += metrics.avg_primitives_per_task_per_level[level];
		aggregate.min_primitives_per_task_per_level[level] += metrics.min_primitives_per_task_per_level[level];
		aggregate.max_primitives_per_task_per_level[level] += metrics.max_primitives_per_task_per_level[level];
		aggregate.level_total_ms[level] += metrics.level_total_ms[level];
		aggregate.level_sort_ms[level] += metrics.level_sort_ms[level];
		aggregate.level_copy_sync_ms[level] += metrics.level_copy_sync_ms[level];
	}
}

void print_topdown_metrics(const TopDownMetricsAggregate& aggregate) {
	if (aggregate.runs == 0) {
		return;
	}

	std::cout << "Topdown Split Heuristic: " << bvh_split_heuristic_name(get_bvh_split_heuristic()) << "\n";
	std::cout << "Topdown Leaf Threshold: " << get_topdown_leaf_threshold() << "\n";
	std::cout << "Topdown Frontier Levels: " << (aggregate.frontier_levels / aggregate.runs) << "\n";
	std::cout << "Topdown Total Sort Time: " << (aggregate.total_sort_ms / aggregate.runs) << " ms\n";
	std::cout << "Topdown Total Copy/Sync Time: " << (aggregate.total_copy_sync_ms / aggregate.runs) << " ms\n";

	for (size_t level = 0; level < aggregate.task_counts_per_level.size(); ++level) {
		std::cout
			<< "  Level " << level
			<< ": tasks=" << (aggregate.task_counts_per_level[level] / aggregate.runs)
			<< ", avg primitives/task=" << (aggregate.avg_primitives_per_task_per_level[level] / aggregate.runs)
			<< ", min primitives/task=" << (aggregate.min_primitives_per_task_per_level[level] / aggregate.runs)
			<< ", max primitives/task=" << (aggregate.max_primitives_per_task_per_level[level] / aggregate.runs)
			<< ", total=" << (aggregate.level_total_ms[level] / aggregate.runs) << " ms"
			<< ", sort=" << (aggregate.level_sort_ms[level] / aggregate.runs) << " ms"
			<< ", copy/sync=" << (aggregate.level_copy_sync_ms[level] / aggregate.runs) << " ms\n";
	}
}

void accumulate_slicing_metrics(SlicingMetricsAggregate& aggregate, const SlicingBvhMetrics& metrics) {
	aggregate.runs++;
	aggregate.levels += metrics.levels;
	aggregate.initial_leaf_write_ms += metrics.initial_leaf_write_ms;
	aggregate.total_sort_ms += metrics.total_sort_ms;
	aggregate.total_merge_ms += metrics.total_merge_ms;
	aggregate.total_write_ms += metrics.total_write_ms;
	aggregate.total_copy_sync_ms += metrics.total_copy_sync_ms;

	const int levels = metrics.levels;
	if (aggregate.boxes_per_level.size() < size_t(levels)) {
		aggregate.boxes_per_level.resize(levels, 0.0);
		aggregate.level_total_ms.resize(levels, 0.0);
		aggregate.level_sort_ms.resize(levels, 0.0);
		aggregate.level_merge_ms.resize(levels, 0.0);
		aggregate.level_write_ms.resize(levels, 0.0);
		aggregate.level_copy_sync_ms.resize(levels, 0.0);
	}

	for (int level = 0; level < levels; ++level) {
		aggregate.boxes_per_level[level] += metrics.boxes_per_level[level];
		aggregate.level_total_ms[level] += metrics.level_total_ms[level];
		aggregate.level_sort_ms[level] += metrics.level_sort_ms[level];
		aggregate.level_merge_ms[level] += metrics.level_merge_ms[level];
		aggregate.level_write_ms[level] += metrics.level_write_ms[level];
		aggregate.level_copy_sync_ms[level] += metrics.level_copy_sync_ms[level];
	}
}

void print_slicing_metrics(const SlicingMetricsAggregate& aggregate) {
	if (aggregate.runs == 0) {
		return;
	}

	std::cout << "Slicing Levels: " << (aggregate.levels / aggregate.runs) << "\n";
	std::cout << "Slicing Initial Leaf Write Time: " << (aggregate.initial_leaf_write_ms / aggregate.runs) << " ms\n";
	std::cout << "Slicing Total Sort Time: " << (aggregate.total_sort_ms / aggregate.runs) << " ms\n";
	std::cout << "Slicing Total Merge Time: " << (aggregate.total_merge_ms / aggregate.runs) << " ms\n";
	std::cout << "Slicing Total Write Time: " << (aggregate.total_write_ms / aggregate.runs) << " ms\n";
	std::cout << "Slicing Total Copy/Sync Time: " << (aggregate.total_copy_sync_ms / aggregate.runs) << " ms\n";

	for (size_t level = 0; level < aggregate.boxes_per_level.size(); ++level) {
		std::cout
			<< "  Level " << level
			<< ": boxes=" << (aggregate.boxes_per_level[level] / aggregate.runs)
			<< ", total=" << (aggregate.level_total_ms[level] / aggregate.runs) << " ms"
			<< ", sort=" << (aggregate.level_sort_ms[level] / aggregate.runs) << " ms"
			<< ", merge=" << (aggregate.level_merge_ms[level] / aggregate.runs) << " ms"
			<< ", write=" << (aggregate.level_write_ms[level] / aggregate.runs) << " ms"
			<< ", copy/sync=" << (aggregate.level_copy_sync_ms[level] / aggregate.runs) << " ms\n";
	}
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

std::vector<sf::RectangleShape> make_bvh_visuals(const BvhNodeSlicing& slicing_bvh) {
	std::vector<sf::RectangleShape> visuals;

	for (const auto& node : slicing_bvh.get_bvh()) {
		double width = node.bbox.x.max - node.bbox.x.min;
		double height = node.bbox.y.max - node.bbox.y.min;
		if (width <= 0.0 || height <= 0.0) {
			continue;
		}

		sf::RectangleShape shape{sf::Vector2f(float(width), float(height))};
		shape.setPosition(sf::Vector2f(float(node.bbox.x.min), float(node.bbox.y.min)));
		shape.setFillColor(sf::Color::Transparent);
		shape.setOutlineThickness(1.0f);
		shape.setOutlineColor(sf::Color(255, 110, 110, 140));
		visuals.push_back(shape);
	}

	return visuals;
}

int main(int argc, char* argv[]) {	
	/* Set Renderer properties here ... */

	// Setting up the command line parser
	
	// Defaults 
	bool show_bvh = false;
	bool benchmark_only = false;
	bool benchmark_speedup = false;
	int benchmark_runs = 1;
	int benchmark_warmup = 0;
	int scene_id = 2;
	int topdown_leaf_threshold = get_topdown_leaf_threshold();
	BvhSplitHeuristic split_heuristic = get_bvh_split_heuristic();
	
	BvhMode bvh_mode = BvhMode::TopDown;
	
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--show-bvh") {
			show_bvh = true;
		}
		else if (arg == "--benchmark-only") {
			benchmark_only = true;
		}
		else if (arg == "--benchmark-speedup") {
			benchmark_speedup = true;
		}
		else if (arg == "--benchmark-runs") {
			if (i + 1 >= argc) {
				std::cerr << "missing benchmark run count\n";
				return 1;
			}
			benchmark_runs = std::stoi(argv[++i]);
			if (benchmark_runs < 1) {
				std::cerr << "benchmark runs must be >= 1\n";
				return 1;
			}
		}
		else if (arg == "--benchmark-warmup") {
			if (i + 1 >= argc) {
				std::cerr << "missing benchmark warmup count\n";
				return 1;
			}
			benchmark_warmup = std::stoi(argv[++i]);
			if (benchmark_warmup < 0) {
				std::cerr << "benchmark warmup must be >= 0\n";
				return 1;
			}
		}
		else if (arg == "--topdown-leaf-size") {
			if (i + 1 >= argc) {
				std::cerr << "missing leaf size\n";
				return 1;
			}
			topdown_leaf_threshold = std::stoi(argv[++i]);
		}
		else if (arg == "--split-heuristic") {
			if (i + 1 >= argc) {
				std::cerr << "missing split heuristic\n";
				return 1;
			}
			if (!parse_split_heuristic(argv[++i], split_heuristic)) {
				std::cerr << "invalid split heuristic\n";
				return 1;
			}
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
			std::cout << "usage: ./raytrace.exe [--show-bvh] [--benchmark-only] [--benchmark-speedup] [--benchmark-runs <n>] [--benchmark-warmup <n>] [--scene <id>] [--bvh <sequential/slicing/topdown>] [--split-heuristic <median/sah>] [--topdown-leaf-size <n>]\n";
			return 0;
		}
	}

	set_topdown_leaf_threshold(topdown_leaf_threshold);
	set_bvh_split_heuristic(split_heuristic);

	auto renderer = Renderer();

	// Load the desired scene
	auto scene_loader = SceneLoader();
	HittableList world;
	scene_loader.load(scene_id, world, renderer);


	// BVL LOGIC GOES HERE, BENCHMARK THE BUILDING PHASE
	BenchmarkResult selected_result;
	TopDownMetricsAggregate topdown_metrics_aggregate;
	SlicingMetricsAggregate slicing_metrics_aggregate;
	if (benchmark_speedup) {
		BvhMode comparison_mode = (bvh_mode == BvhMode::Sequential) ? BvhMode::TopDown : bvh_mode;
		long long sequential_build_total_ms = 0;
		long long comparison_build_total_ms = 0;
		long long sequential_render_total_ms = 0;
		long long comparison_render_total_ms = 0;
		BenchmarkResult sequential_result{};
		BenchmarkResult comparison_result{};

		for (int run = 0; run < benchmark_warmup; ++run) {
			benchmark_bvh(world, renderer, BvhMode::Sequential, false);
			benchmark_bvh(world, renderer, comparison_mode, false);
		}

		for (int run = 0; run < benchmark_runs; ++run) {
			sequential_result = benchmark_bvh(world, renderer, BvhMode::Sequential, false);
			comparison_result = benchmark_bvh(world, renderer, comparison_mode, false);
			sequential_build_total_ms += sequential_result.build_elapsed_ms;
			comparison_build_total_ms += comparison_result.build_elapsed_ms;
			sequential_render_total_ms += sequential_result.render_elapsed_ms;
			comparison_render_total_ms += comparison_result.render_elapsed_ms;
			if (comparison_mode == BvhMode::TopDown) {
				accumulate_topdown_metrics(topdown_metrics_aggregate, get_topdown_bvh_metrics());
			} else if (comparison_mode == BvhMode::Slicing) {
				accumulate_slicing_metrics(slicing_metrics_aggregate, get_slicing_bvh_metrics());
			}
		}

		std::cout << "Sequential Average BVH Construction Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(sequential_build_total_ms) / benchmark_runs) << " ms\n";
		std::cout << "Sequential Average Render Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(sequential_render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << "Sequential Average Total Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(sequential_build_total_ms + sequential_render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << bvh_mode_name(comparison_mode) << " Average BVH Construction Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(comparison_build_total_ms) / benchmark_runs) << " ms\n";
		std::cout << bvh_mode_name(comparison_mode) << " Average Render Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(comparison_render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << bvh_mode_name(comparison_mode) << " Average Total Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(comparison_build_total_ms + comparison_render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << "BVH Split Heuristic: " << bvh_split_heuristic_name(get_bvh_split_heuristic()) << "\n";

		if (comparison_mode == BvhMode::TopDown) {
			print_topdown_metrics(topdown_metrics_aggregate);
		} else if (comparison_mode == BvhMode::Slicing) {
			print_slicing_metrics(slicing_metrics_aggregate);
		}

		if (comparison_build_total_ms > 0) {
			double build_speedup = double(sequential_build_total_ms) / double(comparison_build_total_ms);
			std::cout << bvh_mode_name(comparison_mode) << " Build Speedup vs Sequential: " << build_speedup << "x\n";
		} else {
			std::cout << bvh_mode_name(comparison_mode) << " Build Speedup vs Sequential: <1 ms timing resolution, increase scene size for a stable result>\n";
		}

		if (comparison_render_total_ms > 0) {
			double render_speedup = double(sequential_render_total_ms) / double(comparison_render_total_ms);
			std::cout << bvh_mode_name(comparison_mode) << " Render Speedup vs Sequential: " << render_speedup << "x\n";
		} else {
			std::cout << bvh_mode_name(comparison_mode) << " Render Speedup vs Sequential: <1 ms timing resolution, increase scene size for a stable result>\n";
		}

		if ((comparison_build_total_ms + comparison_render_total_ms) > 0) {
			double total_speedup = double(sequential_build_total_ms + sequential_render_total_ms) / double(comparison_build_total_ms + comparison_render_total_ms);
			std::cout << bvh_mode_name(comparison_mode) << " Total Speedup vs Sequential: " << total_speedup << "x\n";
		} else {
			std::cout << bvh_mode_name(comparison_mode) << " Total Speedup vs Sequential: <1 ms timing resolution, increase scene size for a stable result>\n";
		}

		selected_result = (bvh_mode == BvhMode::Sequential) ? sequential_result : comparison_result;
	} else {
		long long build_total_ms = 0;
		long long render_total_ms = 0;

		for (int run = 0; run < benchmark_warmup; ++run) {
			benchmark_bvh(world, renderer, bvh_mode, false);
		}

		for (int run = 0; run < benchmark_runs; ++run) {
			selected_result = benchmark_bvh(world, renderer, bvh_mode, false);
			build_total_ms += selected_result.build_elapsed_ms;
			render_total_ms += selected_result.render_elapsed_ms;
			if (bvh_mode == BvhMode::TopDown) {
				accumulate_topdown_metrics(topdown_metrics_aggregate, get_topdown_bvh_metrics());
			} else if (bvh_mode == BvhMode::Slicing) {
				accumulate_slicing_metrics(slicing_metrics_aggregate, get_slicing_bvh_metrics());
			}
		}

		std::cout << bvh_mode_name(bvh_mode) << " Average BVH Construction Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(build_total_ms) / benchmark_runs) << " ms\n";
		std::cout << bvh_mode_name(bvh_mode) << " Average Render Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << bvh_mode_name(bvh_mode) << " Average Total Time (" << benchmark_runs << " runs";
		if (benchmark_warmup > 0) {
			std::cout << ", after " << benchmark_warmup << " warmup";
		}
		std::cout << "): " << (double(build_total_ms + render_total_ms) / benchmark_runs) << " ms\n";
		std::cout << "BVH Split Heuristic: " << bvh_split_heuristic_name(get_bvh_split_heuristic()) << "\n";

		if (bvh_mode == BvhMode::TopDown) {
			print_topdown_metrics(topdown_metrics_aggregate);
		} else if (bvh_mode == BvhMode::Slicing) {
			print_slicing_metrics(slicing_metrics_aggregate);
		}
	}

	if (benchmark_only) {
		return 0;
	}

	auto bvh = selected_result.bvh;
	std::vector<sf::RectangleShape> bvh_visuals;
	auto topdown_bvh = std::dynamic_pointer_cast<BvhNodeTopDown>(bvh);
	if (topdown_bvh) {
		bvh_visuals = make_bvh_visuals(*topdown_bvh);
	}
	else if (auto slicing_bvh = std::dynamic_pointer_cast<BvhNodeSlicing>(bvh)) {
		bvh_visuals = make_bvh_visuals(*slicing_bvh);
	}
	else if (show_bvh) {
		std::cout << "BVH doesn't visualizer for the serial case, it takes too long to render\n";
	}

	

	// Create the sfml window
	uint window_width = renderer.get_window_width();
	uint window_height = renderer.get_window_height();

	sf::RenderWindow window(
		sf::VideoMode({ window_width, window_height }), "Raytrace",
		sf::Style::Titlebar | sf::Style::Close
	);

	// Create the sfml graphics repr for each hittable in the world
	auto world_graphics = renderer.world_graphics(world);

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

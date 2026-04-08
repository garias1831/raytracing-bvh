#include "renderer.h"

#include <memory>
#include <vector>
#include "raytrace/hittables/hittable.h"

using std::make_unique;


uint Renderer::get_window_width() const { return window_width; }
uint Renderer::get_window_height() const { return window_height; }

void Renderer::set_source_loc(Point2 source) {
    source_loc = source;
}

std::vector<std::unique_ptr<sf::Shape>> Renderer::world_graphics(const HittableList& world) const {
    std::vector<std::unique_ptr<sf::Shape>> world_graphics;
    auto drawcolor = Color(0.231, 0.776, 0.859);
    for (const auto& obj : world.get_objects()) {
        world_graphics.push_back(obj->to_sf(drawcolor));
    }

    return world_graphics;
}

std::unique_ptr<sf::VertexArray> Renderer::pixel_map(const HittableList& world) const {
    sf::VertexArray pixels(sf::PrimitiveType::Points, window_width * window_height);
    int p;
    for (int j = 0; j < window_height; j++) {
        for (int i = 0; i < window_width; i++) {
            
            auto pixel_center = pixel00_loc + Point2(i, j);
            auto ray_direction = source_loc - pixel_center;

            Ray ray = Ray(pixel_center, ray_direction);
            Color r_color = ray_color(ray, world);

            p = window_width * j + i;
            pixels[p].position = sf::Vector2(float(i), float(j));
            pixels[p].color = sf::Color(r_color.ir(), r_color.ig(), r_color.ib());
        }
    }
    auto source_ij = source_loc - pixel00_loc; 
    int srci = int(source_ij.x());
    int srcj = int(source_ij.y());
    pixels[window_width * srcj + srci].color = sf::Color::White;

    return make_unique<sf::VertexArray>(pixels);
}

void Renderer::initialize() {
    window_height = window_width / aspect_ratio;
    window_height = (window_height >= 1) ? window_height : 1;
    source_loc = pixel00_loc + Point2(window_width, 0);
}

Color Renderer::ray_color(const Ray& ray, const Hittable& world) const {
    HitRecord rec;
    if (world.hit(ray, Interval(0.0, 1.0), rec)) {
        return Color(1.0, 0, 0);
    }

    Vec2 unit_direction = unit_vector(ray.direction());
    auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*Color(1.0, 1.0, 1.0) + a*Color(0.5, 0.7, 1.0);
}
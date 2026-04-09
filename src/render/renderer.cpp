#include "renderer.h"

#include <memory>
#include <vector>
#include "raytrace/hittables/hittable.h"
#include "util/raytrace.h"


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

    return make_unique<sf::VertexArray>(pixels);
}

void Renderer::initialize() {
    window_height = window_width / aspect_ratio;
    window_height = (window_height >= 1) ? window_height : 1;
    source_loc = pixel00_loc + Point2(window_width, 0);
}

double square(double x){
    return x * x;
}

/**
 * Return the attenuation of the light source according to inverse square law.
 * 
 * Credit: https://lisyarus.github.io/blog/posts/point-light-attenuation.html
 * 
 * @return attenuation factor in [0, 1]
 */
double attenuation(double d) {
    double R = 100.0; // R is the distance where the light reaches 1/2 intensity
    return (1 / (1 + square(d / R)));
}

Color Renderer::ray_color(const Ray& ray, const Hittable& world) const {
    HitRecord rec;
    if (world.hit(ray, Interval(0.0, 1.0), rec)) {
        return Color(0.0, 0, 0); // Shadow
    }

    auto light_color = Color(93.0/255.0, 12.0/255.0, 237.0/255.0); // Black light
    double distance = ray.direction().length();

    return attenuation(distance) * light_color;
}
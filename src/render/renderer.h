#ifndef RENDERER_H
#define RENDERER_H

#include <SFML/Graphics.hpp>
#include "util/raytrace.h"
#include "raytrace/hittables/hittable_list.h"

/// @brief Helper to render sf shapes and keep track of scene properties
class Renderer {
    public:
        uint window_width = 700;
        double aspect_ratio = (16.0 / 9.0);

        const Point2 pixel00_loc = Point2(0.5, 0.5);  
        
        
        Renderer() { initialize(); }
        
        uint get_window_width() const; 
        uint get_window_height() const;

        void set_source_loc(Point2 source);

        /** @brief Convert a collection of raytrace objects to sfml-renderable equivalents.*/
        std::vector<std::unique_ptr<sf::Shape>> world_graphics(const HittableList& world) const;

        /** @brief Return an array of pixels colored based on ray intersections */
        std::unique_ptr<sf::VertexArray> pixel_map(const HittableList& world) const;

    private:
        uint window_height;
        Point2 source_loc;

        void initialize();

        Color ray_color(const Ray& ray, const Hittable& world) const; 
};

#endif  
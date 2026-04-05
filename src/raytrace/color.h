#ifndef COLOR_H
#define COLOR_H

/// @brief Represent an rgb color in the range [0, 1]
class Color {
    public:
        Color() : rgb{0, 0, 0} {}

        Color(double r, double g, double b) : rgb{r, g, b} {}

        double r() const {return rgb[0];}
        double g() const {return rgb[1];}
        double b() const {return rgb[2];}

        // Scale to [0, 255]
        int ir() const {return int(255 * rgb[0]);}
        int ig() const {return int(255 * rgb[1]);}
        int ib() const {return int(255 * rgb[2]);}

        double operator[](int i) const {return rgb[i];}
        double& operator[](int i) {return rgb[i];}

        Color& operator*=(double t) {
            rgb[0] *= t;
            rgb[1] *= t;
            rgb[2] *= t;
            return *this;
        }

    private:
        double rgb[3];
};

inline Color operator+(const Color& u, const Color& v) {
    return Color(u[0] + v[0], u[1] + v[1], u[2] + v[2]);
}

inline Color operator-(const Color& u, const Color& v) {
    return Color(u[0] - v[0], u[1] - v[1], u[2] - v[2]);
}

inline Color operator*(const Color& u, const Color& v) {
    return Color(u[0] * v[0], u[1] * v[1], u[2] * v[2]);
}

inline Color operator*(const Color& u, double t) {
    return Color(u[0] * t, u[1] * t, u[2] * t);
}

inline Color operator*(double t, const Color& u) {
    return u * t;
}

#endif
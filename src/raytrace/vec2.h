#ifndef VEC2_H
#define VEC2_H

#include <cmath>

class Vec2 {
    public:
        Vec2() : e{0, 0} {}

        Vec2(double x, double y) : e{x, y} {}

        double x() const {return e[0];}
        double y() const {return e[1];}

        Vec2 operator-() const {return Vec2(-e[0], -e[1]);}
        double operator[](int i) const {return e[i];}
        double& operator[](int i) {return e[i];}

        Vec2& operator+=(const Vec2& vec) {
            e[0] += vec.e[0];
            e[1] += vec.e[1];
            return *this;
        }

        Vec2& operator*=(double t) {
            e[0] *= t;
            e[1] *= t;
            return *this;
        }

        Vec2& operator/=(double t) {
            return *this *= 1/t;
        }

        double length() const {
            return std::sqrt(length_squared());
        }

        double length_squared() const {
            return e[0] * e[0] + e[1] * e[1];
        }
    
    private:
        double e[2];
};

inline Vec2 operator+(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] + v[0], u[1] + v[1]);
}

inline Vec2 operator-(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] - v[0], u[1] - v[1]);
}

inline Vec2 operator*(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] * v[0], u[1] * v[1]);
}

inline Vec2 operator*(const Vec2& u, double t) {
    return Vec2(u[0] * t, u[1] * t);
}

inline Vec2 operator*(double t, const Vec2& u) {
    return u * t;
}

inline Vec2 operator/(const Vec2& v, double t) {
    return v * 1/t;
}

inline double dot(const Vec2& u, const Vec2& v) {
    return u[0] * v[0] + u[1] * v[1]; 
}

inline Vec2 unit_vector(const Vec2& v) {
    return v / v.length();
}

#endif
#ifndef VEC2_H
#define VEC2_H

#include "util/cuda_callable.h"

class Vec2 {
    public:
        CUDA_CALLABLE_MEMBER
        Vec2() : e{0, 0} {}

        CUDA_CALLABLE_MEMBER
        Vec2(double x, double y) : e{x, y} {}

        CUDA_CALLABLE_MEMBER double x() const {return e[0];}
        CUDA_CALLABLE_MEMBER double y() const {return e[1];}

        CUDA_CALLABLE_MEMBER Vec2 operator-() const {return Vec2(-e[0], -e[1]);}
        CUDA_CALLABLE_MEMBER double operator[](int i) const {return e[i];}
        CUDA_CALLABLE_MEMBER double& operator[](int i) {return e[i];}

        CUDA_CALLABLE_MEMBER
        Vec2& operator+=(const Vec2& vec) {
            e[0] += vec.e[0];
            e[1] += vec.e[1];
            return *this;
        }
        
        CUDA_CALLABLE_MEMBER
        Vec2& operator*=(double t) {
            e[0] *= t;
            e[1] *= t;
            return *this;
        }

        CUDA_CALLABLE_MEMBER
        Vec2& operator/=(double t) {
            return *this *= 1/t;
        }

        CUDA_CALLABLE_MEMBER
        double length() const {
            return std::sqrt(length_squared());
        }

        CUDA_CALLABLE_MEMBER
        double length_squared() const {
            return e[0] * e[0] + e[1] * e[1];
        }
    
    private:
        double e[2];
};

using Point2 = Vec2;

CUDA_CALLABLE_MEMBER
inline Vec2 operator+(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] + v[0], u[1] + v[1]);
}

CUDA_CALLABLE_MEMBER
inline Vec2 operator-(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] - v[0], u[1] - v[1]);
}

CUDA_CALLABLE_MEMBER
inline Vec2 operator*(const Vec2& u, const Vec2& v) {
    return Vec2(u[0] * v[0], u[1] * v[1]);
}

CUDA_CALLABLE_MEMBER
inline Vec2 operator*(const Vec2& u, double t) {
    return Vec2(u[0] * t, u[1] * t);
}

CUDA_CALLABLE_MEMBER
inline Vec2 operator*(double t, const Vec2& u) {
    return u * t;
}

CUDA_CALLABLE_MEMBER
inline Vec2 operator/(const Vec2& v, double t) {
    return v * (1/t);
}

CUDA_CALLABLE_MEMBER
inline double dot(const Vec2& u, const Vec2& v) {
    return u[0] * v[0] + u[1] * v[1]; 
}

CUDA_CALLABLE_MEMBER
inline Vec2 unit_vector(const Vec2& v) {
    return v / v.length();
}

inline std::ostream& operator<<(std::ostream& out, const Vec2& v) {
    return out << v[0] << ' ' << v[1];
}

#endif
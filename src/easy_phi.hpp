#ifndef EASY_PHI_HPP
#define EASY_PHI_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <cmath>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <numbers>
#include <random>
#include <filesystem>
#include <ranges>

namespace easy_phi {

using ep_u8 = uint8_t;
using ep_u16 = uint16_t;
using ep_u32 = uint32_t;
using ep_u64 = uint64_t;

using ep_i8 = int8_t;
using ep_i16 = int16_t;
using ep_i32 = int32_t;
using ep_i64 = int64_t;

using ep_f32 = float;
using ep_f64 = double;

using ep_bool = bool;

static ep_u64 globalCounter = 1;

ep_u64 reqGlobalCounter() {
    return globalCounter++;
}

struct Data {
    std::vector<ep_u8> data;

    static Data FromPtr(ep_u8* ptr, ep_u64 size) {
        return Data {
            .data = std::vector<ep_u8>(ptr, ptr + size)
        };
    }

    static ep_bool FromFile(Data* dst, const std::string& fn) {
        std::ifstream file(std::filesystem::path((const char8_t*)fn.c_str()), std::ios::binary | std::ios::ate);
        if (!file) return false;

        ep_u64 size = file.tellg();
        file.seekg(0);
        std::vector<ep_u8> buffer(size);
        if (!file.read((char*)buffer.data(), size)) return false;
        file.close();

        *dst = Data { .data = buffer };
        return true;
    }

    std::string toString() const {
        return std::string((char*)data.data(), data.size());
    }
};

struct Vec2 {
    ep_f64 x, y;

    ep_f64 sum() const {
        return x + y;
    }

    ep_f64 length() const {
        return std::sqrt(x * x + y * y);
    }

    ep_f64 lengthSquared() const {
        return x * x + y * y;
    }

    ep_f64 xyDiff() const {
        return std::abs(x - y);
    }

    Vec2 operator+(const Vec2& v) const {
        return Vec2 { .x = x + v.x, .y = y + v.y };
    }
    
    Vec2 operator-(const Vec2& v) const {
        return Vec2 { .x = x - v.x, .y = y - v.y };
    }

    Vec2 operator*(const Vec2& v) const {
        return Vec2 { .x = x * v.x, .y = y * v.y };
    }

    Vec2 operator/(const Vec2& v) const {
        return Vec2 { .x = x / v.x, .y = y / v.y };
    }

    Vec2 operator+(ep_f64 v) const {
        return Vec2 { .x = x + v, .y = y + v };
    }

    Vec2 operator-(ep_f64 v) const {
        return Vec2 { .x = x - v, .y = y - v };
    }

    Vec2 operator*(ep_f64 v) const {
        return Vec2 { .x = x * v, .y = y * v };
    }

    Vec2 operator/(ep_f64 v) const {
        return Vec2 { .x = x / v, .y = y / v };
    }

    Vec2& operator+=(const Vec2& v) {
        x += v.x; y += v.y;
        return *this;
    }

    Vec2& operator-=(const Vec2& v) {
        x -= v.x; y -= v.y;
        return *this;
    }

    Vec2& operator*=(const Vec2& v) {
        x *= v.x; y *= v.y;
        return *this;
    }

    Vec2& operator/=(const Vec2& v) {
        x /= v.x; y /= v.y;
        return *this;
    }

    Vec2& operator+=(ep_f64 v) {
        x += v; y += v;
        return *this;
    }

    Vec2& operator-=(ep_f64 v) {
        x -= v; y -= v;
        return *this;
    }

    Vec2& operator*=(ep_f64 v) {
        x *= v; y *= v;
        return *this;
    }

    Vec2& operator/=(ep_f64 v) {
        x /= v; y /= v;
        return *this;
    }

    ep_bool operator==(const Vec2& v) const {
        return x == v.x && y == v.y;
    }

    ep_bool operator!=(const Vec2& v) const {
        return x != v.x || y != v.y;
    }

    Vec2 rotate(ep_f64 angle, ep_f64 length) const {
        ep_f64 c = std::cos(angle);
        ep_f64 s = std::sin(angle);
        return Vec2 {
            .x = x + c * length,
            .y = y + s * length
        };
    }

    Vec2 rotateDegress(ep_f64 angle, ep_f64 length) const {
        return rotate(angle / 180.0 * std::numbers::pi, length);
    }

    bool isZeroZone() {
        return x == y;
    }
};

static const ep_f64 INF_TIME = 99999.0;
static const Vec2 INF_TZ = { -INF_TIME, INF_TIME };
static const ep_f64 INF_EV = 1e9;

struct Rect {
    ep_f64 x, y, w, h;

    Vec2 position() const {
        return { x, y };
    }

    Vec2 size() const {
        return { w, h };
    }

    Rect extend(ep_f64 padding) const {
        return Rect {
            .x = x - padding,
            .y = y - padding,
            .w = w + padding * 2,
            .h = h + padding * 2
        };
    }

    Vec2 center() const {
        return { x + w / 2, y + h / 2 };
    }
};

struct HashBucket {
    ep_u64 hash = 0xcbf29ce484222325ULL;
    
    static constexpr ep_u64 FNV_PRIME = 0x100000001b3ULL;
    
    void mix(ep_u64 value) {
        hash ^= value;
        hash *= FNV_PRIME;
    }
    
    template <typename T>
    void submitNumber(T v) {
        static_assert(std::is_arithmetic_v<T>, "T must be numeric");
        
        if constexpr (std::is_floating_point_v<T>) {
            if (v == 0) v = 0;
        }
        
        const ep_u8* bytes = reinterpret_cast<const ep_u8*>(&v);
        for (ep_u64 i = 0; i < sizeof(T); i++) {
            mix(bytes[i]);
        }
    }
    
    void submitBool(bool b) {
        mix(b ? 1 : 0);
    }

    template <typename T>
    void submitOptionalNumber(std::optional<T> v) {
        if (v.has_value()) {
            submitBool(true);
            submitNumber(v.value());
        } else submitBool(false);
    }
    
    ep_u64 getHash() const {
        return hash;
    }
};

struct Color {
    ep_f64 r, g, b, a;

    static Color White() {
        return Color { 1.0, 1.0, 1.0, 1.0 };
    }

    static Color Black() {
        return Color { 0.0, 0.0, 0.0, 1.0 };
    }

    static Color Red() {
        return Color { 1.0, 0.0, 0.0, 1.0 };
    }

    static Color Green() {
        return Color { 0.0, 1.0, 0.0, 1.0 };
    }

    static Color Blue() {
        return Color { 0.0, 0.0, 1.0, 1.0 };
    }

    static Color Transparent() {
        return Color { 0.0, 0.0, 0.0, 0.0 };
    }

    Color applyAlpha(ep_f64 alpha) {
        return Color { r, g, b, a * alpha };
    }

    Color operator*(const Color& c) const {
        return Color { r * c.r, g * c.g, b * c.b, a * c.a };
    }

    Color operator*(ep_f64 v) const {
        return Color { r * v, g * v, b * v, a * v };
    }

    Color operator+(const Color& c) const {
        return Color { r + c.r, g + c.g, b + c.b, a + c.a };
    }

    Color operator+(ep_f64 v) const {
        return Color { r + v, g + v, b + v, a + v };
    }

    Color operator-(const Color& c) const {
        return Color { r - c.r, g - c.g, b - c.b, a - c.a };
    }

    Color operator-(ep_f64 v) const {
        return Color { r - v, g - v, b - v, a - v };
    }

    Color operator/(const Color& c) const {
        return Color { r / c.r, g / c.g, b / c.b, a / c.a };
    }

    Color operator/(ep_f64 v) const {
        return Color { r / v, g / v, b / v, a / v };
    }

    Color& operator*=(const Color& c) {
        r *= c.r; g *= c.g; b *= c.b; a *= c.a;
        return *this;
    }

    Color& operator*=(ep_f64 v) {
        r *= v; g *= v; b *= v; a *= v;
        return *this;
    }

    Color& operator+=(const Color& c) {
        r += c.r; g += c.g; b += c.b; a += c.a;
        return *this;
    }

    Color& operator+=(ep_f64 v) {
        r += v; g += v; b += v; a += v;
        return *this;
    }

    Color& operator-=(const Color& c) {
        r -= c.r; g -= c.g; b -= c.b; a -= c.a;
        return *this;
    }

    Color& operator-=(ep_f64 v) {
        r -= v; g -= v; b -= v; a -= v;
        return *this;
    }

    Color& operator/=(const Color& c) {
        r /= c.r; g /= c.g; b /= c.b; a /= c.a;
        return *this;
    }

    Color& operator/=(ep_f64 v) {
        r /= v; g /= v; b /= v; a /= v;
        return *this;
    }

    bool operator==(const Color& c) const {
        return r == c.r && g == c.g && b == c.b && a == c.a;
    }

    bool operator!=(const Color& c) const {
        return !(*this == c);
    }
};

struct Transform2D {
    ep_f64 matrix[6];

    Transform2D(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) {
        matrix[0] = a; matrix[1] = b;
        matrix[2] = c; matrix[3] = d;
        matrix[4] = e; matrix[5] = f;
    }

    Transform2D() {
        matrix[0] = 1.0; matrix[1] = 0.0;
        matrix[2] = 0.0; matrix[3] = 1.0;
        matrix[4] = 0.0; matrix[5] = 0.0;
    }

    Transform2D& set(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) {
        matrix[0] = a; matrix[1] = b;
        matrix[2] = c; matrix[3] = d;
        matrix[4] = e; matrix[5] = f;
        return *this;
    }

    Transform2D& transform(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) {
        set(
            matrix[0] * a + matrix[2] * b,
            matrix[1] * a + matrix[3] * b,
            matrix[0] * c + matrix[2] * d,
            matrix[1] * c + matrix[3] * d,
            matrix[0] * e + matrix[2] * f + matrix[4],
            matrix[1] * e + matrix[3] * f + matrix[5]
        );
        return *this;
    }

    Transform2D& transform(const Transform2D& o) {
        transform(
            o.matrix[0], o.matrix[1],
            o.matrix[2], o.matrix[3],
            o.matrix[4], o.matrix[5]
        );
        return *this;
    }

    Transform2D& scale(ep_f64 x, ep_f64 y) {
        transform(x, 0.0, 0.0, y, 0.0, 0.0);
        return *this;
    }

    Transform2D& scale(const Vec2& v) {
        scale(v.x, v.y);
        return *this;
    }

    Transform2D& translate(ep_f64 x, ep_f64 y) {
        transform(1.0, 0.0, 0.0, 1.0, x, y);
        return *this;
    }

    Transform2D& translate(const Vec2& v) {
        translate(v.x, v.y);
        return *this;
    }

    Transform2D& rotate(ep_f64 angle) {
        ep_f64 c = std::cos(angle);
        ep_f64 s = std::sin(angle);
        transform(c, s, -s, c, 0.0, 0.0);
        return *this;
    }

    Transform2D& rotateDegress(ep_f64 angle) {
        rotate(angle / 180.0 * std::numbers::pi);
        return *this;
    }

    Vec2 transformPoint(ep_f64 x, ep_f64 y) const {
        return Vec2 {
            .x = matrix[0] * x + matrix[2] * y + matrix[4],
            .y = matrix[1] * x + matrix[3] * y + matrix[5]
        };
    }

    Vec2 transformPoint(const Vec2& v) const {
        return transformPoint(v.x, v.y);
    }

    Transform2D getInverse() const {
        ep_f64 det = matrix[0] * matrix[3] - matrix[1] * matrix[2];
        ep_f64 invDet = det != 0 ? 1.0 / det : 1e9;
        return Transform2D(
            matrix[3] * invDet, -matrix[1] * invDet,
            -matrix[2] * invDet, matrix[0] * invDet,
            (matrix[2] * matrix[5] - matrix[3] * matrix[4]) * invDet,
            (matrix[1] * matrix[4] - matrix[0] * matrix[5]) * invDet
        );
    }
};

bool pointStrictlyInConvexQuad(const Vec2& p, const Vec2 quad[4]) {
    auto cross = [](ep_f64 ax, ep_f64 ay, ep_f64 bx, ep_f64 by) {
        return ax * by - ay * bx;
    };

    auto x = p.x, y = p.y;
    auto cross0 = cross(quad[1].x - quad[0].x, quad[1].y - quad[0].y, x - quad[0].x, y - quad[0].y);
    auto cross1 = cross(quad[2].x - quad[1].x, quad[2].y - quad[1].y, x - quad[1].x, y - quad[1].y);
    auto cross2 = cross(quad[3].x - quad[2].x, quad[3].y - quad[2].y, x - quad[2].x, y - quad[2].y);
    auto cross3 = cross(quad[0].x - quad[3].x, quad[0].y - quad[3].y, x - quad[3].x, y - quad[3].y);
    
    if (cross0 < 0 && cross1 < 0 && cross2 < 0 && cross3 < 0) return true;
    if (cross0 > 0 && cross1 > 0 && cross2 > 0 && cross3 > 0) return true;
    
    return false;
}

bool pointStrictlyInRect(const Vec2& p, const Rect& r) {
    return r.x < p.x && p.x < r.x + r.w &&
           r.y < p.y && p.y < r.y + r.h;
}

bool quadStrictlyIntersectRect(const Vec2 quad[4], const Rect& r) {
    return pointStrictlyInRect(quad[0], r) ||
           pointStrictlyInRect(quad[1], r) ||
           pointStrictlyInRect(quad[2], r) ||
           pointStrictlyInRect(quad[3], r) ||
           pointStrictlyInConvexQuad(Vec2 {r.x, r.y}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x + r.w, r.y}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x + r.w, r.y + r.h}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x, r.y + r.h}, quad);
}

bool lineIsIntersectLineSeg(const Vec2& linePoint, ep_f64 lineDeg, const Vec2 seg[2]) {
    ep_f64 angle = lineDeg / 180.0 * std::numbers::pi;
    Vec2 dir = { std::cos(angle), std::sin(angle) };
    
    Vec2 s = seg[1] - seg[0];
    Vec2 q = seg[0] - linePoint;
    
    ep_f64 rxs = dir.x * s.y - dir.y * s.x;
    ep_f64 qxs = q.x * s.y - q.y * s.x;
    
    constexpr ep_f64 eps = 1e-9;
    
    if (std::abs(rxs) < eps) {
        if (std::abs(qxs) >= eps) return false;
        return true;
    }
    
    ep_f64 u = (q.x * dir.y - q.y * dir.x) / rxs;
    return u >= -eps && u <= 1.0 + eps;
}

bool lineIsIntersectRect(const Vec2& linePoint, ep_f64 lineDeg, const Rect& r) {
    return lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x, r.y }, Vec2 { r.x + r.w, r.y } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x + r.w, r.y }, Vec2 { r.x + r.w, r.y + r.h } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x + r.w, r.y + r.h }, Vec2 { r.x, r.y + r.h } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x, r.y + r.h }, Vec2 { r.x, r.y } });
}

bool pointIsLeavingPoint(const Vec2& point, ep_f64 deg, const Vec2& targetPoint) {
    ep_f64 eps = 1.0;
    return (
        (point.rotateDegress(deg + 90, -eps) - targetPoint).lengthSquared() -
        (point - targetPoint).lengthSquared()
    ) > 0;
}

bool lineIsLeavingScreen(const Vec2& linePoint, ep_f64 lineDeg, const Rect& screenArea) {
    return !lineIsIntersectRect(linePoint, lineDeg, screenArea) && pointIsLeavingPoint(linePoint, lineDeg, screenArea.center());
}

template <typename T1, typename T2>
struct SKVCache {
    T1 key;
    T2 value;

    template <typename F>
    [[gnu::always_inline, gnu::hot]]
    const T2& get(const T1& k, F&& reseter) {
        if (__builtin_expect(key != k, 0)) {
            key = k;
            value = reseter();
        }

        return value;
    }
};

struct EaseSet {
    struct Milthm {
        static ep_f64 easing_in(ep_u64 press, ep_f64 p) {
            switch (press) {
                case 0: return p;
                case 1: return (1.0 - cos(((p * std::numbers::pi) / 2.0)));
                case 2: return pow(p, 2.0);
                case 3: return pow(p, 3.0);
                case 4: return pow(p, 4.0);
                case 5: return pow(p, 5.0);
                case 6: return ((p == 0.0) ? 0.0 : pow(2.0, ((10.0 * p) - 10.0)));
                case 7: return (1.0 - pow((1.0 - pow(p, 2.0)), 0.5));
                case 8: return ((2.70158 * pow(p, 3.0)) - (1.70158 * pow(p, 2.0)));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((-pow(2.0, ((10.0 * p) - 10.0))) * sin((((p * 10.0) - 10.75) * ((2.0 * std::numbers::pi) / 3.0))))));
                case 10: return (1.0 - (((1.0 - p) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - p), 2.0)) : (((1.0 - p) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - p) - (1.5 / 2.75))) * ((1.0 - p) - (1.5 / 2.75))) + 0.75) : (((1.0 - p) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - p) - (2.25 / 2.75))) * ((1.0 - p) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - p) - (2.625 / 2.75))) * ((1.0 - p) - (2.625 / 2.75))) + 0.984375)))));
                default: return p;
            }
        }

        static ep_f64 easing_out(ep_u64 press, ep_f64 p) {
            switch (press) {
                case 0: return p;
                case 1: return sin(((p * std::numbers::pi) / 2.0));
                case 2: return (1.0 - ((1.0 - p) * (1.0 - p)));
                case 3: return (1.0 - pow((1.0 - p), 3.0));
                case 4: return (1.0 - pow((1.0 - p), 4.0));
                case 5: return (1.0 - pow((1.0 - p), 5.0));
                case 6: return ((p == 1.0) ? 1.0 : (1.0 - pow(2.0, ((-10.0) * p))));
                case 7: return pow((1.0 - pow((p - 1.0), 2.0)), 0.5);
                case 8: return ((1.0 + (2.70158 * pow((p - 1.0), 3.0))) + (1.70158 * pow((p - 1.0), 2.0)));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((pow(2.0, ((-10.0) * p)) * sin((((p * 10.0) - 0.75) * ((2.0 * std::numbers::pi) / 3.0)))) + 1.0)));
                case 10: return ((p < (1.0 / 2.75)) ? (7.5625 * pow(p, 2.0)) : ((p < (2.0 / 2.75)) ? (((7.5625 * (p - (1.5 / 2.75))) * (p - (1.5 / 2.75))) + 0.75) : ((p < (2.5 / 2.75)) ? (((7.5625 * (p - (2.25 / 2.75))) * (p - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (p - (2.625 / 2.75))) * (p - (2.625 / 2.75))) + 0.984375))));
                default: return p;
            }
        }

        static ep_f64 easing_in_out(ep_u64 press, ep_f64 p) {
            switch (press) {
                case 0: return p;
                case 1: return ((-(cos((std::numbers::pi * p)) - 1.0)) / 2.0);
                case 2: return ((p < 0.5) ? (2.0 * pow(p, 2.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 2.0) / 2.0)));
                case 3: return ((p < 0.5) ? (4.0 * pow(p, 3.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 3.0) / 2.0)));
                case 4: return ((p < 0.5) ? (8.0 * pow(p, 4.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 4.0) / 2.0)));
                case 5: return ((p < 0.5) ? (16.0 * pow(p, 5.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 5.0) / 2.0)));
                case 6: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : (((p < 0.5) ? pow(2.0, ((20.0 * p) - 10.0)) : (2.0 - pow(2.0, (((-20.0) * p) + 10.0)))) / 2.0)));
                case 7: return ((p < 0.5) ? ((1.0 - pow((1.0 - pow((2.0 * p), 2.0)), 0.5)) / 2.0) : ((pow((1.0 - pow((((-2.0) * p) + 2.0), 2.0)), 0.5) + 1.0) / 2.0));
                case 8: return ((p < 0.5) ? ((pow((2.0 * p), 2.0) * ((((2.5949095 + 1.0) * 2.0) * p) - 2.5949095)) / 2.0) : (((pow(((2.0 * p) - 2.0), 2.0) * (((2.5949095 + 1.0) * ((p * 2.0) - 2.0)) + 2.5949095)) + 2.0) / 2.0));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 0.0) ? 1.0 : ((p < 0.5) ? (((-pow(2.0, ((20.0 * p) - 10.0))) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) : (((pow(2.0, (((-20.0) * p) + 10.0)) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) + 1.0))));
                case 10: return ((p < 0.5) ? ((1.0 - (((1.0 - (2.0 * p)) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - (2.0 * p)), 2.0)) : (((1.0 - (2.0 * p)) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) + 0.75) : (((1.0 - (2.0 * p)) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) + 0.984375))))) / 2.0) : ((1.0 + ((((2.0 * p) - 1.0) < (1.0 / 2.75)) ? (7.5625 * pow(((2.0 * p) - 1.0), 2.0)) : ((((2.0 * p) - 1.0) < (2.0 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (1.5 / 2.75))) * (((2.0 * p) - 1.0) - (1.5 / 2.75))) + 0.75) : ((((2.0 * p) - 1.0) < (2.5 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (2.25 / 2.75))) * (((2.0 * p) - 1.0) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (((2.0 * p) - 1.0) - (2.625 / 2.75))) * (((2.0 * p) - 1.0) - (2.625 / 2.75))) + 0.984375))))) / 2.0));
                default: return p;
            }
        }

        static ep_f64 easing(ep_u64 ease, ep_u64 press, ep_f64 p) {
            switch (ease) {
                case 0: return easing_in(press, p);
                case 1: return easing_out(press, p);
                case 2: return easing_in_out(press, p);
                default: return p;
            }
        }
    };

    struct Phigros {
        struct Official {
            static ep_f64 easing(ep_u64 ease, ep_f64 p) {
                switch (ease) {
                    case 0: return p;
                    case 1: return 1.0 - cos(p * std::numbers::pi / 2.0);
                    case 2: return sin(p * std::numbers::pi / 2.0);
                    case 3: return (1.0 - cos(p * std::numbers::pi)) / 2.0;
                    case 4: return pow(p, 2.0);
                    case 5: return 1.0 - pow(p - 1.0, 2.0);
                    case 6: return ((p *= 2.0) < 1.0 ? pow(p, 2.0) : (-(pow(p - 2.0, 2.0) - 2.0))) / 2.0;
                    case 7: return pow(p, 3.0);
                    case 8: return 1.0 + pow(p - 1.0, 3.0);
                    case 9: return ((p *= 2.0) < 1.0 ? pow(p, 3.0) : (2.0 * pow(p - 2.0, 3.0) + 2.0)) / 2.0;
                    case 10: return pow(p, 4.0);
                    case 11: return 1.0 - pow(p - 1.0, 4.0);
                    case 12: return ((p *= 2.0) < 1.0 ? pow(p, 4.0) : (-(pow(p - 2.0, 4.0) - 2.0))) / 2.0;
                    case 13: return 0.0;
                    case 14: return 1.0;
                    default: return p;
                }
            }
        };

        struct RePhiEdit {
            static ep_f64 easing(ep_u64 ease, ep_f64 p) {
                switch (ease) {
                    case 1: return p;
                    case 2: return sin(((p * std::numbers::pi) / 2.0));
                    case 3: return (1.0 - cos(((p * std::numbers::pi) / 2.0)));
                    case 4: return (1.0 - ((1.0 - p) * (1.0 - p)));
                    case 5: return pow(p, 2.0);
                    case 6: return ((-(cos((std::numbers::pi * p)) - 1.0)) / 2.0);
                    case 7: return ((p < 0.5) ? (2.0 * pow(p, 2.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 2.0) / 2.0)));
                    case 8: return (1.0 - pow((1.0 - p), 3.0));
                    case 9: return pow(p, 3.0);
                    case 10: return (1.0 - pow((1.0 - p), 4.0));
                    case 11: return pow(p, 4.0);
                    case 12: return ((p < 0.5) ? (4.0 * pow(p, 3.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 3.0) / 2.0)));
                    case 13: return ((p < 0.5) ? (8.0 * pow(p, 4.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 4.0) / 2.0)));
                    case 14: return (1.0 - pow((1.0 - p), 5.0));
                    case 15: return pow(p, 5.0);
                    case 16: return ((p == 1.0) ? 1.0 : (1.0 - pow(2.0, ((-10.0) * p))));
                    case 17: return ((p == 0.0) ? 0.0 : pow(2.0, ((10.0 * p) - 10.0)));
                    case 18: return pow((1.0 - pow((p - 1.0), 2.0)), 0.5);
                    case 19: return (1.0 - pow((1.0 - pow(p, 2.0)), 0.5));
                    case 20: return ((1.0 + (2.70158 * pow((p - 1.0), 3.0))) + (1.70158 * pow((p - 1.0), 2.0)));
                    case 21: return ((2.70158 * pow(p, 3.0)) - (1.70158 * pow(p, 2.0)));
                    case 22: return ((p < 0.5) ? ((1.0 - pow((1.0 - pow((2.0 * p), 2.0)), 0.5)) / 2.0) : ((pow((1.0 - pow((((-2.0) * p) + 2.0), 2.0)), 0.5) + 1.0) / 2.0));
                    case 23: return ((p < 0.5) ? ((pow((2.0 * p), 2.0) * ((((2.5949095 + 1.0) * 2.0) * p) - 2.5949095)) / 2.0) : (((pow(((2.0 * p) - 2.0), 2.0) * (((2.5949095 + 1.0) * ((p * 2.0) - 2.0)) + 2.5949095)) + 2.0) / 2.0));
                    case 24: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((pow(2.0, ((-10.0) * p)) * sin((((p * 10.0) - 0.75) * ((2.0 * std::numbers::pi) / 3.0)))) + 1.0)));
                    case 25: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((-pow(2.0, ((10.0 * p) - 10.0))) * sin((((p * 10.0) - 10.75) * ((2.0 * std::numbers::pi) / 3.0))))));
                    case 26: return ((p < (1.0 / 2.75)) ? (7.5625 * pow(p, 2.0)) : ((p < (2.0 / 2.75)) ? (((7.5625 * (p - (1.5 / 2.75))) * (p - (1.5 / 2.75))) + 0.75) : ((p < (2.5 / 2.75)) ? (((7.5625 * (p - (2.25 / 2.75))) * (p - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (p - (2.625 / 2.75))) * (p - (2.625 / 2.75))) + 0.984375))));
                    case 27: return (1.0 - (((1.0 - p) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - p), 2.0)) : (((1.0 - p) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - p) - (1.5 / 2.75))) * ((1.0 - p) - (1.5 / 2.75))) + 0.75) : (((1.0 - p) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - p) - (2.25 / 2.75))) * ((1.0 - p) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - p) - (2.625 / 2.75))) * ((1.0 - p) - (2.625 / 2.75))) + 0.984375)))));
                    case 28: return ((p < 0.5) ? ((1.0 - (((1.0 - (2.0 * p)) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - (2.0 * p)), 2.0)) : (((1.0 - (2.0 * p)) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) + 0.75) : (((1.0 - (2.0 * p)) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) + 0.984375))))) / 2.0) : ((1.0 + ((((2.0 * p) - 1.0) < (1.0 / 2.75)) ? (7.5625 * pow(((2.0 * p) - 1.0), 2.0)) : ((((2.0 * p) - 1.0) < (2.0 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (1.5 / 2.75))) * (((2.0 * p) - 1.0) - (1.5 / 2.75))) + 0.75) : ((((2.0 * p) - 1.0) < (2.5 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (2.25 / 2.75))) * (((2.0 * p) - 1.0) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (((2.0 * p) - 1.0) - (2.625 / 2.75))) * (((2.0 * p) - 1.0) - (2.625 / 2.75))) + 0.984375))))) / 2.0));
                    case 29: return ((p == 0.0) ? 0.0 : ((p == 0.0) ? 1.0 : ((p < 0.5) ? (((-pow(2.0, ((20.0 * p) - 10.0))) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) : (((pow(2.0, (((-20.0) * p) + 10.0)) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) + 1.0))));
                    default: return p;
                }
            }
        };
    };

    struct Rizline {
        using Official = Phigros::Official;
    };
};

enum class EnumPhiEventType : ep_u64 {
    PositionX, PositionY,
    SelfRotation, AxisRotation,
    MultiplyAlpha, AdditiveAlpha,
    Color,
    ScaleX, ScaleY,
    Speed, SpeedCoefficient,
    Text,
    MAX = Text + 1
};

enum class EnumPhiNoteType {
    Tap, Drag, Flick, Hold
};

enum class EnumTextAlign {
    Left, Center, Right
};

enum class EnumTextBaseline {
    Top, Middle, Bottom
};

enum class EnumPhiLineAttachUI {
    Pause, Bar,
    ComboNumber, Combo, Score,
    Name, Level,
    None
};

struct PhiNoteTypeHelper {
    static EnumPhiNoteType FromOfficial(ep_u64 n) {
        if (n == 1) return EnumPhiNoteType::Tap;
        if (n == 2) return EnumPhiNoteType::Drag;
        if (n == 3) return EnumPhiNoteType::Hold;
        if (n == 4) return EnumPhiNoteType::Flick;
        return EnumPhiNoteType::Tap;
    }

    static EnumPhiNoteType FromRPE(ep_u64 n) {
        if (n == 1) return EnumPhiNoteType::Tap;
        if (n == 2) return EnumPhiNoteType::Hold;
        if (n == 3) return EnumPhiNoteType::Flick;
        if (n == 4) return EnumPhiNoteType::Drag;
        return EnumPhiNoteType::Tap;
    }

    static EnumPhiNoteType FromPEC(const std::string& s) {
        if (s == "n1") return EnumPhiNoteType::Tap;
        if (s == "n2") return EnumPhiNoteType::Hold;
        if (s == "n3") return EnumPhiNoteType::Flick;
        if (s == "n4") return EnumPhiNoteType::Drag;
        return EnumPhiNoteType::Tap;
    } 
};

struct PhiLineAttachUIHelper {
    static EnumPhiLineAttachUI FromString(const std::string& s) {
        if (s == "pause") return EnumPhiLineAttachUI::Pause;
        if (s == "bar") return EnumPhiLineAttachUI::Bar;
        if (s == "combo") return EnumPhiLineAttachUI::Combo;
        if (s == "combonumber") return EnumPhiLineAttachUI::ComboNumber;
        if (s == "score") return EnumPhiLineAttachUI::Score;
        if (s == "name") return EnumPhiLineAttachUI::Name;
        if (s == "level") return EnumPhiLineAttachUI::Level;
        return EnumPhiLineAttachUI::None;
    }
};

bool isMultiplyEventType(EnumPhiEventType type) {
    return (
        type == EnumPhiEventType::MultiplyAlpha ||
        type == EnumPhiEventType::ScaleX ||
        type == EnumPhiEventType::ScaleY ||
        type == EnumPhiEventType::SpeedCoefficient
    );
}

struct PhiMeta {
    ep_f64 offset;
    std::string title;
    std::string composer;
    std::string artist;
    std::string charter;
    std::string difficulty;

    ep_u64 rpeVersion = 0;

    ep_bool isHoldCoverAtHead;
    ep_bool isZeroLengthHoldHidden;
    ep_bool isHighNoteHidden;
    ep_bool isRegLineAlphaNoteHidden;
    Vec2 lineWidthUnit, lineHeightUnit;

    ep_f64 coverEllipsis = 1e-5;

    ep_f64 maxViewRatio = (ep_f64)16 / 9;
    Vec2 worldOrigin;
    Vec2 worldViewport;
};

struct PhiBPMEvent {
    ep_f64 time; // beats
    ep_f64 bpm;

    static void SortBpmEvents(std::vector<PhiBPMEvent>& events) {
        std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }
};

struct PhiEventLayerIndexs {
    static constexpr ep_u64 RPE_MAX = 5;
    static constexpr ep_u64 UNIT = 1000000;

    static constexpr ep_u64 NOTE_ATTRS = UNIT * 1;
    static constexpr ep_u64 NOTE_ATTRS_2 = UNIT * 2;
    static constexpr ep_u64 LINE_DEFAULT = UNIT * 3;
};

struct PhiEvent {
    Vec2 timeZone, valueZone;
    EnumPhiEventType type;

    ep_f64 (* easingFunc)(void*, ep_f64);
    void* easingFuncContext;
    Vec2 easingZone = { 0.0, 1.0 };

    ep_u64 layerIndex;

    ep_f64 cumulativeValueAtStart;

    ep_f64 valueAtTime(ep_f64 t) {
        // if (t < timeZone.x) return 0.0;
        ep_f64 p = std::clamp((t - timeZone.x) / (timeZone.y - timeZone.x), 0.0, 1.0);

        if (easingFunc) {
            if (easingZone == Vec2 { 0.0, 1.0 }) {
                p = easingFunc(easingFuncContext, p);
            } else if (easingZone.x < easingZone.y) {
                ep_f64 s = easingFunc(easingFuncContext, easingZone.x);
                ep_f64 e = easingFunc(easingFuncContext, easingZone.y);

                if (s != e) {
                    p = (easingFunc(easingFuncContext, p * (easingZone.y - easingZone.x) + easingZone.x) - s) / (e - s);
                }
            }
        }

        if (type == EnumPhiEventType::Color || type == EnumPhiEventType::Text) {
            p = std::clamp(p, 0.0, 1.0);
        }

        return valueZone.x + p * (valueZone.y - valueZone.x);
    }

    static ep_f64 GetDefaultValue(EnumPhiEventType type) {
        return isMultiplyEventType(type) ? 1.0 : 0.0;
    }
};

struct PhiAnimLayer {
    std::vector<PhiEvent> events[(ep_u64)EnumPhiEventType::MAX];

    void addEvent(const PhiEvent& e) {
        events[(ep_u64)e.type].push_back(e);
    }

    std::vector<PhiEvent>& getEvents(EnumPhiEventType type) {
        return events[(ep_u64)type];
    }

    void init() {
        for (auto& typed_events : events) {
            std::sort(typed_events.begin(), typed_events.end(), [](const auto& a, const auto& b) {
                return a.timeZone.x < b.timeZone.x;
            });
        }

        initSpeedCumul();
    }

    void updateType(ep_u64 type, ep_f64 t) {
        auto& typed_events = getEvents((EnumPhiEventType)type);
        if (typed_events.empty()) return;

        if (lastUpdatedTimes[type] == t) return;
        if (lastUpdatedTimes[type] > t) currentIndexs[type] = 0;

        while (
            currentIndexs[type] < typed_events.size() - 1
            && typed_events[currentIndexs[type] + 1].timeZone.x <= t
        ) currentIndexs[type]++;

        auto& e = typed_events[currentIndexs[type]];
        currentValues[type] = e.valueAtTime(t);

        if (type == (ep_u64)EnumPhiEventType::Speed) {
            currentValues[type] = e.cumulativeValueAtStart + (
                (e.valueZone.x + currentValues[type])
                * (std::min(t, e.timeZone.y) - e.timeZone.x) / 2
                + std::max(t - e.timeZone.y, 0.0) * e.valueZone.y
            );
        }

        lastUpdatedTimes[type] = t;
    }

    void updateType(EnumPhiEventType type, ep_f64 t) {
        updateType((ep_u64)type, t);
    }

    void update(ep_f64 t) {
        for (ep_u64 type = 0; type < (ep_u64)EnumPhiEventType::MAX; type++) {
            updateType(type, t);
        }
    }

    ep_f64 get(EnumPhiEventType type) {
        if (events[(ep_u64)type].empty()) return PhiEvent::GetDefaultValue(type);
        return currentValues[(ep_u64)type];
    }

    std::optional<ep_f64> getAlwaysValue(EnumPhiEventType type) {
        auto& typedEvents = getEvents(type);
        if (typedEvents.empty()) return PhiEvent::GetDefaultValue(type);

        if (type == EnumPhiEventType::Speed) {
            if (typedEvents.size() == 1 && typedEvents[0].valueZone.isZeroZone() && typedEvents[0].timeZone.x <= -INF_TIME / 2) {
                return typedEvents[0].valueZone.x;
            }

            return std::nullopt;
        }

        ep_f64 fixedValue = typedEvents[0].valueZone.x;
        for (auto& e : typedEvents) {
            if (!e.valueZone.isZeroZone() || fixedValue != e.valueZone.x) {
                return std::nullopt;
            }
        }

        return fixedValue;
    }

    private:
    ep_f64 lastUpdatedTimes[(ep_u64)EnumPhiEventType::MAX];
    ep_u64 currentIndexs[(ep_u64)EnumPhiEventType::MAX];
    ep_f64 currentValues[(ep_u64)EnumPhiEventType::MAX];

    void initSpeedCumul() {
        auto& speedEvents = getEvents(EnumPhiEventType::Speed);
        ep_f64 cumulativeValue = 0.0;

        for (ep_u64 i = 0; i < speedEvents.size(); i++) {
            auto& e = speedEvents[i];
            e.cumulativeValueAtStart = cumulativeValue;
            cumulativeValue += e.valueZone.sum() * (e.timeZone.y - e.timeZone.x) / 2;

            if (i < speedEvents.size() - 1) {
                cumulativeValue += e.valueZone.y * (speedEvents[i + 1].timeZone.x - e.timeZone.y);
            }
        }
    }
};

struct PhiAnimGroup {
    std::unordered_map<ep_u64, ep_u64> layerIndexMap;
    std::vector<PhiAnimLayer> layers;

    void addEvent(const PhiEvent& e) {
        if (!layerIndexMap.contains(e.layerIndex)) {
            layerIndexMap[e.layerIndex] = layers.size();
            layers.push_back({});
        }

        layers[layerIndexMap[e.layerIndex]].addEvent(e);
    }

    void init() {
        for (auto& layer : layers) {
            layer.init();
        }
    }

    void updateType(EnumPhiEventType type, ep_f64 t) {
        for (auto& layer : layers) {
            layer.updateType(type, t);
        }
    }

    void update(ep_f64 t) {
        for (auto& layer : layers) {
            layer.update(t);
        }
    }

    ep_f64 get_based(EnumPhiEventType type, ep_f64 baseValue) {
        ep_f64 value = baseValue;

        for (auto& layer : layers) {
            if (isMultiplyEventType(type)) value *= layer.get(type);
            else value += layer.get(type);
        }

        return value;
    }

    std::optional<ep_f64> getAlwaysHashValue(EnumPhiEventType type) {
        ep_f64 result = PhiEvent::GetDefaultValue(type);

        for (auto& layer : layers) {
            auto v = layer.getAlwaysValue(type);
            if (!v.has_value()) return std::nullopt;
            if (isMultiplyEventType(type)) result *= v.value();
            else result += v.value();
        }

        return result;
    }
};

struct PhiAnimator {
    std::unordered_map<ep_u64, PhiAnimGroup> groups;

    PhiAnimGroup& requestGroup(ep_u64 index) {
        return groups.try_emplace(index, PhiAnimGroup {}).first->second;
    }

    template <typename T>
    PhiAnimGroup& requestGroup(T& obj) {
        return requestGroup(obj.indexer.get());
    }

    template <typename T>
    void addEvent(T& obj, const PhiEvent& e) {
        requestGroup(obj).addEvent(e);
    }

    void init() {
        for (auto& [_, group] : groups) {
            group.init();
        }
    }

    ep_f64 get_based(ep_u64 index, ep_f64 t, EnumPhiEventType type, ep_f64 baseValue) {
        auto group_it = groups.find(index);
        if (group_it == groups.end()) return baseValue;

        auto& group = group_it->second;
        group.updateType(type, t);
        return group.get_based(type, baseValue);
    }

    template <typename T>
    ep_f64 get_based(T& obj, ep_f64 t, EnumPhiEventType type, ep_f64 baseValue) {
        return get_based(obj.indexer.get(), t, type, baseValue);
    }

    ep_f64 get(ep_u64 index, ep_f64 t, EnumPhiEventType type) {
        return get_based(index, t, type, PhiEvent::GetDefaultValue(type));
    }

    template <typename T>
    ep_f64 get(T& obj, ep_f64 t, EnumPhiEventType type) {
        return get(obj.indexer.get(), t, type);
    }

    // std::nullopt means it is unpredictable
    template <typename T>
    std::optional<ep_u64> get_note_group_hash(T& note) {
        HashBucket hash;

        auto group_it = groups.find(note.indexer.get());
        if (group_it == groups.end()) {
            hash.submitBool(true);
            return hash.getHash();
        }
        hash.submitBool(false);

        auto& group = group_it->second;

        for (const auto type : {
            EnumPhiEventType::PositionY,
            EnumPhiEventType::SelfRotation,
            EnumPhiEventType::AxisRotation,
            EnumPhiEventType::ScaleY,
            EnumPhiEventType::Speed,
            EnumPhiEventType::SpeedCoefficient
        }) {
            auto v = group.getAlwaysHashValue(type);
            if (!v.has_value()) return std::nullopt;
            hash.submitNumber(v.value());
        }

        return hash.getHash();
    }

    ep_f64 get_alpha(ep_u64 index, ep_f64 t, ep_f64 additionalDefault) {
        return get(index, t, EnumPhiEventType::MultiplyAlpha) * get_based(index, t, EnumPhiEventType::AdditiveAlpha, additionalDefault);
    }

    template <typename T>
    ep_f64 get_alpha(T& obj, ep_f64 t, ep_f64 additionalDefault) {
        return get_alpha(obj.indexer.get(), t, additionalDefault);
    }
};

struct PhiObjectIndexer {
    ep_u64 index;

    ep_u64 get() {
        return index ? index : (index = reqGlobalCounter());
    }
};

struct PhiNote {
    PhiObjectIndexer indexer;

    struct State {
        ep_f64 lastUpdateTime;
        ep_bool playedHitsound;

        void timeUpdated(ep_f64 t) {
            if (lastUpdateTime > t) {
                playedHitsound = false;
            }

            lastUpdateTime = t;
        }

        ep_bool onPlayHitsound() {
            if (!playedHitsound) {
                playedHitsound = true;
                return true;
            }

            return false;
        }
    };

    EnumPhiNoteType type;
    ep_f64 time, holdTime;
    ep_bool isFake;

    ep_u64 lineIndex;
    Vec2 floorPosition;
    std::optional<ep_f64> fixedHoldSpeed;
    ep_bool isSimul;
    ep_bool isReversedCover;

    State state;

    void init(PhiAnimator& animator) {
        floorPosition = { getFloorPositionAt(time, animator), getFloorPositionAt(time + holdTime, animator) };
    }

    ep_f64 getFloorPositionAt(ep_f64 t, PhiAnimator& animator) {
        if (t > time && fixedHoldSpeed.has_value()) {
            return getFloorPositionAt(time, animator) + (t - time) * fixedHoldSpeed.value();
        }

        return animator.get(lineIndex, t, EnumPhiEventType::Speed) + animator.get(*this, t, EnumPhiEventType::Speed);
    }

    ep_bool isHold() {
        return holdTime > 0.0 || type == EnumPhiNoteType::Hold;
    }

    void reverseCover() {
        isReversedCover = !isReversedCover;
    }
};

struct PhiNoteGroup {
    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstNoteIndex;

        void timeUpdated(ep_f64 t) {
            if (lastUpdateTime > t) {
                firstNoteIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedNoteIndex(ep_u64 index) {
            if (firstNoteIndex == index) {
                firstNoteIndex++;
            }
        }
    };
    
    std::vector<ep_u64> indexs;
    bool breakable = true;

    State state;
};

struct PhiLine {
    PhiObjectIndexer indexer;

    std::vector<PhiBPMEvent> bpms;
    std::vector<PhiNote> notes;

    std::optional<ep_u64> fatherLineIndex;
    ep_f64 zOrder;
    ep_bool enableCover;
    Vec2 anchor = { 0.5, 0.5 };

    std::optional<std::string> textureName;
    std::optional<ep_u64> texture;
    std::optional<EnumPhiLineAttachUI> attachUI;

    std::vector<PhiNoteGroup> noteGroups;

    void init(PhiAnimator& animator) {
        std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });

        noteGroups.emplace_back().breakable = false;
        std::unordered_map<ep_u64, ep_u64> noteGroupMap;

        for (ep_u64 i = 0; i < notes.size(); i++) {
            auto& note = notes[i];
            note.lineIndex = indexer.get();
            note.init(animator);

            auto hash = animator.get_note_group_hash(note);
            if (hash.has_value()) {
                if (!noteGroupMap.contains(hash.value())) {
                    noteGroups.emplace_back();
                    noteGroupMap[hash.value()] = noteGroups.size() - 1;
                }

                auto& group = noteGroups[noteGroupMap[hash.value()]];
                group.indexs.push_back(i);
            } else noteGroups[0].indexs.push_back(i);
        }
    }

    ep_f64 beat2sec(ep_f64 beat) {
        if (bpms.size() == 1) return beat * (60.0 / bpms[0].bpm);

        ep_f64 t = 0.0;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];
            ep_f64 spb = 60.0 / e.bpm;

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;

                if (beat >= et_beat) {
                    t += et_beat * spb;
                    beat -= et_beat;
                } else {
                    t += beat * spb;
                    break;
                }
            } else {
                t += beat * spb;
            }
        }

        return t;
    }

    ep_f64 sec2beat(ep_f64 t) {
        if (bpms.size() == 1) return t / (60.0 / bpms[0].bpm);

        ep_f64 beat = 0.0;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];
            ep_f64 spb = 60.0 / e.bpm;

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;
                ep_f64 et_sec = et_beat * spb;

                if (t >= et_sec) {
                    beat += et_beat;
                    t -= et_sec;
                } else {
                    beat += t / spb;
                    break;
                }
            } else {
                beat += t / spb;
            }
        }

        return beat;
    }

    ep_f64 getBpmAtSecond(ep_f64 t) {
        if (bpms.size() == 1) return bpms[0].bpm;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;
                ep_f64 et_sec = et_beat * (60.0 / e.bpm);

                if (t >= et_sec) {
                    t -= et_sec;
                } else {
                    return e.bpm;
                }
            } else {
                return e.bpm;
            }
        }

        return 120.0;
    }
};

struct PhiHitEffectItem {
    static constexpr ep_u64 kParticleCount = 4;

    struct Particle {
        ep_f64 dt, rotation, size;
    };

    ep_f64 time;
    ep_u64 lineIndex, noteIndex;
    std::vector<Particle> particles;
};

struct PhiStoryboardAssets {
    // 用于区分是否到达了第一个
    static constexpr ep_f64 kTextIndexOffset = 1;
    static constexpr ep_f64 kColorIndexOffset = 1;

    std::vector<std::string> texts;
    std::vector<std::pair<ep_u64, Vec2>> textures; // id, size
    std::vector<Color> colors;

    std::function<std::optional<std::pair<ep_u64, Vec2>>(std::string)> textureLoader;
    std::function<void(ep_u64)> textureDestroyer;

    std::pair<ep_f64, ep_f64> requestTextPair(const std::string& start, const std::string& end) {
        if (texts.empty() || texts[texts.size() - 1] != start) texts.push_back(start);
        texts.push_back(end);
        return {
            texts.size() - 2 + kTextIndexOffset,
            texts.size() - 1 + kTextIndexOffset
        };
    }

    std::pair<ep_f64, ep_f64> requestColorPair(const Color& start, const Color& end) {
        if (colors.empty() || colors[colors.size() - 1] != start) colors.push_back(start);
        colors.push_back(end);
        return {
            colors.size() - 2 + kColorIndexOffset,
            colors.size() - 1 + kColorIndexOffset
        };
    }

    std::optional<std::string> getText(ep_f64 index) {
        if (index < kTextIndexOffset) return std::nullopt;
        index -= kTextIndexOffset;

        if (index >= texts.size()) return std::nullopt;
        return texts[(ep_u64)index];
    }

    Color getColor(ep_f64 index, const Color& defaultValue) {
        if (index < kColorIndexOffset) return defaultValue;
        index -= kColorIndexOffset;

        if (index >= colors.size()) return defaultValue;

        auto start = colors[(ep_u64)index];
        auto end = colors[(ep_u64)std::ceil(index)];
        auto p = std::fmod(index, 1.0);

        return start * (1.0 - p) + end * p;
    }

    std::optional<ep_u64> requestLoadTexture(const std::string& name) {
        if (!textureLoader) return std::nullopt;
        auto id = textureLoader(name);
        if (id.has_value()) {
            textures.push_back(id.value());
            return textures.size() - 1;
        } else {
            return std::nullopt;
        }
    }

    std::pair<ep_u64, Vec2> getTexture(ep_u64 index) noexcept {
        return textures[index];
    }

    void clearTextures() {
        for (auto& texture : textures) {
            if (textureDestroyer) {
                textureDestroyer(texture.first);
            }
        }

        textures.clear();
    }

    ~PhiStoryboardAssets() {
        clearTextures();
    }
};

struct PhiChart {
    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstHitEffectIndex;

        void timeUpdated(ep_f64 t) {
            if (lastUpdateTime > t) {
                firstHitEffectIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedHitEffectIndex(ep_u64 index) {
            if (firstHitEffectIndex == index) {
                firstHitEffectIndex++;
            }
        }
    };

    struct UserOptions {
        Vec2 noteScaling = { 1.0, 1.0 };

        ep_f64 unsafeBackgroundDim = 0.8;
        ep_f64 backgroundDim = 0.6;
        ep_f64 backgroundTextureBlurRadius = (ep_f64)1 / 50;

        Color lineDefaultColor = { (ep_f64)0xff / 0xff, (ep_f64)0xec / 0xff, (ep_f64)0x9f / 0xff, 1.0 };

        std::pair<Color, Color> progressBarDefaultColor = {
            { (ep_f64)145 / 255, (ep_f64)145 / 255, (ep_f64)145 / 255, 0.85 },
            { 1.0, 1.0, 1.0, 0.9 }
        };

        Vec2 storyboardTextBaseSize = { 0.028125, 0.0 };

        enum class EnumStoryboardTextureSclaingBehavior {
            AboutWidth,
            AboutHeight,
            Stretch
        };
        EnumStoryboardTextureSclaingBehavior storyboardTextureSclaingBehavior = EnumStoryboardTextureSclaingBehavior::AboutWidth;
        Vec2 storyboardTextureScaling = { 1.0, 1.0 };

        ep_f64 hitEffectDuration = 0.5;
        ep_f64 hitEffectAlpha = (ep_f64)0xe1 / 0xff;
        ep_f64 hitEffectTextureScaling = 1.54;
        ep_f64 hitEffectParticleSize = 1.0;
        ep_f64 hitEffectParticleDistance = 1.0;

        ep_bool enableNoteOffScreenBreakOptimization = true;
    };

    PhiMeta meta;
    std::vector<PhiLine> lines;
    PhiAnimator animator;
    PhiStoryboardAssets storyboardAssets;

    std::vector<PhiHitEffectItem> hitEffects;
    std::vector<ep_f64> comboTimes;
    std::vector<ep_u64> zOrderSortedLines;

    UserOptions options;

    State state;

    void init() {
        animator.init();

        for (auto& line : lines) {
            if (line.textureName.has_value()) {
                line.texture = storyboardAssets.requestLoadTexture(line.textureName.value());
            }

            line.init(animator);
        }

        initSimulNote();
        initHitEffects();
        initPlayemntInfo();
        initZOrderSortedLines();
    }

    Vec2 getLinePositionRaw(ep_f64 t, PhiLine& line) {
        return {
            animator.get(line, t, EnumPhiEventType::PositionX),
            animator.get(line, t, EnumPhiEventType::PositionY)
        };
    }

    Vec2 getLinePositionRelOrigin(ep_f64 t, PhiLine& line, const Vec2& screenSize) {
        Vec2 pos = getLinePositionRaw(t, line);
        pos = pos / meta.worldViewport * screenSize;

        if (line.fatherLineIndex.has_value()) {
            auto fatherLineIndex = line.fatherLineIndex.value();
            if (0 <= fatherLineIndex && fatherLineIndex < lines.size()) {
                auto& fatherLine = lines[fatherLineIndex];
                auto fatherLinePosition = getLinePositionRelOrigin(t, fatherLine, screenSize);
                auto fatherLineRotation = animator.get(fatherLine, t, EnumPhiEventType::SelfRotation);

                pos = fatherLinePosition.rotateDegress(
                    fatherLineRotation + std::atan2(pos.y, pos.x) * 180.0 / std::numbers::pi,
                    pos.length()
                );
            }
        }

        return pos;
    }

    Vec2 getLinePosition(ep_f64 t, PhiLine& line, const Vec2& screenSize) {
        Vec2 ori = getLinePositionRelOrigin(t, line, screenSize);
        return ori - meta.worldOrigin / meta.worldViewport * screenSize;
    }

    struct NoteFrameInfo {
        Vec2 headPosition, tailPosition;
        ep_bool isArrived = false;
        ep_bool isVisible = true;
        ep_f64 lineRotation, textureRotation, speedVectorRotation;
        Color color;
        Vec2 scale;
        ep_f64 speedCoefficient;
    };

    NoteFrameInfo getNoteFrameInfo(
        PhiLine& line, PhiNote& note,
        ep_f64 time, const Vec2& screenSize
    ) {
        NoteFrameInfo info {};

        auto linePosition = getLinePosition(time, line, screenSize);
        auto lineRotation = animator.get(line, time, EnumPhiEventType::SelfRotation);
        auto lineSpeedCoefficient = animator.get(line, time, EnumPhiEventType::SpeedCoefficient);
        auto lineAlpha = animator.get_alpha(line, time, 0.0);
        auto noteRotation = animator.get(note, time, EnumPhiEventType::SelfRotation);
        auto noteAxisRotation = animator.get(note, time, EnumPhiEventType::AxisRotation);
        auto noteColorIndex = animator.get(note, time, EnumPhiEventType::Color);
        auto noteColor = storyboardAssets.getColor(noteColorIndex, { 1.0, 1.0, 1.0, 1.0 });
        auto noteAlpha = animator.get_alpha(note, time, 1.0);
        auto noteScaling = Vec2 {
            animator.get(note, time, EnumPhiEventType::ScaleX),
            animator.get(note, time, EnumPhiEventType::ScaleY)
        };
        
        Transform2D lineTransform {};
        lineTransform.translate(linePosition);
        lineTransform.rotateDegress(lineRotation);
        lineTransform.rotateDegress(noteAxisRotation);
        lineTransform.scale(screenSize);
        lineTransform.scale(1.0, -1.0);

        info.isArrived = time >= note.time;
        auto noteTotalFloorPosition = note.getFloorPositionAt(time, animator);
        auto noteSpeedCoefficient = lineSpeedCoefficient * animator.get(note, time, EnumPhiEventType::SpeedCoefficient);
        auto noteFloorPosition = (note.floorPosition - noteTotalFloorPosition) * noteSpeedCoefficient;
        Vec2 noteBasePosition = { animator.get(note, time, EnumPhiEventType::PositionX), animator.get(note, time, EnumPhiEventType::PositionY) };

        auto noteRelPositionHead = noteBasePosition + Vec2 { 0.0, info.isArrived ? 0.0 : noteFloorPosition.x },
             noteRelPositionTail = noteBasePosition + Vec2 { 0.0, noteFloorPosition.y };
        
        if (line.enableCover && !info.isArrived) {
            if (meta.isHoldCoverAtHead && noteRelPositionHead.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.isVisible = false;
            if (!meta.isHoldCoverAtHead && noteRelPositionTail.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.isVisible = false;
        }

        if (note.isHold() && meta.isZeroLengthHoldHidden && note.floorPosition.xyDiff() == 0) info.isVisible = false;
        if (noteRelPositionHead.y > 2.0 && meta.isHighNoteHidden) info.isVisible = false;
        if (meta.isRegLineAlphaNoteHidden && lineAlpha < 0.0) info.isVisible = false;

        info.headPosition = lineTransform.transformPoint(noteRelPositionHead);
        info.tailPosition = lineTransform.transformPoint(noteRelPositionTail);
        info.lineRotation = lineRotation;
        info.textureRotation = lineRotation + noteRotation + noteAxisRotation;
        info.speedVectorRotation = lineRotation + noteAxisRotation;
        if (noteSpeedCoefficient < 0) info.speedVectorRotation += 180.0;
        info.color = noteColor.applyAlpha(noteAlpha);
        info.scale = noteScaling;
        info.speedCoefficient = noteSpeedCoefficient;

        return info;
    }

    ep_u64 getCombo(ep_f64 t) {
        if (comboTimes.empty() || comboTimes[0] > t) return 0;

        ep_u64 left = 0, right = comboTimes.size() - 1;
        ep_u64 ans = 1;

        while (left <= right) {
            ep_u64 mid = left + (right - left) / 2;
            if (comboTimes[mid] <= t) {
                ans = mid + 1;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        return ans;
    }

    private:
    void initSimulNote() {
        std::unordered_map<ep_f64, ep_u64> noteTimes;
        for (auto& line : lines) {
            for (auto& note : line.notes) {
                noteTimes[note.time]++;
            }
        }

        for (auto& line : lines) {
            for (auto& note : line.notes) {
                note.isSimul = noteTimes[note.time] > 1;
            }
        }
    }

    void initHitEffects() {
        std::mt19937 rng { std::random_device {} () };
        std::uniform_real_distribution<ep_f64> rng_dist { 0.0, 1.0 };
        auto uniform = [&](ep_f64 a, ep_f64 b) { return a + (b - a) * rng_dist(rng); };

        hitEffects.clear();

        for (auto& line : lines) {
            for (auto& note : line.notes) {
                if (note.isFake) continue;

                ep_f64 t = note.time;
                while (t <= note.time + note.holdTime) {
                    auto& item = hitEffects.emplace_back();
                    item.time = t;
                    item.lineIndex = &line - lines.data();
                    item.noteIndex = &note - line.notes.data();

                    for (ep_u64 i = 0; i < PhiHitEffectItem::kParticleCount; i++) {
                        auto& particle = item.particles.emplace_back();
                        particle.rotation = uniform(0.0, 360.0);
                        particle.size = uniform(185.0, 265.0);
                    }

                    t += 30.0 / line.getBpmAtSecond(t);
                }
            }
        }

        std::stable_sort(hitEffects.begin(), hitEffects.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }

    void initPlayemntInfo() {
        for (auto& line : lines) {
            for (auto& note : line.notes) {
                if (note.isFake) continue;

                comboTimes.push_back(note.time);
                if (note.isHold()) {
                    comboTimes.push_back(note.time + std::max(0.0, note.holdTime - 0.2));
                }
            }
        }

        std::sort(comboTimes.begin(), comboTimes.end());
    }

    void initZOrderSortedLines() {
        zOrderSortedLines.clear();
        for (ep_u64 i = 0; i < lines.size(); i++) zOrderSortedLines.push_back(i);

        std::stable_sort(zOrderSortedLines.begin(), zOrderSortedLines.end(), [&](ep_u64 a, ep_u64 b){
            return lines[a].zOrder < lines[b].zOrder;
        });
    }
};

std::string toUtfChar(ep_u16 n, ep_u16 n2 = 0) {
    ep_u32 codepoint;
    
    if (n >= 0xD800 && n <= 0xDBFF) {
        if (n2 >= 0xDC00 && n2 <= 0xDFFF) {
            codepoint = 0x10000 + ((n - 0xD800) << 10) | (n2 - 0xDC00);
        } else {
            return "\xEF\xBF\xBD";
        }
    } else if (n >= 0xDC00 && n <= 0xDFFF) {
        return "\xEF\xBF\xBD";
    } else {
        codepoint = n;
    }
    
    std::string result;
    
    if (codepoint <= 0x7F) {
        result.push_back((char)(codepoint));
    } 
    else if (codepoint <= 0x7FF) {
        result.push_back((char)(0xC0 | (codepoint >> 6)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    } 
    else if (codepoint <= 0xFFFF) {
        result.push_back((char)(0xE0 | (codepoint >> 12)));
        result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    } 
    else {
        result.push_back((char)(0xF0 | (codepoint >> 18)));
        result.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    
    return result;
}

std::string formatToStdString(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (len < 0) return "";

    std::vector<char> buf(len + 1);
    va_start(args, fmt);
    vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);

    return std::string(buf.data(), len);
}

struct JsonNode {
    enum class EnumType {
        String, Number, Bool, Array, Object, Null
    };

    EnumType type;
    std::variant<
        std::monostate,
        std::string,
        ep_f64,
        ep_bool,
        std::vector<JsonNode>,
        std::unordered_map<std::string, JsonNode>
    > value;

    static JsonNode MakeString(const std::string& str) {
        return JsonNode {
            .type = EnumType::String,
            .value = str
        };
    }

    static JsonNode MakeNumber(ep_f64 num) {
        return JsonNode {
            .type = EnumType::Number,
            .value = num
        };
    }

    static JsonNode MakeBool(ep_bool b) {
        return JsonNode {
            .type = EnumType::Bool,
            .value = b
        };
    }

    static JsonNode MakeArray(const std::vector<JsonNode>& arr) {
        return JsonNode {
            .type = EnumType::Array,
            .value = arr
        };
    }

    static JsonNode MakeObject(const std::unordered_map<std::string, JsonNode>& obj) {
        return JsonNode {
            .type = EnumType::Object,
            .value = obj
        };
    }

    static JsonNode MakeNull() {
        return JsonNode {
            .type = EnumType::Null,
            .value = std::monostate{}
        };
    }

    ep_bool isString() const { return type == EnumType::String; }
    ep_bool isNumber() const { return type == EnumType::Number; }
    ep_bool isBool() const { return type == EnumType::Bool; }
    ep_bool isArray() const { return type == EnumType::Array; }
    ep_bool isObject() const { return type == EnumType::Object; }
    ep_bool isNull() const { return type == EnumType::Null; }

    std::string& getString() noexcept { return std::get<std::string>(value); }
    std::string getString() const noexcept { return std::get<std::string>(value); }
    ep_f64 getNumber() const noexcept { return std::get<ep_f64>(value); }
    ep_bool getBool() const noexcept { return std::get<ep_bool>(value); }
    std::vector<JsonNode>& getArray() noexcept { return std::get<std::vector<JsonNode>>(value); }
    std::vector<JsonNode> getArray() const noexcept { return std::get<std::vector<JsonNode>>(value); }
    std::unordered_map<std::string, JsonNode>& getObject() noexcept { return std::get<std::unordered_map<std::string, JsonNode>>(value); }
    std::unordered_map<std::string, JsonNode> getObject() const noexcept { return std::get<std::unordered_map<std::string, JsonNode>>(value); }
    
    struct StringReader {
        std::string str;
        ep_u64 pos;

        StringReader(const std::string& str) : str(str), pos(0) {}

        void eatWhitespace() {
            while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == '\r')) {
                pos++;
            }
        }

        ep_bool nextIs(const char c) {
            return pos < str.size() && str[pos] == c;
        }

        ep_bool nextIsAny(const std::string& s) {
            for (ep_u64 i = 0; i < s.size(); i++) {
                if (nextIs(s[i])) return true;
            }
            return false;
        }

        ep_bool nextIsSub(const std::string& s) {
            return pos + s.size() <= str.size() && str.substr(pos, s.size()) == s;
        }

        ep_bool nextIsSubAny(const std::vector<std::string>& ss) {
            for (const auto& s : ss) {
                if (nextIsSub(s)) return true;
            }
            return false;
        }

        std::string getNextCharToString() {
            return pos < str.size() ? (std::string() + str[pos++]) : "";
        }

        std::string generatePositionString() {
            return "at " + std::to_string(pos) + " of " + std::to_string(str.size());
        }

        ep_bool eof() {
            return pos >= str.size();
        }

        ep_bool readUnicodeEscape(ep_u16* dst) {
            if (pos + 4 > str.size()) return false;

            auto c1 = str[pos++];
            auto c2 = str[pos++];
            auto c3 = str[pos++];
            auto c4 = str[pos++];

            if ('0' <= c1 && c1 <= '9') {
                *dst = (ep_u16)(c1 - '0') << 12;
            } else if ('A' <= c1 && c1 <= 'F') {
                *dst = (ep_u16)(c1 - 'A' + 10) << 12;
            } else if ('a' <= c1 && c1 <= 'f') {
                *dst = (ep_u16)(c1 - 'a' + 10) << 12;
            } else return false;

            if ('0' <= c2 && c2 <= '9') {
                *dst |= (ep_u16)(c2 - '0') << 8;
            } else if ('A' <= c2 && c2 <= 'F') {
                *dst |= (ep_u16)(c2 - 'A' + 10) << 8;
            } else if ('a' <= c2 && c2 <= 'f') {
                *dst |= (ep_u16)(c2 - 'a' + 10) << 8;
            } else return false;

            if ('0' <= c3 && c3 <= '9') {
                *dst |= (ep_u16)(c3 - '0') << 4;
            } else if ('A' <= c3 && c3 <= 'F') {
                *dst |= (ep_u16)(c3 - 'A' + 10) << 4;
            } else if ('a' <= c3 && c3 <= 'f') {
                *dst |= (ep_u16)(c3 - 'a' + 10) << 4;
            } else return false;

            if ('0' <= c4 && c4 <= '9') {
                *dst |= (ep_u16)(c4 - '0');
            } else if ('A' <= c4 && c4 <= 'F') {
                *dst |= (ep_u16)(c4 - 'A' + 10);
            } else if ('a' <= c4 && c4 <= 'f') {
                *dst |= (ep_u16)(c4 - 'a' + 10);
            } else return false;

            return true;
        }
        
        std::string getNextToString(ep_u64 len) {
            return str.substr(pos, len);
        }

        char getNextChar() {
            return pos < str.size() ? str[pos++] : '\0';
        }
    };

    static std::pair<ep_bool, std::string> Parse(JsonNode* dst, StringReader& reader) {
        #define FAILED(err, msg) \
            { \
                return { false, std::string(err) + ": " + msg + " " + reader.generatePositionString() }; \
            }
        
        reader.eatWhitespace();

        if (reader.nextIs('"')) {
            reader.pos++;
            std::string str;
            ep_bool isInBackslash = false;

            while (!reader.eof()) {
                if (reader.nextIs('"') && !isInBackslash) {
                    reader.pos++;
                    *dst = MakeString(str);
                    return { true, "" };
                } else if (reader.nextIs('\\') && !isInBackslash) {
                    isInBackslash = true;
                    reader.pos++;
                } else if (isInBackslash) {
                    if (reader.nextIsAny("\"\\/")) {
                        str += reader.getNextCharToString();
                        reader.pos++;
                    } else if (reader.nextIs('b')) {
                        str += '\b';
                        reader.pos++;
                    } else if (reader.nextIs('f')) {
                        str += '\f';
                        reader.pos++;
                    } else if (reader.nextIs('n')) {
                        str += '\n';
                        reader.pos++;
                    } else if (reader.nextIs('r')) {
                        str += '\r';
                        reader.pos++;
                    } else if (reader.nextIs('t')) {
                        str += '\t';
                        reader.pos++;
                    } else if (reader.nextIs('u')) {
                        reader.pos++;

                        ep_u16 u1;
                        if (!reader.readUnicodeEscape(&u1)) FAILED("invalid unicode escape", reader.getNextCharToString());

                        ep_u16 u2 = 0;
                        if (u1 >= 0xD800 && u1 <= 0xDBFF) {
                            if (!reader.nextIsSub("\\u")) FAILED("expected \\u for surrogate pair", reader.getNextCharToString());
                            reader.pos += 2;

                            if (!reader.readUnicodeEscape(&u2)) FAILED("invalid unicode escape", reader.getNextCharToString());
                            if (u2 < 0xDC00 || u2 > 0xDFFF) FAILED("invalid surrogate pair", reader.getNextCharToString());
                        } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
                            FAILED("invalid low surrogate", reader.getNextCharToString());
                        }

                        str += toUtfChar(u1, u2);
                    } else {
                        FAILED("unexpected char after backslash", reader.getNextCharToString());
                    }

                    isInBackslash = false;
                } else {
                    str += reader.getNextCharToString();
                }
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIsAny("0123456789-")) {
            ep_f64 num = 0;
            ep_bool isNegative = reader.nextIs('-');
            if (isNegative) reader.pos++;

            ep_bool afterDot = false;
            ep_u64 decimal = 1;
            ep_f64 fraction = 0;
            ep_bool hasFraction = false;

            while (!reader.eof()) {
                ep_u8 c = reader.getNextChar();

                if ('0' <= c && c <= '9') {
                    if (!afterDot) {
                        num *= 10;
                        num += (ep_f64)(c - '0');
                    } else {
                        fraction = fraction * 10 + (ep_f64)(c - '0');
                        decimal *= 10;
                        hasFraction = true;
                    }
                } else if (c == '.') {
                    if (afterDot) FAILED("unexpected dot", reader.getNextCharToString());
                    afterDot = true;
                } else if (c == 'e' || c == 'E') {
                    if (hasFraction) num += fraction / (ep_f64)decimal;
                    
                    ep_bool isNegativeExp = reader.nextIs('-');
                    if (isNegativeExp) reader.pos++;
                    else if (reader.nextIs('+')) reader.pos++;

                    ep_u64 exp = 0;
                    ep_bool hasExp = false;
                    while (!reader.eof()) {
                        ep_u8 c = reader.getNextChar();

                        if ('0' <= c && c <= '9') {
                            exp *= 10;
                            exp += (ep_u64)(c - '0');
                            hasExp = true;
                        } else {
                            reader.pos--;
                            break;
                        }
                    }
                    
                    if (!hasExp) FAILED("expected exponent digits", reader.getNextCharToString());

                    if (isNegativeExp) num /= std::pow<ep_f64>(10, exp);
                    else num *= std::pow<ep_f64>(10, exp);
                    *dst = MakeNumber(num * (isNegative ? -1 : 1));
                    return { true, "" };
                } else {
                    reader.pos--;
                    if (hasFraction) num += fraction / (ep_f64)decimal;
                    *dst = MakeNumber(num * (isNegative ? -1 : 1));
                    return { true, "" };
                }
            }
            
            if (hasFraction) num += fraction / (ep_f64)decimal;
            *dst = MakeNumber(num * (isNegative ? -1 : 1));
            return { true, "" };
        } else if (reader.nextIsSubAny({ "true", "false" })) {
            ep_bool b = reader.nextIsSub("true");
            *dst = MakeBool(b);
            reader.pos += b ? 4 : 5;
            return { true, "" };
        } else if (reader.nextIs('[')) {
            reader.pos++;
            std::vector<JsonNode> arr;

            while (!reader.eof()) {
                reader.eatWhitespace();

                if (reader.nextIs(']')) {
                    *dst = MakeArray(arr);
                    reader.pos++;
                    return { true, "" };
                }

                if (arr.size()) {
                    if (!reader.nextIs(',')) FAILED("expected comma", reader.getNextCharToString());
                    reader.pos++;
                    reader.eatWhitespace();
                }

                JsonNode node;
                auto [success, err] = Parse(&node, reader);
                if (!success) return { false, err };

                arr.push_back(node);
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIs('{')) {
            reader.pos++;
            std::unordered_map<std::string, JsonNode> obj;

            while (!reader.eof()) {
                reader.eatWhitespace();

                if (reader.nextIs('}')) {
                    *dst = MakeObject(obj);
                    reader.pos++;
                    return { true, "" };
                }

                if (obj.size()) {
                    if (!reader.nextIs(',')) FAILED("expected comma", reader.getNextCharToString());
                    reader.pos++;
                    reader.eatWhitespace();
                }

                JsonNode key;
                {
                    auto [success, err] = Parse(&key, reader);
                    if (!success) return { false, err };
                }

                if (!key.isString()) FAILED("expected string", key.getString());

                reader.eatWhitespace();
                if (!reader.nextIs(':')) FAILED("expected colon", reader.getNextCharToString());
                reader.pos++;
                reader.eatWhitespace();

                JsonNode value;
                {
                    auto [success, err] = Parse(&value, reader);
                    if (!success) return { false, err };
                }

                obj[key.getString()] = value;
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIsSub("null")) {
            *dst = MakeNull();
            reader.pos += 4;
            return { true, "" };
        }

        FAILED("unexpected char", reader.getNextCharToString());
        #undef FAILED
    }

    static std::pair<ep_bool, std::string> Parse(JsonNode* dst, const Data& data) {
        StringReader reader(std::string(data.data.begin(), data.data.end()));
        return Parse(dst, reader);
    }

    template<typename T>
    void Print(T& stream) {
        if (isString()) {
            stream << '"';
            for (ep_u8 c : getString()) {
                if (c == '"') stream << "\\\"";
                else if (c == '\\') stream << "\\\\";
                else if (c == '\n') stream << "\\n";
                else if (c == '\r') stream << "\\r";
                else if (c == '\t') stream << "\\t";
                else if (c == '\b') stream << "\\b";
                else if (c == '\f') stream << "\\f";
                else stream << c;
            }
            stream << '"';
        } else if (isNumber()) stream << formatToStdString("%.10g", getNumber());
        else if (isBool()) stream << (getBool() ? "true" : "false");
        else if (isArray()) {
            stream << '[';
            for (ep_u64 i = 0; i < getArray().size(); i++) {
                if (i) stream << ',';
                getArray()[i].Print(stream);
            }
            stream << ']';
        } else if (isObject()) {
            stream << '{';
            ep_u64 i = 0;
            for (auto& [key, value] : getObject()) {
                JsonNode::MakeString(key).Print(stream);
                stream << ':';
                value.Print(stream);
                if (i < getObject().size() - 1) stream << ',';
                i++;
            }
            stream << '}';
        } else if (isNull()) stream << "null";
    }

    void Print() {
        Print(std::cout);
    }

    ep_bool operator==(const JsonNode& other) const {
        if (type != other.type) return false;
        if (type == EnumType::Null) return true;
        if (type == EnumType::String) return getString() == other.getString();
        if (type == EnumType::Number) return getNumber() == other.getNumber();
        if (type == EnumType::Bool) return getBool() == other.getBool();
        if (type == EnumType::Array) {
            if (getArray().size() != other.getArray().size()) return false;
            for (ep_u32 i = 0; i < getArray().size(); i++) {
                if (getArray()[i] != other.getArray()[i]) return false;
            }
            return true;
        }
        if (type == EnumType::Object) {
            if (getObject().size() != other.getObject().size()) return false;
            for (auto& [key, value] : getObject()) {
                if (value != other[key]) return false;
            }
            return true;
        }
        return false;
    }

    ep_bool operator!=(const JsonNode& other) const {
        return !(*this == other);
    }

    JsonNode operator[](ep_u64 index) const noexcept {
        return getArray()[index];
    }

    JsonNode operator[](const std::string& key) const noexcept {
        return getObject()[key];
    }

    JsonNode& operator[](ep_u64 index) noexcept {
        auto& arr = getArray();
        return arr[index];
    }

    JsonNode& operator[](const std::string& key) noexcept {
        auto& obj = getObject();
        return obj[key];
    }

    ep_bool hasKey(const std::string& key) const {
        if (type != EnumType::Object) return false;
        return getObject().contains(key);
    }
};

struct PhiChartLoadResult {
    ep_bool success;
    std::vector<std::string> errors;
    PhiChart chart;
};

struct TokenStringReader {
    std::string str;
    ep_u64 pos = 0;

    TokenStringReader(const std::string& str) : str(str) {}

    bool nextToken(std::string& dst) {
        jumpToNextNonWhiteSpace();
        if (pos >= str.size()) return false;
        ep_u64 start = pos;
        jumpToNextWhiteSpace();
        if (pos == start) return false;
        dst = str.substr(start, pos - start);
        jumpToNextNonWhiteSpace();
        return true;
    }

    private:
    bool currentIsWhiteSpace() const {
        return str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v';
    }

    void jumpToNextWhiteSpace() {
        while (pos < str.size() && !currentIsWhiteSpace()) pos++;
    }

    void jumpToNextNonWhiteSpace() {
        while (pos < str.size() && currentIsWhiteSpace()) pos++;
    }
};

#define CHART_LOAD_FAILED(prefix, err) \
    { \
        return PhiChartLoadResult { \
            .success = false, \
            .errors = { std::string(prefix) + ": " + (err) } \
        }; \
    }

PhiChartLoadResult loadChartFromOfficialJson(const Data& data) {
    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) CHART_LOAD_FAILED("official", std::string("failed to parse json: ") + err);

    PhiChart chart {};

    if (!jsonRoot.isObject()) CHART_LOAD_FAILED("official", "root is not an object");

    if (!jsonRoot.hasKey("formatVersion")) CHART_LOAD_FAILED("official", "missing formatVersion field");
    if (!jsonRoot["formatVersion"].isNumber()) CHART_LOAD_FAILED("official", "formatVersion is not a number");
    ep_u64 formatVersion = jsonRoot["formatVersion"].getNumber();

    chart.meta.isHoldCoverAtHead = true;
    chart.meta.isZeroLengthHoldHidden = true;
    chart.meta.isHighNoteHidden = true;
    chart.meta.isRegLineAlphaNoteHidden = false;
    chart.meta.lineWidthUnit = { 0.0, 5.76 };
    chart.meta.lineHeightUnit = { 0.0, 0.0075 };
    chart.meta.worldOrigin = { 0.0, 1.0 };
    chart.meta.worldViewport = { 1.0, -1.0 };

    if (1 <= formatVersion && formatVersion <= 3) {
        if (!jsonRoot.hasKey("offset")) CHART_LOAD_FAILED("official", "missing offset field");
        if (!jsonRoot["offset"].isNumber()) CHART_LOAD_FAILED("official", "offset is not a number");
        chart.meta.offset = jsonRoot["offset"].getNumber();

        if (!jsonRoot.hasKey("judgeLineList")) CHART_LOAD_FAILED("official", "missing judgeLineList field");
        if (!jsonRoot["judgeLineList"].isArray()) CHART_LOAD_FAILED("official", "judgeLineList is not an array");
        auto& judgeLineListNode = jsonRoot["judgeLineList"];

        for (auto& judgeLineNode : judgeLineListNode.getArray()) {
            if (!judgeLineNode.isObject()) CHART_LOAD_FAILED("official", "judgeLineList item is not an object");
            
            auto& line = chart.lines.emplace_back();
            line.enableCover = true;

            if (!judgeLineNode.hasKey("bpm")) CHART_LOAD_FAILED("official", "missing bpm field");
            if (!judgeLineNode["bpm"].isNumber()) CHART_LOAD_FAILED("official", "bpm is not a number");
            ep_f64 bpm = judgeLineNode["bpm"].getNumber();
            ep_f64 timeFactor = 1.875 / bpm;
            line.bpms = { { 0, bpm } };

            if (!judgeLineNode.hasKey("notesAbove")) CHART_LOAD_FAILED("official", "missing notesAbove field");
            if (!judgeLineNode["notesAbove"].isArray()) CHART_LOAD_FAILED("official", "notesAbove is not an array");
            if (!judgeLineNode.hasKey("notesBelow")) CHART_LOAD_FAILED("official", "missing notesBelow field");
            if (!judgeLineNode["notesBelow"].isArray()) CHART_LOAD_FAILED("official", "notesBelow is not an array");

            auto& notesAboveNode = judgeLineNode["notesAbove"];
            auto& notesBelowNode = judgeLineNode["notesBelow"];
            std::vector<std::pair<JsonNode*, ep_bool>> noteGroups = {
                { &notesAboveNode, true },
                { &notesBelowNode, false }
            };

            for (auto& [ notesNodePtr, isAbove ] : noteGroups) {
                auto& notesNode = *notesNodePtr;

                for (auto& noteNode : notesNode.getArray()) {
                    if (!noteNode.isObject()) CHART_LOAD_FAILED("official", "notesAbove/notesBelow item is not an object");

                    if (!noteNode.hasKey("type")) CHART_LOAD_FAILED("official", "missing type field");
                    if (!noteNode["type"].isNumber()) CHART_LOAD_FAILED("official", "type is not a number");
                    auto type = PhiNoteTypeHelper::FromOfficial(noteNode["type"].getNumber());

                    if (!noteNode.hasKey("time")) CHART_LOAD_FAILED("official", "missing time field");
                    if (!noteNode["time"].isNumber()) CHART_LOAD_FAILED("official", "time is not a number");
                    auto time = noteNode["time"].getNumber() * timeFactor;

                    if (!noteNode.hasKey("holdTime")) CHART_LOAD_FAILED("official", "missing holdTime field");
                    if (!noteNode["holdTime"].isNumber()) CHART_LOAD_FAILED("official", "holdTime is not a number");
                    auto holdTime = noteNode["holdTime"].getNumber() * timeFactor;

                    if (!noteNode.hasKey("positionX")) CHART_LOAD_FAILED("official", "missing positionX field");
                    if (!noteNode["positionX"].isNumber()) CHART_LOAD_FAILED("official", "positionX is not a number");
                    auto positionX = noteNode["positionX"].getNumber() * 0.05625;

                    std::string speedKey = "speed";
                    if (!noteNode.hasKey(speedKey)) CHART_LOAD_FAILED("official", std::string("missing ") + speedKey + " field");
                    if (!noteNode[speedKey].isNumber()) CHART_LOAD_FAILED("official", speedKey + " is not a number");
                    auto speed = noteNode[speedKey].getNumber();

                    auto& note = line.notes.emplace_back();
                    note.type = type;
                    note.time = time;
                    note.holdTime = holdTime;
                    note.isFake = false;

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { positionX, positionX },
                        .type = EnumPhiEventType::PositionX,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });

                    if (type == EnumPhiNoteType::Hold) {
                        note.fixedHoldSpeed = speed * 0.6;
                    } else {
                        if (speed != 1.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { speed, speed },
                                .type = EnumPhiEventType::SpeedCoefficient,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });
                        }
                    }

                    if (!isAbove) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { -1.0, -1.0 },
                            .type = EnumPhiEventType::SpeedCoefficient,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { 180.0, 180.0 },
                            .type = EnumPhiEventType::SelfRotation,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        note.reverseCover();
                    }
                }
            }

            if (!judgeLineNode.hasKey("speedEvents")) CHART_LOAD_FAILED("official", "missing speedEvents field");
            if (!judgeLineNode["speedEvents"].isArray()) CHART_LOAD_FAILED("official", "speedEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineMoveEvents")) CHART_LOAD_FAILED("official", "missing judgeLineMoveEvents field");
            if (!judgeLineNode["judgeLineMoveEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineMoveEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineRotateEvents")) CHART_LOAD_FAILED("official", "missing judgeLineRotateEvents field");
            if (!judgeLineNode["judgeLineRotateEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineRotateEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineDisappearEvents")) CHART_LOAD_FAILED("official", "missing judgeLineDisappearEvents field");
            if (!judgeLineNode["judgeLineDisappearEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineDisappearEvents is not an array");

            auto& speedEventsNode = judgeLineNode["speedEvents"];
            auto& judgeLineMoveEventsNode = judgeLineNode["judgeLineMoveEvents"];
            auto& judgeLineRotateEventsNode = judgeLineNode["judgeLineRotateEvents"];
            auto& judgeLineDisappearEventsNode = judgeLineNode["judgeLineDisappearEvents"];

            // events, startKey, endKey, easeTypeKey, type, converter
            std::vector<std::tuple<JsonNode*, std::string, std::string, std::string, EnumPhiEventType, std::function<ep_f64(ep_f64)>>> eventGroups;
            
            if (formatVersion == 1) {
                eventGroups = {
                    { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](ep_f64 v) { return v * 0.6; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](ep_f64 v) { return std::floor(v / 1000.0) / 880.0; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionY, [](ep_f64 v) { return std::fmod(v, 1000.0) / 520.0; } },
                    { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](ep_f64 v) { return -v; } },
                    { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v; } }
                };
            } else if (formatVersion == 3) {
                eventGroups = {
                    { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](ep_f64 v) { return v * 0.6; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](ep_f64 v) { return v; } },
                    { &judgeLineMoveEventsNode, "start2", "end2", "", EnumPhiEventType::PositionY, [](ep_f64 v) { return v; } },
                    { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](ep_f64 v) { return -v; } },
                    { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v; } }
                };
            }

            for (auto& [eventsNode, startKey, endKey, easeTypeKey, type, converter] : eventGroups) {
                if (!eventsNode->isArray()) CHART_LOAD_FAILED("official", "XXXEvents is not an array");

                for (auto& eventNode : eventsNode->getArray()) {
                    if (!eventNode.isObject()) CHART_LOAD_FAILED("official", "XXXEvents item is not an object");

                    if (!eventNode.hasKey("startTime")) CHART_LOAD_FAILED("official", "missing startTime field");
                    if (!eventNode["startTime"].isNumber()) CHART_LOAD_FAILED("official", "startTime is not a number");
                    auto startTime = eventNode["startTime"].getNumber() * timeFactor;

                    if (!eventNode.hasKey("endTime")) CHART_LOAD_FAILED("official", "missing endTime field");
                    if (!eventNode["endTime"].isNumber()) CHART_LOAD_FAILED("official", "endTime is not a number");
                    auto endTime = eventNode["endTime"].getNumber() * timeFactor;

                    if (!eventNode.hasKey(startKey)) CHART_LOAD_FAILED("official", std::string("missing ") + startKey + " field");
                    if (!eventNode[startKey].isNumber()) CHART_LOAD_FAILED("official", std::string(startKey) + " is not a number");
                    auto startValue = converter(eventNode[startKey].getNumber());

                    if (!eventNode.hasKey(endKey)) CHART_LOAD_FAILED("official", std::string("missing ") + endKey + " field");
                    if (!eventNode[endKey].isNumber()) CHART_LOAD_FAILED("official", std::string(endKey) + " is not a number");
                    auto endValue = converter(eventNode[endKey].getNumber());

                    chart.animator.addEvent(line, PhiEvent {
                        .timeZone = { startTime, endTime },
                        .valueZone = { startValue, endValue },
                        .type = type,
                        .layerIndex = PhiEventLayerIndexs::LINE_DEFAULT
                    });
                }
            }
        }
    } else {
        CHART_LOAD_FAILED("official", std::string("unsupported formatVersion: ") + std::to_string(formatVersion))
    }

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadChartFromRpeJson(const Data& data) {
    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) CHART_LOAD_FAILED("rpe", std::string("failed to parse json: ") + err);
    
    PhiChart chart {};

    chart.meta.isHoldCoverAtHead = false;
    chart.meta.isZeroLengthHoldHidden = false;
    chart.meta.isHighNoteHidden = false;
    chart.meta.isRegLineAlphaNoteHidden = true;
    chart.meta.lineWidthUnit = { (ep_f64)4000 / 1350, 0.0 };
    chart.meta.lineHeightUnit = { 0.0, (ep_f64)1 / 180 };
    chart.meta.worldOrigin = { (ep_f64)-1350 / 2, (ep_f64)900 / 2 };
    chart.meta.worldViewport = { 1350, -900 };

    if (!jsonRoot.isObject()) CHART_LOAD_FAILED("rpe", "root is not an object");

    if (!jsonRoot.hasKey("META")) CHART_LOAD_FAILED("rpe", "missing META field");
    if (!jsonRoot["META"].isObject()) CHART_LOAD_FAILED("rpe", "META is not an object");
    auto& metaNode = jsonRoot["META"];

    if (!metaNode.hasKey("RPEVersion")) CHART_LOAD_FAILED("rpe", "missing RPEVersion field");
    if (!metaNode["RPEVersion"].isNumber()) CHART_LOAD_FAILED("rpe", "RPEVersion is not a number");
    chart.meta.rpeVersion = metaNode["RPEVersion"].getNumber();

    if (!metaNode.hasKey("charter")) CHART_LOAD_FAILED("rpe", "missing charter field");
    if (!metaNode["charter"].isString()) CHART_LOAD_FAILED("rpe", "charter is not a string");
    chart.meta.charter = metaNode["charter"].getString();

    if (!metaNode.hasKey("composer")) CHART_LOAD_FAILED("rpe", "missing composer field");
    if (!metaNode["composer"].isString()) CHART_LOAD_FAILED("rpe", "composer is not a string");
    chart.meta.composer = metaNode["composer"].getString();

    if (!metaNode.hasKey("name")) CHART_LOAD_FAILED("rpe", "missing name field");
    if (!metaNode["name"].isString()) CHART_LOAD_FAILED("rpe", "name is not a string");
    chart.meta.title = metaNode["name"].getString();

    if (!metaNode.hasKey("level")) CHART_LOAD_FAILED("rpe", "missing level field");
    if (!metaNode["level"].isString()) CHART_LOAD_FAILED("rpe", "level is not a string");
    chart.meta.difficulty = metaNode["level"].getString();

    if (!metaNode.hasKey("offset")) CHART_LOAD_FAILED("rpe", "missing offset field");
    if (!metaNode["offset"].isNumber()) CHART_LOAD_FAILED("rpe", "offset is not a number");
    chart.meta.offset = metaNode["offset"].getNumber() / 1000;

    auto parseTimeTuple = [](const JsonNode& node, ep_f64* dst) {
        if (!node.isArray()) return false;
        if (node.getArray().size() != 3) return false;

        const auto& arr = node.getArray();
        if (!arr[0].isNumber()) return false;
        if (!arr[1].isNumber()) return false;
        if (!arr[2].isNumber()) return false;

        ep_f64 n1 = arr[0].getNumber(),
               n2 = arr[1].getNumber(),
               n3 = arr[2].getNumber();

        *dst = n1 + n2 / n3;
        return true;
    };

    std::vector<PhiBPMEvent> sharedBpmEvents;
    
    if (!jsonRoot.hasKey("BPMList")) CHART_LOAD_FAILED("rpe", "missing BPMList field");
    if (!jsonRoot["BPMList"].isArray()) CHART_LOAD_FAILED("rpe", "BPMList is not an array");
    auto& bpmListNode = jsonRoot["BPMList"];

    for (auto& bpmEventNode : bpmListNode.getArray()) {
        if (!bpmEventNode.isObject()) CHART_LOAD_FAILED("rpe", "BPMList item is not an object");

        if (!bpmEventNode.hasKey("startTime")) CHART_LOAD_FAILED("rpe", "missing startTime field");
        ep_f64 startTime;
        if (!parseTimeTuple(bpmEventNode["startTime"], &startTime)) CHART_LOAD_FAILED("rpe", "startTime is not a valid time tuple");

        if (!bpmEventNode.hasKey("bpm")) CHART_LOAD_FAILED("rpe", "missing bpm field");
        if (!bpmEventNode["bpm"].isNumber()) CHART_LOAD_FAILED("rpe", "bpm is not a number");
        ep_f64 bpm = bpmEventNode["bpm"].getNumber();

        sharedBpmEvents.push_back({
            .time = startTime,
            .bpm = bpm
        });
    }

    PhiBPMEvent::SortBpmEvents(sharedBpmEvents);

    if (!jsonRoot.hasKey("judgeLineList")) CHART_LOAD_FAILED("rpe", "missing judgeLineList field");
    if (!jsonRoot["judgeLineList"].isArray()) CHART_LOAD_FAILED("rpe", "judgeLineList is not an array");
    auto& judgeLineListNode = jsonRoot["judgeLineList"];

    for (auto& judgeLineNode : judgeLineListNode.getArray()) {
        if (!judgeLineNode.isObject()) CHART_LOAD_FAILED("rpe", "judgeLineList item is not an object");

        auto& line = chart.lines.emplace_back();
        line.bpms = sharedBpmEvents;

        if (!judgeLineNode.hasKey("eventLayers")) CHART_LOAD_FAILED("rpe", "missing eventLayers field");
        if (!judgeLineNode["eventLayers"].isArray()) CHART_LOAD_FAILED("rpe", "eventLayers is not an array");
        auto& eventLayersNode = judgeLineNode["eventLayers"];

        ep_u64 eventLayerIndex = 0;
        // events, hasEasing, type, converter
        using EventGroupType = std::tuple<JsonNode*, bool, EnumPhiEventType, std::function<ep_f64(ep_f64)>>;

        auto progressEventGroup = [&](EventGroupType group) -> std::pair<bool, std::string> {
            auto& [eventsNode, hasEasing, type, converter] = group;
            if (!eventsNode->isArray()) return { false, "XXXEvents is not an array" };

            for (auto& eventNode : eventsNode->getArray()) {
                if (!eventNode.isObject()) return { false, "XXXEvents item is not an object" };

                if (!eventNode.hasKey("startTime")) return { false, "missing startTime field" };
                ep_f64 startTime;
                if (!parseTimeTuple(eventNode["startTime"], &startTime)) return { false, "startTime is not a valid time tuple" };

                if (!eventNode.hasKey("endTime")) return { false, "missing endTime field" };
                ep_f64 endTime;
                if (!parseTimeTuple(eventNode["endTime"], &endTime)) return { false, "endTime is not a valid time tuple" };

                ep_f64 start, end;

                if (!eventNode.hasKey("start")) return { false, "missing start field" };
                if (!eventNode.hasKey("end")) return { false, "missing end field" };

                if (type == EnumPhiEventType::Text) {
                    if (!eventNode["start"].isString()) return { false, "start is not a string" };
                    if (!eventNode["end"].isString()) return { false, "end is not a string" };

                    auto valueZone = chart.storyboardAssets.requestTextPair(eventNode["start"].getString(), eventNode["end"].getString());
                    start = valueZone.first;
                    end = valueZone.second;
                } else if (type == EnumPhiEventType::Color) {
                    if (!eventNode["start"].isArray()) return { false, "start is not an array" };
                    if (!eventNode["end"].isArray()) return { false, "end is not an array" };

                    auto& startArr = eventNode["start"].getArray();
                    auto& endArr = eventNode["end"].getArray();

                    if (startArr.size() < 3) return { false, "start array size is less than 3" };
                    if (endArr.size() < 3) return { false, "end array size is less than 3" };

                    if (!startArr[0].isNumber()) return { false, "start array item is not a number" };
                    if (!startArr[1].isNumber()) return { false, "start array item is not a number" };
                    if (!startArr[2].isNumber()) return { false, "start array item is not a number" };

                    if (!endArr[0].isNumber()) return { false, "end array item is not a number" };
                    if (!endArr[1].isNumber()) return { false, "end array item is not a number" };
                    if (!endArr[2].isNumber()) return { false, "end array item is not a number" };

                    auto startColor = Color {
                        startArr[0].getNumber() / 255,
                        startArr[1].getNumber() / 255,
                        startArr[2].getNumber() / 255,
                        1.0
                    };

                    auto endColor = Color {
                        endArr[0].getNumber() / 255,
                        endArr[1].getNumber() / 255,
                        endArr[2].getNumber() / 255,
                        1.0
                    };

                    auto valueZone = chart.storyboardAssets.requestColorPair(startColor, endColor);
                    start = valueZone.first;
                    end = valueZone.second;
                } else {
                    if (!eventNode["start"].isNumber()) return { false, "start is not a number" };
                    start = eventNode["start"].getNumber();

                    if (!eventNode["end"].isNumber()) return { false, "end is not a number" };
                    end = eventNode["end"].getNumber();
                }

                start = converter(start);
                end = converter(end);

                startTime = line.beat2sec(startTime);
                endTime = line.beat2sec(endTime);

                PhiEvent e {};
                e.timeZone = { startTime, endTime };
                e.valueZone = { start, end };
                e.type = type;
                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;

                if (hasEasing) {
                    if (!eventNode.hasKey("easingType")) return { false, "missing easingType field" };
                    if (!eventNode["easingType"].isNumber()) return { false, "easingType is not a number" };
                    e.easingFuncContext = (void*)(ep_u64)eventNode["easingType"].getNumber();

                    if (e.easingFuncContext != 0) {
                        e.easingFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing((ep_u64)ctx, p); };
                    }

                    if (eventNode.hasKey("easingLeft")) {
                        if (!eventNode["easingLeft"].isNumber()) return { false, "easingLeft is not a number" };
                        e.easingZone.x = eventNode["easingLeft"].getNumber();
                    }

                    if (eventNode.hasKey("easingRight")) {
                        if (!eventNode["easingRight"].isNumber()) return { false, "easingRight is not a number" };
                        e.easingZone.y = eventNode["easingRight"].getNumber();
                    }
                }

                chart.animator.addEvent(line, e);
            }

            return { true, "" };
        };

        for (auto& eventLayerNode : eventLayersNode.getArray()) {
            if (eventLayerNode.isNull()) continue;
            if (!eventLayerNode.isObject()) CHART_LOAD_FAILED("rpe", "eventLayers item is not an object");

            std::vector<EventGroupType> groups;

            if (eventLayerNode.hasKey("alphaEvents")) groups.push_back({ &eventLayerNode["alphaEvents"], true, EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v / 255; } });
            if (eventLayerNode.hasKey("moveXEvents")) groups.push_back({ &eventLayerNode["moveXEvents"], true, EnumPhiEventType::PositionX, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("moveYEvents")) groups.push_back({ &eventLayerNode["moveYEvents"], true, EnumPhiEventType::PositionY, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("rotateEvents")) groups.push_back({ &eventLayerNode["rotateEvents"], true, EnumPhiEventType::SelfRotation, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("speedEvents")) groups.push_back({ &eventLayerNode["speedEvents"], false, EnumPhiEventType::Speed, [](ep_f64 v) { return v * 120 / 900; } });

            for (auto& group : groups) {
                auto [success, msg] = progressEventGroup(group);
                if (!success) CHART_LOAD_FAILED("rpe", msg);
            }

            eventLayerIndex++;
        }

        if (judgeLineNode.hasKey("extended")) {
            auto& extendedNode = judgeLineNode["extended"];
            if (!extendedNode.isObject()) CHART_LOAD_FAILED("rpe", "extended is not an object");

            std::vector<EventGroupType> groups;

            if (extendedNode.hasKey("textEvents")) groups.push_back({ &extendedNode["textEvents"], true, EnumPhiEventType::Text, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("scaleXEvents")) groups.push_back({ &extendedNode["scaleXEvents"], true, EnumPhiEventType::ScaleX, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("scaleYEvents")) groups.push_back({ &extendedNode["scaleYEvents"], true, EnumPhiEventType::ScaleY, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("colorEvents")) groups.push_back({ &extendedNode["colorEvents"], true, EnumPhiEventType::Color, [](ep_f64 v) { return v; } });

            for (auto& group : groups) {
                auto [success, msg] = progressEventGroup(group);
                if (!success) CHART_LOAD_FAILED("rpe", msg);
            }

            eventLayerIndex++;
        }

        if (judgeLineNode.hasKey("notes")) {
            auto& notesNode = judgeLineNode["notes"];
            if (!notesNode.isArray()) CHART_LOAD_FAILED("rpe", "notes is not an array");

            for (auto& noteNode : notesNode.getArray()) {
                if (!noteNode.isObject()) CHART_LOAD_FAILED("rpe", "notes item is not an object");

                auto& note = line.notes.emplace_back();

                if (!noteNode.hasKey("startTime")) CHART_LOAD_FAILED("rpe", "missing startTime field");
                ep_f64 startTime;
                if (!parseTimeTuple(noteNode["startTime"], &startTime)) CHART_LOAD_FAILED("rpe", "startTime is not a valid time tuple");

                if (!noteNode.hasKey("endTime")) CHART_LOAD_FAILED("rpe", "missing endTime field");
                ep_f64 endTime;
                if (!parseTimeTuple(noteNode["endTime"], &endTime)) CHART_LOAD_FAILED("rpe", "endTime is not a valid time tuple");

                startTime = line.beat2sec(startTime);
                endTime = line.beat2sec(endTime);

                if (!noteNode.hasKey("above")) CHART_LOAD_FAILED("rpe", "missing above field");
                ep_bool isAbove;
                if (noteNode["above"].isBool()) isAbove = noteNode["above"].getBool();
                else if (noteNode["above"].isNumber()) isAbove = noteNode["above"].getNumber() == 1;
                else CHART_LOAD_FAILED("rpe", "above is not a boolean or number");

                if (!noteNode.hasKey("type")) CHART_LOAD_FAILED("rpe", "missing type field");
                if (!noteNode["type"].isNumber()) CHART_LOAD_FAILED("rpe", "type is not a number");
                auto type = PhiNoteTypeHelper::FromRPE(noteNode["type"].getNumber());

                if (!noteNode.hasKey("speed")) CHART_LOAD_FAILED("rpe", "missing speed field");
                if (!noteNode["speed"].isNumber()) CHART_LOAD_FAILED("rpe", "speed is not a number");
                ep_f64 speed = noteNode["speed"].getNumber();

                if (!noteNode.hasKey("isFake")) CHART_LOAD_FAILED("rpe", "missing isFake field");
                ep_bool isFake;
                if (noteNode["isFake"].isBool()) isFake = noteNode["isFake"].getBool();
                else if (noteNode["isFake"].isNumber()) isFake = noteNode["isFake"].getNumber() == 1;
                else CHART_LOAD_FAILED("rpe", "isFake is not a boolean or number");

                if (!noteNode.hasKey("positionX")) CHART_LOAD_FAILED("rpe", "missing positionX field");
                if (!noteNode["positionX"].isNumber()) CHART_LOAD_FAILED("rpe", "positionX is not a number");
                ep_f64 positionX = noteNode["positionX"].getNumber() / 1350;
                
                if (noteNode.hasKey("yOffset")) {
                    if (!noteNode["yOffset"].isNumber()) CHART_LOAD_FAILED("rpe", "yOffset is not a number");
                    auto yOffset = noteNode["yOffset"].getNumber() / 900 * speed;
                    if (!isAbove) yOffset *= -1;
                    
                    if (yOffset != 0.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { yOffset, yOffset },
                            .type = EnumPhiEventType::PositionY,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("visibleTime")) {
                    if (!noteNode["visibleTime"].isNumber()) CHART_LOAD_FAILED("rpe", "visibleTime is not a number");
                    auto visibleTime = noteNode["visibleTime"].getNumber();

                    if (visibleTime < 999999.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = { -INF_TIME, startTime - visibleTime },
                            .valueZone = { 0.0, 0.0 },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = { startTime - visibleTime, INF_TIME },
                            .valueZone = { 1.0, 1.0 },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("size")) {
                    if (!noteNode["size"].isNumber()) CHART_LOAD_FAILED("rpe", "size is not a number");
                    auto size = noteNode["size"].getNumber();

                    if (size != 1.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { size, size },
                            .type = EnumPhiEventType::ScaleX,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("alpha")) {
                    if (!noteNode["alpha"].isNumber()) CHART_LOAD_FAILED("rpe", "alpha is not a number");
                    auto alpha = noteNode["alpha"].getNumber() / 255;

                    if (alpha != 1.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { alpha, alpha },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });
                    }
                }

                note.type = type;
                note.time = startTime;
                note.holdTime = endTime - startTime;
                note.isFake = isFake;

                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { positionX, positionX },
                    .type = EnumPhiEventType::PositionX,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                });

                if (speed != 1.0) {
                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { speed, speed },
                        .type = EnumPhiEventType::SpeedCoefficient,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });
                }

                if (!isAbove) {
                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { -1.0, -1.0 },
                        .type = EnumPhiEventType::SpeedCoefficient,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                    });

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { 180.0, 180.0 },
                        .type = EnumPhiEventType::SelfRotation,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                    });

                    note.reverseCover();
                }
            }
        }

        if (judgeLineNode.hasKey("attachUI")) {
            if (!judgeLineNode["attachUI"].isString()) CHART_LOAD_FAILED("rpe", "attachUI is not a string");
            line.attachUI = PhiLineAttachUIHelper::FromString(judgeLineNode["attachUI"].getString());
        }

        if (judgeLineNode.hasKey("Texture")) {
            if (!judgeLineNode["Texture"].isString()) CHART_LOAD_FAILED("rpe", "Texture is not a string");
            auto textureName = judgeLineNode["Texture"].getString();

            if (textureName != "line.png") {
                line.textureName = textureName;
            }
        }

        if (judgeLineNode.hasKey("father")) {
            if (!judgeLineNode["father"].isNumber()) CHART_LOAD_FAILED("rpe", "father is not a number");
            ep_i64 fatherLineIndex = judgeLineNode["father"].getNumber();
            if (fatherLineIndex >= 0) {
                line.fatherLineIndex = fatherLineIndex;
            }
        }

        bool enableCover = true;
        if (judgeLineNode.hasKey("isCover")) {
            if (judgeLineNode["isCover"].isNumber()) enableCover = judgeLineNode["isCover"].getNumber() == 1;
            else if (judgeLineNode["isCover"].isBool()) enableCover = judgeLineNode["isCover"].getBool();
            else CHART_LOAD_FAILED("rpe", "isCover is not a boolean or number");
        }
        line.enableCover = enableCover;

        if (judgeLineNode.hasKey("zOrder")) {
            if (!judgeLineNode["zOrder"].isNumber()) CHART_LOAD_FAILED("rpe", "zOrder is not a number");
            line.zOrder = judgeLineNode["zOrder"].getNumber();
        }

        if (judgeLineNode.hasKey("anchor")) {
            if (!judgeLineNode["anchor"].isArray()) CHART_LOAD_FAILED("rpe", "anchor is not an array");

            auto& anchorArr = judgeLineNode["anchor"].getArray();
            if (anchorArr.size() < 2) CHART_LOAD_FAILED("rpe", "anchor array size is less than 2");

            if (!anchorArr[0].isNumber() || !anchorArr[1].isNumber()) CHART_LOAD_FAILED("rpe", "anchor array element is not a number");
            line.anchor = { anchorArr[0].getNumber(), anchorArr[1].getNumber() };
        }
    }

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadChartFromPec(const Data& data) {
    TokenStringReader reader(std::string((char*)data.data.data(), data.data.size()));
    std::string token;

    auto readNumber = [&](ep_f64* dst) {
        if (!reader.nextToken(token)) return false;
        char* end;
        *dst = std::strtod(token.c_str(), &end);
        return end != token.c_str();
    };

    auto readBool = [&](ep_bool* dst) {
        ep_f64 num;
        if (!readNumber(&num)) return false;
        *dst = num == 1.0;
        return true;
    };

    PhiChart chart {};
    chart.meta.isHoldCoverAtHead = true;
    chart.meta.isZeroLengthHoldHidden = false;
    chart.meta.isHighNoteHidden = false;
    chart.meta.isRegLineAlphaNoteHidden = true;
    chart.meta.lineWidthUnit = { (ep_f64)4000 / 1350, 0.0 };
    chart.meta.lineHeightUnit = { 0.0, (ep_f64)1 / 180 };
    chart.meta.worldOrigin = { (ep_f64)-1350 / 2, (ep_f64)900 / 2 };
    chart.meta.worldViewport = { 1350, -900 };

    ep_f64 offset;
    if (!readNumber(&offset)) CHART_LOAD_FAILED("pec", "failed to read offset");
    chart.meta.offset = (offset - 150.0) / 1000.0;

    struct Commands {
        struct Bpm {
            ep_f64 startTime, bpm;
        };

        struct Note {
            ep_i64 lineIndex;
            PhiNote note;
            ep_bool isAbove;
            ep_f64 speed = 1.0, size = 1.0, positionX;
        };

        struct Event {
            Vec2 timeZone;
            ep_f64 value;
            ep_bool useFront = false;
            ep_u64 easingType = 1;
        };
    };

    std::vector<Commands::Bpm> bpmCommands;
    std::vector<Commands::Note> noteCommands;
    std::unordered_map<ep_i64, std::unordered_map<EnumPhiEventType, std::vector<Commands::Event>>> eventCommands;

    while (reader.nextToken(token)) {
        if (token == "bp") {
            ep_f64 startTime, bpm;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (bp)");
            if (!readNumber(&bpm)) CHART_LOAD_FAILED("pec", "failed to read bpm (bp)");

            bpmCommands.push_back({
                .startTime = startTime,
                .bpm = bpm
            });
        } else if (token == "n1" || token == "n2" || token == "n3" || token == "n4") {
            auto type = PhiNoteTypeHelper::FromPEC(token);

            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (nx)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (nx)");
            if (type == EnumPhiNoteType::Hold) {
                if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (nx)");
            } else endTime = startTime;

            ep_f64 positionX;
            ep_bool isAbove, isFake;

            if (!readNumber(&positionX)) CHART_LOAD_FAILED("pec", "failed to read positionX (nx)");
            if (!readBool(&isAbove)) CHART_LOAD_FAILED("pec", "failed to read isAbove (nx)");
            if (!readBool(&isFake)) CHART_LOAD_FAILED("pec", "failed to read isFake (nx)");

            noteCommands.push_back(Commands::Note {
                .lineIndex = (ep_i64)lineIndex,
                .note = PhiNote {
                    .type = type,
                    .time = startTime,
                    .holdTime = endTime - startTime,
                    .isFake = isFake
                },
                .isAbove = isAbove,
                .positionX = positionX
            });
        } else if (token == "#") {
            ep_f64 speed;
            if (!readNumber(&speed)) CHART_LOAD_FAILED("pec", "failed to read speed (#)");
            if (!noteCommands.empty()) noteCommands.back().speed = speed;
        } else if (token == "&") {
            ep_f64 size;
            if (!readNumber(&size)) CHART_LOAD_FAILED("pec", "failed to read size (&)");
            if (!noteCommands.empty()) noteCommands.back().size = size;
        } else if (token == "cp") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cp)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cp)");

            ep_f64 x, y;
            if (!readNumber(&x)) CHART_LOAD_FAILED("pec", "failed to read x (cp)");
            if (!readNumber(&y)) CHART_LOAD_FAILED("pec", "failed to read y (cp)");

            eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                .timeZone = { time, time }, .value = x
            });

            eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                .timeZone = { time, time }, .value = y
            });
        } else if (token == "cd") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cd)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cd)");

            ep_f64 r;
            if (!readNumber(&r)) CHART_LOAD_FAILED("pec", "failed to read y (cd)");

            eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                .timeZone = { time, time }, .value = r
            });
        } else if (token == "ca") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (ca)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (ca)");

            ep_f64 a;
            if (!readNumber(&a)) CHART_LOAD_FAILED("pec", "failed to read a (ca)");

            eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                .timeZone = { time, time }, .value = a
            });
        } else if (token == "cv") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cv)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cv)");

            ep_f64 v;
            if (!readNumber(&v)) CHART_LOAD_FAILED("pec", "failed to read v (cv)");

            eventCommands[lineIndex][EnumPhiEventType::Speed].push_back(Commands::Event {
                .timeZone = { time, time }, .value = v
            });
        } else if (token == "cm") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cm)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cm)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cm)");

            ep_f64 x, y;
            if (!readNumber(&x)) CHART_LOAD_FAILED("pec", "failed to read x (cm)");
            if (!readNumber(&y)) CHART_LOAD_FAILED("pec", "failed to read y (cm)");

            ep_f64 easingType;
            if (!readNumber(&easingType)) CHART_LOAD_FAILED("pec", "failed to read easingType (cm)");

            eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = x,
                .useFront = true, .easingType = (ep_u64)easingType
            });

            eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = y,
                .useFront = true, .easingType = (ep_u64)easingType
            });
        } else if (token == "cr") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cr)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cr)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cr)");

            ep_f64 r;
            if (!readNumber(&r)) CHART_LOAD_FAILED("pec", "failed to read r (cr)");

            ep_f64 easingType;
            if (!readNumber(&easingType)) CHART_LOAD_FAILED("pec", "failed to read easingType (cr)");

            eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = r,
                .useFront = true, .easingType = (ep_u64)easingType
            });
        } else if (token == "cf") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cf)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cf)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cf)");

            ep_f64 a;
            if (!readNumber(&a)) CHART_LOAD_FAILED("pec", "failed to read a (cf)");

            eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = a,
                .useFront = true
            });
        }
    }

    std::sort(bpmCommands.begin(), bpmCommands.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
    std::vector<PhiBPMEvent> sharedBpmEvents;
    for (auto& cmd : bpmCommands) {
        sharedBpmEvents.push_back(PhiBPMEvent {
            .time = cmd.startTime,
            .bpm = cmd.bpm
        });
    }

    std::unordered_map<ep_i64, ep_u64> lineIndexMap;
    auto getLineByIndex = [&](ep_i64 index) -> PhiLine& {
        if (lineIndexMap.contains(index)) return chart.lines[lineIndexMap[index]];
        PhiLine line {};
        line.bpms = sharedBpmEvents;
        chart.lines.push_back(line);
        lineIndexMap[index] = chart.lines.size() - 1;
        return chart.lines.back();
    };

    auto toSeconds = [&](ep_i64 lineIndex, ep_f64 beatTime) {
        auto& line = getLineByIndex(lineIndex);
        return line.beat2sec(beatTime);
    };

    for (auto& cmd : noteCommands) {
        auto& line = getLineByIndex(cmd.lineIndex);
        auto& note = line.notes.emplace_back(std::move(cmd.note));

        auto time = toSeconds(cmd.lineIndex, note.time);
        auto holdTime = toSeconds(cmd.lineIndex, note.time + note.holdTime) - time;
        note.time = time;
        note.holdTime = holdTime;

        if (!cmd.isAbove) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { -1.0, -1.0 },
                .type = EnumPhiEventType::SpeedCoefficient,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
            });

            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { 180.0, 180.0 },
                .type = EnumPhiEventType::SelfRotation,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
            });

            note.reverseCover();
        }

        if (cmd.positionX != 0.0) {
            auto value = cmd.positionX / 2048.0;

            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { value, value },
                .type = EnumPhiEventType::PositionX,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }

        if (cmd.speed != 1.0) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { cmd.speed, cmd.speed },
                .type = EnumPhiEventType::SpeedCoefficient,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }

        if (cmd.size != 1.0) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { cmd.size, cmd.size },
                .type = EnumPhiEventType::ScaleX,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }
    }

    for (auto& [lineIndex, events] : eventCommands) {
        for (auto& [type, typedEvents] : events) {
            std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                if (a.timeZone.x != b.timeZone.x) return a.timeZone.x < b.timeZone.x;
                if (a.timeZone.y != b.timeZone.y) return a.timeZone.y < b.timeZone.y;
                return b.useFront;
            });

            for (auto& cmd : typedEvents) {
                cmd.timeZone.x = toSeconds(lineIndex, cmd.timeZone.x);
                cmd.timeZone.y = toSeconds(lineIndex, cmd.timeZone.y);

                if (type == EnumPhiEventType::PositionX) cmd.value = (cmd.value / 2048.0 - 0.5) * 1350.0;
                else if (type == EnumPhiEventType::PositionY) cmd.value = (cmd.value / 1400.0 - 0.5) * 900.0;
                else if (type == EnumPhiEventType::Speed) cmd.value = cmd.value / 1400.0 * 900.0 * 120.0 / 900.0;
                else if (type == EnumPhiEventType::AdditiveAlpha) cmd.value /= 255.0;
            }

            for (ep_u64 i = 0; i < typedEvents.size(); i++) {
                auto& cmd = typedEvents[i];
                auto& line = getLineByIndex(lineIndex);

                ep_f64 startValue;
                if (cmd.useFront && i - 1 >= 0) startValue = typedEvents[i - 1].value;
                else startValue = cmd.value;

                if (i < typedEvents.size() - 1) {
                    auto& next = typedEvents[i + 1];
                    if (cmd.timeZone.isZeroZone() && next.timeZone.x == cmd.timeZone.y) continue;
                }
                
                if (cmd.timeZone.isZeroZone()) cmd.timeZone.y += 1.0;

                PhiEvent e {};
                e.timeZone = cmd.timeZone;
                e.valueZone = { startValue, cmd.value };
                e.type = type;
                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT;

                if (cmd.easingType) {
                    e.easingFuncContext = (void*)cmd.easingType;
                    e.easingFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing((ep_u64)ctx, p); };
                }

                chart.animator.addEvent(line, e);
            }
        }
    }

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadChartFromData(const Data& data) {
    PhiChartLoadResult result{};
    result.success = false;

    #define TRY_LOAD_FUNC(func) \
        { \
            auto res = func(data); \
            if (res.success) return res; \
            result.errors.insert(result.errors.end(), res.errors.begin(), res.errors.end()); \
        }
    
    TRY_LOAD_FUNC(loadChartFromOfficialJson);
    TRY_LOAD_FUNC(loadChartFromRpeJson);
    TRY_LOAD_FUNC(loadChartFromPec);

    return result;

    #undef TRY_LOAD_FUNC
}

struct StoryboardHelpers {
    static void attachTextureLoader(
        PhiStoryboardAssets& assets,
        const std::string& path,
        const std::function<std::optional<std::pair<ep_u64, Vec2>>(std::string)>& loader,
        const std::function<void(ep_u64)>& destroyer
    ) {
        assets.clearTextures();

        assets.textureLoader = [=](std::string name) {
            std::filesystem::path texturePath(path);
            texturePath /= std::filesystem::path(name).lexically_normal();
            return loader(texturePath.string());
        };

        assets.textureDestroyer = destroyer;
    }
};

void stripString(std::string& str) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    auto tail = std::ranges::find_if(str | std::views::reverse, not_space);
    str.erase(tail.base(), str.end());
    auto head = std::ranges::find_if(str, not_space);
    str.erase(str.begin(), head);
}

void splitStringToLines(const std::string& str, std::vector<std::string>& lines) {
    for (auto&& subrange : str | std::views::split('\n')) {
        lines.emplace_back(subrange.begin(), subrange.end());
    }
}

struct ParsedRPEChartInfo {
    std::string name;
    std::string path;
    std::string song;
    std::string picture;
    std::string chart;
    std::string level;
    std::string composer;
    std::string lastEditTime;
    std::string length;
    std::string editTime;
    std::string group;
};

std::vector<ParsedRPEChartInfo> parseRPEChartInfo(const Data& data) {
    std::vector<ParsedRPEChartInfo> infos;

    auto str = data.toString();
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());

    std::vector<std::string> lines;
    splitStringToLines(str, lines);

    ParsedRPEChartInfo info {};
    ep_u64 vaildLineCount = 0;

    for (auto& line : lines) {
        stripString(line);

        if (line.empty()) continue;
        if (line[0] == '#') {
            if (vaildLineCount) {
                infos.push_back(info);
                info = {};
                vaildLineCount = 0;
            }
            continue;
        }

        auto split = line.find(": ");
        if (split == std::string::npos) continue;

        auto key = line.substr(0, split);
        auto value = line.substr(split + 2);

        if (key == "Name") info.name = value;
        else if (key == "Path") info.path = value;
        else if (key == "Song") info.song = value;
        else if (key == "Picture") info.picture = value;
        else if (key == "Chart") info.chart = value;
        else if (key == "Level") info.level = value;
        else if (key == "Composer") info.composer = value;
        else if (key == "LastEditTime") info.lastEditTime = value;
        else if (key == "Length") info.length = value;
        else if (key == "EditTime") info.editTime = value;
        else if (key == "Group") info.group = value;
        vaildLineCount++;
    }

    if (vaildLineCount) {
        infos.push_back(info);
    }

    return infos;
}

struct PhiLineAttachUIData {
    Vec2 position, scale = { 1.0, 1.0 };
    ep_f64 rotation;
    Color color = { 1.0, 1.0, 1.0, 1.0 };
};

struct CalculateFrameConfig {
    struct NoteTextureInfo {
        struct Item {
            Vec2 textureSize;
            Vec2 cutPadding;
            Vec2 scaling = { 1.0, 1.0 };
        };

        Item single;
        Item simul;
    };

    Vec2 screenSize;
    Vec2 backgroundTextureSize;
    std::unordered_map<EnumPhiNoteType, NoteTextureInfo> noteTextureInfos;
    ep_f64 songLength;
    ep_f64 maxNoteBodyLength = 8192.0;
};

struct CalculatedFrame {
    struct CalculatedNote {
        Vec2 position;
        ep_f64 rotation;
        ep_f64 width, head, body, tail;
        EnumPhiNoteType type;
        ep_bool isSimul;
        Color color;
    };

    struct CalculatedText {
        std::string text;
        Vec2 position, scale;
        EnumTextAlign align; EnumTextBaseline baseline;
        ep_f64 fontSize, rotation;
        Color color;
    };

    struct CalculatedStoryboardTexture {
        ep_u64 texture;
        Vec2 position, size, scale, anchor;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedHitEffectTexture {
        Vec2 position, size;
        ep_f64 progress;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedHitEffectParticle {
        Vec2 position, size;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedPoly {
        Vec2 p1, p2, p3, p4;
        Color color;
    };

    using CalculatedObject = std::variant<
        CalculatedNote,
        CalculatedText,
        CalculatedStoryboardTexture,
        CalculatedHitEffectTexture,
        CalculatedHitEffectParticle,
        CalculatedPoly
    >;

    ep_f64 backgroundImageBlurRadius;
    Rect unsafeBackgroundRect, backgroundRect;
    ep_f64 unsafeAreaDim, backgroundDim;
    Rect objectsClipRect;
    std::vector<CalculatedObject> objects;
    std::vector<std::pair<EnumPhiNoteType, ep_f64>> hitsounds;

    void addPoly(
        const Vec2& point,
        const Vec2& size,
        const Color& color,
        const Transform2D& transform = Transform2D()
    ) {
        objects.push_back(CalculatedPoly {
            .p1 = transform.transformPoint(point),
            .p2 = transform.transformPoint(point + Vec2 { size.x, 0.0 }),
            .p3 = transform.transformPoint(point + size),
            .p4 = transform.transformPoint(point + Vec2 { 0.0, size.y }),
            .color = color
        });
    }

    struct Cache {
        std::unordered_map<EnumPhiLineAttachUI, PhiLineAttachUIData> attachUIDatas;
        std::unordered_map<EnumPhiNoteType, std::vector<CalculatedObject>> noteObjects;

        void clear() {
            attachUIDatas.clear();
            for (auto& [_, objects] : noteObjects) objects.clear();
        }
    };

    Cache cache;
};

void calculateFrame(
    PhiChart& chart, ep_f64 time,
    const CalculateFrameConfig& config,
    CalculatedFrame& frame
) {
    frame.objects.clear();
    frame.hitsounds.clear();
    frame.cache.clear();

    auto calcCoveredOrContainRect = [](const Rect& dst, const Vec2& size, bool isCovered) {
        ep_f64 dst_ratio = dst.w / dst.h;
        ep_f64 src_ratio = size.x / size.y;

        ep_f64 w, h;
        if (isCovered ? (src_ratio < dst_ratio) : (src_ratio > dst_ratio)) {
            w = dst.w;
            h = dst.w / src_ratio;
        } else {
            w = dst.h * src_ratio;
            h = dst.h;
        }

        return Rect {
            (dst.x + dst.w / 2 - w / 2),
            (dst.y + dst.h / 2 - h / 2),
            w, h
        };
    };

    ep_f64 screenRatio = config.screenSize.x / config.screenSize.y;
    Rect safeArea = screenRatio > chart.meta.maxViewRatio ? calcCoveredOrContainRect(
        { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
        { chart.meta.maxViewRatio, 1.0 }, false
    ) : Rect { 0.0, 0.0, config.screenSize.x, config.screenSize.y };

    auto safeAreaPosition = safeArea.position();
    auto safeAreaSize = safeArea.size();
    auto toScreen = [&](Vec2 pos) { return pos + safeAreaPosition; };

    frame.backgroundImageBlurRadius = config.backgroundTextureSize.sum() * chart.options.backgroundTextureBlurRadius;

    frame.unsafeBackgroundRect = calcCoveredOrContainRect(
        { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
        config.backgroundTextureSize, true
    );
    frame.unsafeAreaDim = chart.options.unsafeBackgroundDim;

    frame.backgroundRect = calcCoveredOrContainRect(safeArea, config.backgroundTextureSize, true);
    frame.backgroundDim = chart.options.backgroundDim;

    frame.objectsClipRect = safeArea;

    auto processAttachUIText = [&](CalculatedFrame::CalculatedText rawText, EnumPhiLineAttachUI attachUIType) {
        auto& data = frame.cache.attachUIDatas[attachUIType];
        rawText.position += data.position;
        rawText.scale *= data.scale;
        rawText.rotation += data.rotation;
        rawText.color *= data.color;
        return rawText;
    };

    time -= chart.meta.offset;
    
    auto lineWidth = (chart.meta.lineWidthUnit * safeAreaSize).sum();
    auto lineHeight = (chart.meta.lineHeightUnit * safeAreaSize).sum();
    auto standardNoteWidth = safeAreaSize.x * 0.1234375;

    struct NoteTextureSizeInfo {
        ep_f64 width;
        ep_f64 head, body, tail;

        void scale(const Vec2& v) {
            width *= v.x;
            head *= v.y;
            body *= v.y;
            tail *= v.y;
        }

        ep_f64 getHeadHalfDiagonal() const {
            return Vec2 { width, head }.length() / 2;
        }
    };

    auto getNoteTextureSizeInfo = [&](EnumPhiNoteType type, bool isSimul, bool hideHead) {
        const auto& texInfo = (
            isSimul
            ? config.noteTextureInfos.at(type).simul
            : config.noteTextureInfos.at(type).single
        );

        auto scaling = texInfo.scaling * chart.options.noteScaling;

        auto width = standardNoteWidth * scaling.x;
        auto totalHeight = width / texInfo.textureSize.x * texInfo.textureSize.y * scaling.y;
        auto cutPadding = texInfo.cutPadding / texInfo.textureSize.y;
        auto head = hideHead ? 0.0 : cutPadding.x * totalHeight;
        auto tail = cutPadding.y * totalHeight;

        return NoteTextureSizeInfo {
            .width = width,
            .head = head,
            .tail = tail
        };
    };

    ep_f64 maxHalfNoteHeadDiagonal = 0.0;

    for (auto& [type, _] : config.noteTextureInfos) {
        maxHalfNoteHeadDiagonal = std::max({
            maxHalfNoteHeadDiagonal,
            getNoteTextureSizeInfo(type, false, false).getHeadHalfDiagonal(),
            getNoteTextureSizeInfo(type, true, false).getHeadHalfDiagonal()
        });
    }

    chart.state.timeUpdated(time);

    for (auto& lineIndex : chart.zOrderSortedLines) {
        auto& line = chart.lines[lineIndex];

        auto linePosition = chart.getLinePosition(time, line, safeAreaSize);
        auto lineScreenPosition = toScreen(linePosition);
        auto linePositionRelOrigin = chart.getLinePositionRelOrigin(time, line, safeAreaSize);
        auto lineRotation = chart.animator.get(line, time, EnumPhiEventType::SelfRotation);
        auto lineAlpha = chart.animator.get_alpha(line, time, 0.0);
        auto lineTextIndex = chart.animator.get(line, time, EnumPhiEventType::Text);
        auto lineText = chart.storyboardAssets.getText(lineTextIndex);
        auto lineColorIndex = chart.animator.get(line, time, EnumPhiEventType::Color);
        auto lineColor = chart.storyboardAssets.getColor(
            lineColorIndex,
            (line.attachUI.has_value() || lineText.has_value() || line.textureName.has_value())
                ? Color::White()
                : chart.options.lineDefaultColor
        );
        auto lineScale = Vec2 {
            chart.animator.get(line, time, EnumPhiEventType::ScaleX),
            chart.animator.get(line, time, EnumPhiEventType::ScaleY)
        };

        if (line.attachUI.has_value()) {
            frame.cache.attachUIDatas[line.attachUI.value()] = {
                .position = linePositionRelOrigin,
                .scale = lineScale,
                .rotation = lineRotation,
                .color = lineColor.applyAlpha(lineAlpha)
            };
        } else {
            if (lineAlpha * lineColor.a > 0) {
                if (lineText.has_value()) {
                    frame.objects.push_back(CalculatedFrame::CalculatedText {
                        .text = lineText.value(),
                        .position = lineScreenPosition,
                        .scale = lineScale,
                        .align = EnumTextAlign::Center,
                        .baseline = EnumTextBaseline::Middle,
                        .fontSize = (chart.options.storyboardTextBaseSize * safeAreaSize).sum(),
                        .rotation = lineRotation,
                        .color = lineColor.applyAlpha(lineAlpha)
                    });
                } else {
                    if (line.textureName.has_value()) {
                        if (line.texture.has_value()) {
                            auto texture = chart.storyboardAssets.getTexture(line.texture.value());
                            ep_f64 textureWidth, textureHeight;

                            if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutWidth) {
                                textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                                textureHeight = textureWidth / texture.second.x * texture.second.y;
                            } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutHeight) {
                                textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                                textureWidth = textureHeight / texture.second.y * texture.second.x;
                            } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::Stretch) {
                                textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                                textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                            } else textureWidth = textureHeight = 0;

                            textureWidth *= chart.options.storyboardTextureScaling.x;
                            textureHeight *= chart.options.storyboardTextureScaling.y;

                            frame.objects.push_back(CalculatedFrame::CalculatedStoryboardTexture {
                                .texture = texture.first,
                                .position = lineScreenPosition,
                                .size = Vec2 { textureWidth, textureHeight },
                                .scale = lineScale,
                                .anchor = line.anchor,
                                .rotation = lineRotation,
                                .color = lineColor.applyAlpha(lineAlpha)
                            });
                        }
                    } else {
                        frame.addPoly(
                            Vec2 { -lineWidth, -lineHeight } * line.anchor * lineScale,
                            Vec2 { lineWidth, lineHeight } * lineScale,
                            lineColor.applyAlpha(lineAlpha),
                            Transform2D()
                                .translate(lineScreenPosition)
                                .rotateDegress(lineRotation)
                        );
                    }
                }
            }
        }

        for (auto& noteGroup : line.noteGroups) {
            noteGroup.state.timeUpdated(time);

            for (ep_u64 note_ii = noteGroup.state.firstNoteIndex; note_ii < noteGroup.indexs.size(); note_ii++) {
                auto note_i = noteGroup.indexs[note_ii];
                auto& note = line.notes[note_i];
                note.state.timeUpdated(time);

                auto frameInfo = chart.getNoteFrameInfo(line, note, time, safeAreaSize);

                if (frameInfo.isArrived && note.state.onPlayHitsound()) {
                    if (!note.isFake) {
                        frame.hitsounds.push_back({ note.type, time - note.time });
                    }
                }

                if (note.time + note.holdTime < time) {
                    noteGroup.state.passedNoteIndex(note_ii);
                    continue;
                }

                auto noteScreenHeadPosition = toScreen(frameInfo.headPosition);
                auto noteScreenTailPosition = toScreen(frameInfo.tailPosition);

                auto sizeInfo = getNoteTextureSizeInfo(note.type, note.isSimul, frameInfo.isArrived);
                sizeInfo.body = std::min(config.maxNoteBodyLength, (noteScreenHeadPosition - noteScreenTailPosition).length());
                sizeInfo.scale(frameInfo.scale);

                Transform2D noteTransform;
                noteTransform.translate(noteScreenHeadPosition);
                noteTransform.rotateDegress(frameInfo.textureRotation);
                noteTransform.scale(1.0, -1.0);

                Vec2 noteQuad[4] = {
                    noteTransform.transformPoint({ -sizeInfo.width / 2, -sizeInfo.head }),
                    noteTransform.transformPoint({ sizeInfo.width / 2, -sizeInfo.head }),
                    noteTransform.transformPoint({ sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail }),
                    noteTransform.transformPoint({ -sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail })
                };

                // 只 hide 不用考虑 maxHalfNoteHeadDiagonal, 但是这里 break 优化也要用
                auto extendedSafeArea = safeArea.extend(maxHalfNoteHeadDiagonal);
                ep_bool noteInsideScreen = quadStrictlyIntersectRect(noteQuad, extendedSafeArea);

                if (noteInsideScreen) {
                    if (frameInfo.isVisible) {
                        frame.cache.noteObjects[note.type].push_back(CalculatedFrame::CalculatedNote {
                            .position = noteScreenHeadPosition,
                            .rotation = frameInfo.textureRotation,
                            .width = sizeInfo.width,
                            .head = sizeInfo.head,
                            .body = sizeInfo.body,
                            .tail = sizeInfo.tail,
                            .type = note.type,
                            .isSimul = note.isSimul,
                            .color = frameInfo.color
                        });
                    }
                } else {
                    if (chart.options.enableNoteOffScreenBreakOptimization && noteGroup.breakable) {
                        if (lineIsLeavingScreen(
                            noteScreenHeadPosition,
                            frameInfo.speedVectorRotation,
                            extendedSafeArea
                        ) && lineIsLeavingScreen(
                            noteScreenHeadPosition,
                            frameInfo.textureRotation,
                            extendedSafeArea
                        )) break;
                    }
                }
            }
        }
    }

    for (const auto type : {
        EnumPhiNoteType::Hold,
        EnumPhiNoteType::Drag,
        EnumPhiNoteType::Tap,
        EnumPhiNoteType::Flick
    }) {
        auto& noteObjects = frame.cache.noteObjects[type];
        frame.objects.insert(frame.objects.end(), noteObjects.begin(), noteObjects.end());
    }

    const ep_f64 hitEffectTextureSize = standardNoteWidth * chart.options.hitEffectTextureScaling;

    for (ep_u64 i = chart.state.firstHitEffectIndex; i < chart.hitEffects.size(); i++) {
        auto& hitEffect = chart.hitEffects[i];
        if (hitEffect.time > time) break;

        auto& line = chart.lines[hitEffect.lineIndex];
        auto& note = line.notes[hitEffect.noteIndex];

        auto info = chart.getNoteFrameInfo(line, note, hitEffect.time, safeAreaSize);
        auto endTime = hitEffect.time + std::max(chart.options.hitEffectDuration, hitEffect.particles.size() ? (hitEffect.particles.back().dt + chart.options.hitEffectDuration) : 0.0);

        if (endTime < time) {
            chart.state.passedHitEffectIndex(i);
            continue;
        }

        auto effectScreenPosition = toScreen(info.headPosition);
        auto progress = (time - hitEffect.time) / chart.options.hitEffectDuration;

        if (progress <= 1.0) {
            frame.objects.push_back(CalculatedFrame::CalculatedHitEffectTexture {
                .position = effectScreenPosition,
                .size = { hitEffectTextureSize, hitEffectTextureSize },
                .progress = progress,
                .rotation = 0.0,
                .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha)
            });
        }

        for (auto& particle : hitEffect.particles) {
            auto particleTime = hitEffect.time + particle.dt;
            if (particleTime > time) break;
            if (particleTime + chart.options.hitEffectDuration < time) continue;

            auto info = chart.getNoteFrameInfo(line, note, particleTime, safeAreaSize);
            auto effectScreenHeadPosition = toScreen(info.headPosition);
            auto progress = std::clamp((time - particleTime) / chart.options.hitEffectDuration, 0.0, 1.0);
            auto size = standardNoteWidth / 5.3 * chart.options.hitEffectParticleSize * (((0.20783014 * progress - 1.65243926) * progress + 1.6398785) * progress + 0.49884492);
            auto distance = standardNoteWidth / 180 * chart.options.hitEffectParticleDistance * particle.size * (((850.3997391752 * progress + 6236.3848902154) * progress + 80.3542231806) * progress / ((6570.5817658876 * progress + 495.7977913926) * progress + 1.0));

            auto particlePosition = toScreen(info.headPosition.rotateDegress(particle.rotation, distance));
            frame.objects.push_back(CalculatedFrame::CalculatedHitEffectParticle {
                .position = particlePosition,
                .size = { size, size },
                .rotation = 0.0,
                .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha * (1.0 - progress))
            });
        }
    }

    auto combo = chart.getCombo(time);

    time += chart.meta.offset;
    ep_f64 songPorgress = time / config.songLength;

    ep_f64 progressBarHeight = safeAreaSize.x * 0.005921;
    ep_f64 progressBarWidth = safeAreaSize.x * songPorgress;
    ep_f64 progressBarPointWidth = safeAreaSize.x * 0.00175;

    auto& progressBarAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Bar];

    frame.addPoly(
        { 0.0, 0.0 },
        { progressBarWidth, progressBarHeight },
        chart.options.progressBarDefaultColor.first * progressBarAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(progressBarAttachUIData.position)
            .scale(progressBarAttachUIData.scale)
            .rotateDegress(progressBarAttachUIData.rotation)
    );

    frame.addPoly(
        { 0.0, 0.0 },
        { progressBarPointWidth, progressBarHeight },
        chart.options.progressBarDefaultColor.second * progressBarAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(progressBarWidth - progressBarPointWidth, 0.0)
            .translate(progressBarAttachUIData.position)
            .scale(progressBarAttachUIData.scale)
            .rotateDegress(progressBarAttachUIData.rotation)
    );

    auto pauseButtonPosition = Vec2 { 3.16669, 3.6065 } * progressBarHeight;
    auto pauseButtonSize = Vec2 { safeAreaSize.x * 32 / 1920, safeAreaSize.x * 37.48 / 1920 };
    ep_f64 pauseButtonItemWidth = pauseButtonSize.x * 0.323;

    auto& pauseButtonAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Pause];

    frame.addPoly(
        { 0.0, 0.0 },
        { pauseButtonItemWidth, pauseButtonSize.y },
        pauseButtonAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(pauseButtonPosition)
            .translate(pauseButtonAttachUIData.position)
            .scale(pauseButtonAttachUIData.scale)
            .rotateDegress(pauseButtonAttachUIData.rotation)
    );

    frame.addPoly(
        { pauseButtonSize.x - pauseButtonItemWidth, 0.0 },
        { pauseButtonItemWidth, pauseButtonSize.y },
        pauseButtonAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(pauseButtonPosition)
            .translate(pauseButtonAttachUIData.position)
            .scale(pauseButtonAttachUIData.scale)
            .rotateDegress(pauseButtonAttachUIData.rotation)
    );

    if (combo >= 3) {
        frame.objects.push_back(processAttachUIText(CalculatedFrame::CalculatedText {
            .text = std::to_string(combo),
            .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.027083 }),
            .scale = { 1.0, 1.0 },
            .align = EnumTextAlign::Center,
            .baseline = EnumTextBaseline::Middle,
            .fontSize = safeAreaSize.x * 0.0393081,
            .rotation = 0.0,
            .color = Color::White()
        }, EnumPhiLineAttachUI::ComboNumber));

        frame.objects.push_back(processAttachUIText(CalculatedFrame::CalculatedText {
            .text = "AUTOPLAY",
            .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.0478125 }),
            .scale = { 1.0, 1.0 },
            .align = EnumTextAlign::Center,
            .baseline = EnumTextBaseline::Top,
            .fontSize = safeAreaSize.x * 0.0130208,
            .rotation = 0.0,
            .color = Color::White()
        }, EnumPhiLineAttachUI::Combo));
    }

    ep_u64 score = chart.comboTimes.size() ? std::clamp<ep_f64>(std::ceil((ep_f64)1000000 / chart.comboTimes.size() * combo), 0, 1000000) : 1000000;
    frame.objects.push_back(processAttachUIText(CalculatedFrame::CalculatedText {
        .text = formatToStdString("%07llu", score),
        .position = toScreen({ safeAreaSize.x * (1 - ((ep_f64)40 / 1920)), safeAreaSize.x * 0.01614583 }),
        .scale = { 1.0, 1.0 },
        .align = EnumTextAlign::Right,
        .baseline = EnumTextBaseline::Top,
        .fontSize = safeAreaSize.x * 0.0277778,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Score));
    
    frame.objects.push_back(processAttachUIText(CalculatedFrame::CalculatedText {
        .text = chart.meta.title,
        .position = toScreen({ safeAreaSize.x * 0.0225, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
        .scale = { 1.0, 1.0 },
        .align = EnumTextAlign::Left,
        .baseline = EnumTextBaseline::Bottom,
        .fontSize = safeAreaSize.x * 0.018115942,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Name));
    
    frame.objects.push_back(processAttachUIText(CalculatedFrame::CalculatedText {
        .text = chart.meta.difficulty,
        .position = toScreen({ safeAreaSize.x * 0.9775, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
        .scale = { 1.0, 1.0 },
        .align = EnumTextAlign::Right,
        .baseline = EnumTextBaseline::Bottom,
        .fontSize = safeAreaSize.x * 0.018115942,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Level));
}

#undef CHART_LOAD_FAILED

} // namespace easy_phi

#endif // EASY_PHI_HPP

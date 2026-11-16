#pragma once

// 2D integer vector for map cells.

struct Vec2i {
    int x = 0;
    int y = 0;
};

inline bool operator==(const Vec2i& a, const Vec2i& b) {
    return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const Vec2i& a, const Vec2i& b) {
    return !(a == b);
}

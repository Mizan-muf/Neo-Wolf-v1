#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/InputState.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Vec2.h"
#include "engine/world/Map.h"

namespace {
void Assert(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + " assertion failed: " + expression);
    }
}

void AssertNear(
    float actual,
    float expected,
    float epsilon,
    const char* expression,
    const char* file,
    int line) {
    if (std::fabs(actual - expected) > epsilon) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + " assertion failed: " + expression +
            " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

#define TEST_ASSERT(expr) Assert((expr), #expr, __FILE__, __LINE__)
#define TEST_ASSERT_NEAR(actual, expected, epsilon) \
    AssertNear((actual), (expected), (epsilon), #actual " ~= " #expected, __FILE__, __LINE__)

void TestVec2AndMathHelpers() {
    const Vec2 a{3.0f, 4.0f};
    const Vec2 b{1.0f, 2.0f};

    const Vec2 sum = Add(a, b);
    TEST_ASSERT_NEAR(sum.x, 4.0f, 0.0001f);
    TEST_ASSERT_NEAR(sum.y, 6.0f, 0.0001f);

    const Vec2 diff = Subtract(a, b);
    TEST_ASSERT_NEAR(diff.x, 2.0f, 0.0001f);
    TEST_ASSERT_NEAR(diff.y, 2.0f, 0.0001f);

    const Vec2 scaled = Scale(b, 3.0f);
    TEST_ASSERT_NEAR(scaled.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(scaled.y, 6.0f, 0.0001f);

    TEST_ASSERT_NEAR(Length(a), 5.0f, 0.0001f);

    const Vec2 normalized = Normalize(a);
    TEST_ASSERT_NEAR(normalized.x, 0.6f, 0.0001f);
    TEST_ASSERT_NEAR(normalized.y, 0.8f, 0.0001f);

    const Vec2 normalizedZero = Normalize(Vec2{0.0f, 0.0f});
    TEST_ASSERT_NEAR(normalizedZero.x, 0.0f, 0.0001f);
    TEST_ASSERT_NEAR(normalizedZero.y, 0.0f, 0.0001f);

    TEST_ASSERT_NEAR(Dot(a, b), 11.0f, 0.0001f);

    TEST_ASSERT(Clamp(10, 0, 5) == 5);
    TEST_ASSERT(Clamp(-2, 0, 5) == 0);
    TEST_ASSERT_NEAR(Clamp(0.4f, 0.0f, 1.0f), 0.4f, 0.0001f);

    TEST_ASSERT_NEAR(DegreesToRadians(180.0f), kPi, 0.0001f);
    TEST_ASSERT_NEAR(RadiansToDegrees(kPi), 180.0f, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngleRadians(-kPi / 2.0f), 1.5f * kPi, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngleDegrees(-90.0f), 270.0f, 0.0001f);
}

void TestInputStateTransitions() {
    InputState input{};

    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.BeginFrame();
    TEST_ASSERT(input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, false);
    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(input.WasReleased(InputAction::MoveForward));

    input.BeginFrame();
    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));
}

void TestMapQueries() {
    Map map(4, 3);

    TEST_ASSERT(map.Width() == 4);
    TEST_ASSERT(map.Height() == 3);

    TEST_ASSERT(map.IsInsideMap(0, 0));
    TEST_ASSERT(map.IsInsideMap(3, 2));
    TEST_ASSERT(!map.IsInsideMap(-1, 0));
    TEST_ASSERT(!map.IsInsideMap(4, 1));
    TEST_ASSERT(!map.IsInsideMap(2, 3));

    TEST_ASSERT(map.GetCell(1, 1) == TileType::Empty);
    TEST_ASSERT(map.GetCell(-1, -1) == TileType::Empty);

    TEST_ASSERT(map.SetCell(1, 1, TileType::Wall));
    TEST_ASSERT(map.GetCell(1, 1) == TileType::Wall);
    TEST_ASSERT(map.IsWall(1, 1));

    TEST_ASSERT(map.SetCell(2, 1, TileType::Door));
    TEST_ASSERT(map.GetCell(2, 1) == TileType::Door);
    TEST_ASSERT(!map.IsWall(2, 1));

    TEST_ASSERT(map.SetCell(2, 2, TileType::Trigger));
    TEST_ASSERT(map.GetCell(2, 2) == TileType::Trigger);
    TEST_ASSERT(!map.IsWall(2, 2));

    TEST_ASSERT(!map.SetCell(10, 10, TileType::Wall));
    TEST_ASSERT(!map.IsWall(-1, 0));
}
} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };

    const std::vector<TestCase> tests{
        {"Vec2 and math helpers", &TestVec2AndMathHelpers},
        {"InputState transitions", &TestInputStateTransitions},
        {"Map queries", &TestMapQueries},
    };

    std::size_t failures = 0;
    for (const TestCase& test : tests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test case(s) failed.\n";
        return 1;
    }

    std::cout << "All test cases passed.\n";
    return 0;
}

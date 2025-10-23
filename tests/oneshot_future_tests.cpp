#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>
#include <optional>
#include "OneShotFuture.hpp"

using namespace std::chrono_literals;

// --------------------------------------------------
// OneShot<T> tests
// --------------------------------------------------

TEST(OneShotTest, SimpleValueTransfer) {
    auto pair = OneShot<int>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    std::thread producer([&]() {
        std::this_thread::sleep_for(50ms);
        EXPECT_TRUE(s.set_value(42));
    });

    auto val = r.get();
    EXPECT_EQ(val, 42);
    producer.join();
}

TEST(OneShotTest, TimeoutAndThenGet) {
    auto pair = OneShot<int>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    std::thread producer([&]() {
        std::this_thread::sleep_for(150ms);
        s.set_value(77);
    });

    auto val = r.get_for(50ms);
    EXPECT_FALSE(val.has_value()); // should timeout

    auto val2 = r.get();
    EXPECT_EQ(val2, 77);

    producer.join();
}

TEST(OneShotTest, ExceptionPropagation) {
    auto pair = OneShot<int>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    std::thread producer([&]() {
        s.set_exception(std::make_exception_ptr(std::runtime_error("fail")));
    });

    EXPECT_THROW(r.get(), std::runtime_error);
    producer.join();
}

TEST(OneShotTest, BrokenPromiseThrows) {
    auto pair = OneShot<int>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    // Destroy sender without setting a value
    s = {};
    EXPECT_THROW(r.get(), std::future_error);
}

TEST(OneShotTest, ReadyCheck) {
    auto pair = OneShot<int>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    EXPECT_FALSE(r.ready());
    s.set_value(5);
    EXPECT_TRUE(r.ready());
    EXPECT_EQ(r.get(), 5);
}

// --------------------------------------------------
// OneShot<void> tests
// --------------------------------------------------

TEST(OneShotVoidTest, SimpleSignal) {
    auto pair = OneShot<void>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    std::thread t([&]() {
        std::this_thread::sleep_for(50ms);
        s.set_value();
    });

    EXPECT_FALSE(r.ready());
    r.get(); // should complete without throwing
    t.join();
}

TEST(OneShotVoidTest, Timeout) {
    auto pair = OneShot<void>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    EXPECT_FALSE(r.get_for(20ms));
    s.set_value();
    EXPECT_TRUE(r.get_for(100ms));
}

TEST(OneShotVoidTest, BrokenPromiseThrows) {
    auto pair = OneShot<void>::make();
    auto& s = pair.first;
    auto& r = pair.second;

    s = {};
    EXPECT_THROW(r.get(), std::future_error);
}


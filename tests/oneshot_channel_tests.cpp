#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>
#include <optional>
#include "OneShotChannel.hpp"

using namespace std::chrono_literals;

// --------------------------------------------------
// OneShotChannel<T> tests
// --------------------------------------------------

TEST(OneShotChannelTest, SimpleValueTransfer) {
    auto [s, r] = OneShotChannel<int>::make();

    std::thread producer([&]() {
        std::this_thread::sleep_for(50ms);
        EXPECT_TRUE(s.set_value(123));
    });

    auto val = r.get();
    EXPECT_EQ(val, 123);
    producer.join();
}

TEST(OneShotChannelTest, TimeoutAndReset) {
    auto [s, r] = OneShotChannel<int>::make();

    EXPECT_FALSE(r.get_for(20ms)); // no value yet

    s.set_value(9);
    EXPECT_EQ(r.get_for(100ms), std::optional<int>(9));

    // reuse the same channel
    EXPECT_TRUE(s.reset());
    EXPECT_TRUE(r.reset());

    std::thread producer([&]() {
        std::this_thread::sleep_for(30ms);
        EXPECT_TRUE(s.set_value(42));
    });

    auto result = r.get_for(200ms);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);

    producer.join();
}

TEST(OneShotChannelTest, DISABLED_BrokenPromiseThrows) {
    auto [s, r] = OneShotChannel<int>::make();

    // destroy sender
    s = {};
    EXPECT_THROW(r.get(), std::future_error);
}

TEST(OneShotChannelTest, ExceptionPropagation) {
    auto [s, r] = OneShotChannel<int>::make();

    std::thread producer([&]() {
        s.set_exception(std::make_exception_ptr(std::runtime_error("bad")));
    });

    EXPECT_THROW(r.get(), std::runtime_error);
    producer.join();
}

TEST(OneShotChannelTest, MultipleResetsWork) {
    auto [s, r] = OneShotChannel<int>::make();

    for (int i = 0; i < 3; ++i) {
        std::thread t([&, i]() mutable {
            std::this_thread::sleep_for(10ms);
            s.set_value(i);
        });
        auto val = r.get_for(100ms);
        EXPECT_TRUE(val.has_value());
        EXPECT_EQ(*val, i);
        s.reset();
        r.reset();
        t.join();
    }
}

// --------------------------------------------------
// OneShotChannel<void> tests
// --------------------------------------------------

TEST(OneShotChannelVoidTest, BasicSignal) {
    auto [s, r] = OneShotChannel<void>::make();

    std::thread producer([&]() {
        std::this_thread::sleep_for(50ms);
        s.set_value();
    });

    EXPECT_FALSE(r.ready());
    r.get();
    producer.join();
}

TEST(OneShotChannelVoidTest, ResetAndReuse) {
    auto [s, r] = OneShotChannel<void>::make();

    s.set_value();
    r.get(); // ok

    s.reset();
    r.reset();

    std::thread t([&]() {
        std::this_thread::sleep_for(20ms);
        s.set_value();
    });

    EXPECT_TRUE(r.get_for(200ms));
    t.join();
}

TEST(OneShotChannelVoidTest, DISABLED_BrokenPromiseThrows) {
    auto [s, r] = OneShotChannel<void>::make();
    s = {};
    EXPECT_THROW(r.get(), std::future_error);
}

TEST(OneShotChannelVoidTest, ExceptionPropagation) {
    auto [s, r] = OneShotChannel<void>::make();
    std::thread t([&]() {
        s.set_exception(std::make_exception_ptr(std::runtime_error("oops")));
    });
    EXPECT_THROW(r.get(), std::runtime_error);
    t.join();
}


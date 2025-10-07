/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */

#pragma once
#include <future>
#include <memory>
#include <atomic>
#include <optional>
#include <chrono>
#include <exception>
#include <mutex>

//
// A resettable "one-shot" channel using std::promise/std::future under the hood
// C++17-compatible
//
template<typename T>
class OneShotChannel {
public:
    class Sender;
    class Receiver;

private:
    struct Shared {
        std::mutex mtx;
        std::promise<T> promise;
        std::future<T> future;
        bool used = false;

        Shared() : future(promise.get_future()) {}

        void reset_locked() {
            promise = std::promise<T>();
            future = promise.get_future();
            used = false;
        }
    };

    std::shared_ptr<Shared> state_;

    explicit OneShotChannel(std::shared_ptr<Shared> s) : state_(std::move(s)) {}

public:
    static std::pair<Sender, Receiver> make() {
        auto s = std::make_shared<Shared>();
        return {Sender{s}, Receiver{s}};
    }

    //
    // Sending side
    //
    class Sender {
        std::shared_ptr<Shared> state_;
    public:
        Sender() = default;
        explicit Sender(std::shared_ptr<Shared> s) : state_(std::move(s)) {}

        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            if (state_) {
                std::lock_guard<std::mutex> lock(state_->mtx);
                if (!state_->used) {
                    try {
                        state_->promise.set_exception(std::make_exception_ptr(
                            std::future_error(std::future_errc::broken_promise)));
                    } catch (...) {}
                }
            }
        }

        bool set_value(T value) {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->used) return false;
            state_->used = true;
            state_->promise.set_value(std::move(value));
            return true;
        }

        bool set_exception(std::exception_ptr e) {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->used) return false;
            state_->used = true;
            state_->promise.set_exception(std::move(e));
            return true;
        }

        bool reset() {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            state_->reset_locked();
            return true;
        }

        explicit operator bool() const noexcept { return (bool)state_; }
    };

    //
    // Receiving side
    //
    class Receiver {
        std::shared_ptr<Shared> state_;
    public:
        Receiver() = default;
        explicit Receiver(std::shared_ptr<Shared> s) : state_(std::move(s)) {}

        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;

        T get() {
            if (!state_) throw std::future_error(std::future_errc::no_state);
            return state_->future.get();
        }

        bool ready() const {
            if (!state_) return false;
            using namespace std::chrono_literals;
            return state_->future.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        std::optional<T> get_for(const std::chrono::duration<Rep, Period>& dur) {
            if (!state_) return std::nullopt;
            if (state_->future.wait_for(dur) == std::future_status::ready) {
                return state_->future.get();
            }
            return std::nullopt;
        }

        bool reset() {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            state_->reset_locked();
            return true;
        }

        explicit operator bool() const noexcept { return (bool)state_; }
    };
};


//
// void specialization
//
template<>
class OneShotChannel<void> {
public:
    class Sender;
    class Receiver;

private:
    struct Shared {
        std::mutex mtx;
        std::promise<void> promise;
        std::future<void> future;
        bool used = false;

        Shared() : future(promise.get_future()) {}

        void reset_locked() {
            promise = std::promise<void>();
            future = promise.get_future();
            used = false;
        }
    };

    std::shared_ptr<Shared> state_;
    explicit OneShotChannel(std::shared_ptr<Shared> s) : state_(std::move(s)) {}

public:
    static std::pair<Sender, Receiver> make() {
        auto s = std::make_shared<Shared>();
        return {Sender{s}, Receiver{s}};
    }

    class Sender {
        std::shared_ptr<Shared> state_;
    public:
        Sender() = default;
        explicit Sender(std::shared_ptr<Shared> s) : state_(std::move(s)) {}
        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            if (state_) {
                std::lock_guard<std::mutex> lock(state_->mtx);
                if (!state_->used) {
                    try {
                        state_->promise.set_exception(std::make_exception_ptr(
                            std::future_error(std::future_errc::broken_promise)));
                    } catch (...) {}
                }
            }
        }

        bool set_value() {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->used) return false;
            state_->used = true;
            state_->promise.set_value();
            return true;
        }

        bool set_exception(std::exception_ptr e) {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            if (state_->used) return false;
            state_->used = true;
            state_->promise.set_exception(std::move(e));
            return true;
        }

        bool reset() {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            state_->reset_locked();
            return true;
        }

        explicit operator bool() const noexcept { return (bool)state_; }
    };

    class Receiver {
        std::shared_ptr<Shared> state_;
    public:
        Receiver() = default;
        explicit Receiver(std::shared_ptr<Shared> s) : state_(std::move(s)) {}

        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;

        void get() {
            if (!state_) throw std::future_error(std::future_errc::no_state);
            state_->future.get();
        }

        bool ready() const {
            if (!state_) return false;
            using namespace std::chrono_literals;
            return state_->future.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        bool get_for(const std::chrono::duration<Rep, Period>& dur) {
            if (!state_) return false;
            if (state_->future.wait_for(dur) == std::future_status::ready) {
                state_->future.get();
                return true;
            }
            return false;
        }

        bool reset() {
            if (!state_) return false;
            std::lock_guard<std::mutex> lock(state_->mtx);
            state_->reset_locked();
            return true;
        }

        explicit operator bool() const noexcept { return (bool)state_; }
    };
};


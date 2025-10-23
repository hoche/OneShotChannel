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
// Thread safety: Senders serialize via promise operations. Receivers use shared_future
// with copy-under-lock pattern to allow safe concurrent reset() without data races.
template<typename T>
class OneShotChannel {
public:
    class Sender;
    class Receiver;

private:
    struct Shared {
        std::mutex mtx;
        std::promise<T> promise;
        std::shared_future<T> future;  // shared_future allows multiple concurrent readers
        bool used = false;

        Shared() : future(promise.get_future().share()) {}

        void reset_locked() {
            promise = std::promise<T>();
            future = promise.get_future().share();
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
        Sender& operator=(Sender&& other) noexcept {
            if (this != &other) {
                // If current sender has state and is being replaced, set broken promise
                if (state_) {
                    std::lock_guard<std::mutex> lock(state_->mtx);
                    if (!state_->used) {
                        try {
                            state_->promise.set_exception(std::make_exception_ptr(
                                std::future_error(std::future_errc::broken_promise)));
                            state_->used = true;
                        } catch (...) {}
                    }
                }
                state_ = std::move(other.state_);
            }
            return *this;
        }
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            if (state_) {
                std::lock_guard<std::mutex> lock(state_->mtx);
                if (!state_->used) {
                    try {
                        state_->promise.set_exception(std::make_exception_ptr(
                            std::future_error(std::future_errc::broken_promise)));
                        state_->used = true;
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
            // Copy future under lock to avoid race with reset()
            if (!state_) throw std::future_error(std::future_errc::no_state);
            std::shared_future<T> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            return local.get();
        }

        bool ready() const {
            // Copy future under lock to avoid race with reset()
            if (!state_) return false;
            using namespace std::chrono_literals;
            std::shared_future<T> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            return local.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        std::optional<T> get_for(const std::chrono::duration<Rep, Period>& dur) {
            // Copy future under lock to avoid race with reset().
            // If an exception occurs (e.g., broken_promise during concurrent reset),
            // it is swallowed and std::nullopt is returned (same as timeout).
            if (!state_) return std::nullopt;
            std::shared_future<T> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            if (local.wait_for(dur) == std::future_status::ready) {
                try {
                    return local.get();
                } catch (...) {
                    // Treat broken/exceptional state as not available under timed get
                    return std::nullopt;
                }
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
// Same thread-safety model as OneShotChannel<T>.
template<>
class OneShotChannel<void> {
public:
    class Sender;
    class Receiver;

private:
    struct Shared {
        std::mutex mtx;
        std::promise<void> promise;
        std::shared_future<void> future;  // shared_future allows multiple concurrent readers
        bool used = false;

        Shared() : future(promise.get_future().share()) {}

        void reset_locked() {
            promise = std::promise<void>();
            future = promise.get_future().share();
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
        Sender& operator=(Sender&& other) noexcept {
            if (this != &other) {
                // If current sender has state and is being replaced, set broken promise
                if (state_) {
                    std::lock_guard<std::mutex> lock(state_->mtx);
                    if (!state_->used) {
                        try {
                            state_->promise.set_exception(std::make_exception_ptr(
                                std::future_error(std::future_errc::broken_promise)));
                            state_->used = true;
                        } catch (...) {}
                    }
                }
                state_ = std::move(other.state_);
            }
            return *this;
        }
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            if (state_) {
                std::lock_guard<std::mutex> lock(state_->mtx);
                if (!state_->used) {
                    try {
                        state_->promise.set_exception(std::make_exception_ptr(
                            std::future_error(std::future_errc::broken_promise)));
                        state_->used = true;
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
            // Copy future under lock to avoid race with reset()
            if (!state_) throw std::future_error(std::future_errc::no_state);
            std::shared_future<void> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            local.get();
        }

        bool ready() const {
            // Copy future under lock to avoid race with reset()
            if (!state_) return false;
            using namespace std::chrono_literals;
            std::shared_future<void> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            return local.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        bool get_for(const std::chrono::duration<Rep, Period>& dur) {
            // Copy future under lock to avoid race with reset().
            // If an exception occurs (e.g., broken_promise during concurrent reset),
            // it is swallowed and false is returned (same as timeout).
            if (!state_) return false;
            std::shared_future<void> local;
            {
                std::lock_guard<std::mutex> lock(state_->mtx);
                local = state_->future;
            }
            if (local.wait_for(dur) == std::future_status::ready) {
                try {
                    local.get();
                    return true;
                } catch (...) {
                    // Treat broken/exceptional state as not available under timed get
                    return false;
                }
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


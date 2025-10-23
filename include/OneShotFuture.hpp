/**
 * Copyright @ 2024 Michel Hoche-Mong
 * SPDX-License-Identifier: CC-BY-4.0
 */

#pragma once
#include <future>
#include <memory>
#include <atomic>
#include <utility>
#include <optional>
#include <exception>
#include <chrono>

//
// Generic template
//
template<typename T>
class OneShot {
public:
    class Sender {
        std::shared_ptr<std::promise<T>> prom_;
        std::shared_ptr<std::atomic<bool>> used_;

    public:
        Sender() = default;
        Sender(std::shared_ptr<std::promise<T>> p, std::shared_ptr<std::atomic<bool>> u)
            : prom_(std::move(p)), used_(std::move(u)) {}

        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            // if promise not fulfilled, mark broken_promise
            if (prom_ && !used_->load()) {
                try {
                    prom_->set_exception(
                        std::make_exception_ptr(std::future_error(std::future_errc::broken_promise)));
                } catch (...) {
                    // ignore double-set exceptions
                }
            }
        }

        bool set_value(T value) {
            if (!prom_) return false;
            bool expected = false;
            if (!used_->compare_exchange_strong(expected, true)) return false;
            prom_->set_value(std::move(value));
            return true;
        }

        bool set_exception(std::exception_ptr e) {
            if (!prom_) return false;
            bool expected = false;
            if (!used_->compare_exchange_strong(expected, true)) return false;
            prom_->set_exception(std::move(e));
            return true;
        }

        explicit operator bool() const noexcept { return (bool)prom_; }
    };

    class Receiver {
        std::future<T> fut_;

    public:
        Receiver() = default;
        explicit Receiver(std::future<T>&& f) : fut_(std::move(f)) {}

        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;

        T get() { return fut_.get(); }

        bool ready() const {
            using namespace std::chrono_literals;
            return fut_.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& dur) const {
            return fut_.wait_for(dur) == std::future_status::ready;
        }

        // Returns std::optional<T> with timeout
        template<typename Rep, typename Period>
        std::optional<T> get_for(const std::chrono::duration<Rep, Period>& dur) {
            if (fut_.wait_for(dur) == std::future_status::ready) {
                return fut_.get();
            }
            return std::nullopt;
        }

        explicit operator bool() const noexcept { return fut_.valid(); }
    };

    static std::pair<Sender, Receiver> make() {
        auto prom = std::make_shared<std::promise<T>>();
        auto used = std::make_shared<std::atomic<bool>>(false);
        Sender s{prom, used};
        Receiver r{prom->get_future()};
        return {std::move(s), std::move(r)};
    }
};


//
// void specialization
//
template<>
class OneShot<void> {
public:
    class Sender {
        std::shared_ptr<std::promise<void>> prom_;
        std::shared_ptr<std::atomic<bool>> used_;

    public:
        Sender() = default;
        Sender(std::shared_ptr<std::promise<void>> p, std::shared_ptr<std::atomic<bool>> u)
            : prom_(std::move(p)), used_(std::move(u)) {}

        Sender(Sender&&) noexcept = default;
        Sender& operator=(Sender&&) noexcept = default;
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;

        ~Sender() {
            if (prom_ && !used_->load()) {
                try {
                    prom_->set_exception(
                        std::make_exception_ptr(std::future_error(std::future_errc::broken_promise)));
                } catch (...) {
                }
            }
        }

        bool set_value() {
            if (!prom_) return false;
            bool expected = false;
            if (!used_->compare_exchange_strong(expected, true)) return false;
            prom_->set_value();
            return true;
        }

        bool set_exception(std::exception_ptr e) {
            if (!prom_) return false;
            bool expected = false;
            if (!used_->compare_exchange_strong(expected, true)) return false;
            prom_->set_exception(std::move(e));
            return true;
        }

        explicit operator bool() const noexcept { return (bool)prom_; }
    };

    class Receiver {
        std::future<void> fut_;

    public:
        Receiver() = default;
        explicit Receiver(std::future<void>&& f) : fut_(std::move(f)) {}

        Receiver(Receiver&&) noexcept = default;
        Receiver& operator=(Receiver&&) noexcept = default;
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;

        void get() { fut_.get(); }

        bool ready() const {
            using namespace std::chrono_literals;
            return fut_.wait_for(0s) == std::future_status::ready;
        }

        template<typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& dur) const {
            return fut_.wait_for(dur) == std::future_status::ready;
        }

        // returns true if completed within timeout
        template<typename Rep, typename Period>
        bool get_for(const std::chrono::duration<Rep, Period>& dur) {
            if (fut_.wait_for(dur) == std::future_status::ready) {
                fut_.get();
                return true;
            }
            return false;
        }

        explicit operator bool() const noexcept { return fut_.valid(); }
    };

    static std::pair<Sender, Receiver> make() {
        auto prom = std::make_shared<std::promise<void>>();
        auto used = std::make_shared<std::atomic<bool>>(false);
        Sender s{prom, used};
        Receiver r{prom->get_future()};
        return {std::move(s), std::move(r)};
    }
};


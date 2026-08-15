/**
 * @file include/core/Optional.h
 * @brief Provides the C++14 optional value used by public ManuMesh APIs.
 * @ingroup manumesh_core
 */

#pragma once

#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace manumesh {

template <typename T> class Optional {
public:
    Optional() noexcept = default;

    Optional(const T& value) { construct(value); }

    Optional(T&& value) { construct(std::move(value)); }

    Optional(const Optional& other) {
        if (other.engaged_) {
            construct(*other.ptr());
        }
    }

    Optional(Optional&& other) noexcept(std::is_nothrow_move_constructible<T>::value) {
        if (other.engaged_) {
            construct(std::move(*other.ptr()));
        }
    }

    ~Optional() { reset(); }

    Optional& operator=(const Optional& other) {
        if (this == &other) {
            return *this;
        }
        if (engaged_ && other.engaged_) {
            *ptr() = *other.ptr();
        } else if (other.engaged_) {
            construct(*other.ptr());
        } else {
            reset();
        }
        return *this;
    }

    Optional& operator=(Optional&& other) noexcept(
        std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value
    ) {
        if (this == &other) {
            return *this;
        }
        if (engaged_ && other.engaged_) {
            *ptr() = std::move(*other.ptr());
        } else if (other.engaged_) {
            construct(std::move(*other.ptr()));
        } else {
            reset();
        }
        return *this;
    }

    Optional& operator=(const T& value) {
        assign(value);
        return *this;
    }

    Optional& operator=(T&& value) {
        assign(std::move(value));
        return *this;
    }

    bool has_value() const noexcept { return engaged_; }

    explicit operator bool() const noexcept { return engaged_; }

    T& value() {
        requireValue();
        return *ptr();
    }

    const T& value() const {
        requireValue();
        return *ptr();
    }

    T& operator*() { return value(); }

    const T& operator*() const { return value(); }

    T* operator->() { return &value(); }

    const T* operator->() const { return &value(); }

    template <typename... Args> T& emplace(Args&&... args) {
        reset();
        construct(std::forward<Args>(args)...);
        return *ptr();
    }

    void reset() noexcept {
        if (engaged_) {
            ptr()->~T();
            engaged_ = false;
        }
    }

private:
    template <typename... Args> void construct(Args&&... args) {
        new (&storage_) T(std::forward<Args>(args)...);
        engaged_ = true;
    }

    template <typename U> void assign(U&& value) {
        if (engaged_) {
            *ptr() = std::forward<U>(value);
        } else {
            construct(std::forward<U>(value));
        }
    }

    void requireValue() const {
        if (!engaged_) {
            throw std::logic_error("Optional has no value.");
        }
    }

    T* ptr() { return reinterpret_cast<T*>(&storage_); }

    const T* ptr() const { return reinterpret_cast<const T*>(&storage_); }

    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
    bool engaged_ = false;
};

} // namespace manumesh

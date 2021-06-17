#pragma once


#include <memory>


namespace zen {


template <class T>
struct copiable_unique_ptr : std::unique_ptr<T> {
    using std::unique_ptr<T>::unique_ptr;

    copiable_unique_ptr(std::unique_ptr<T> &&o)
        : std::unique_ptr<T>(std::move(o)) {
    }

    copiable_unique_ptr(copiable_unique_ptr const &o)
        : std::unique_ptr<T>(std::make_unique<T>(
            static_cast<T const &>(*o))) {
    }

    operator std::unique_ptr<T> &() { return *this; }
    operator std::unique_ptr<T> const &() const { return *this; }
};


template <class T>
copiable_unique_ptr(std::unique_ptr<T> &&o) -> copiable_unique_ptr<T>;


}

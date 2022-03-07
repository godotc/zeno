#pragma once

#include <tuple>

namespace zeno {

struct tuple_hash {
    std::size_t operator()(std::tuple<int, int> const &key) const {
        auto const &[x, y] = key;
        return (x * 2718281828) ^ (y * 3141592653);
    }
};

}
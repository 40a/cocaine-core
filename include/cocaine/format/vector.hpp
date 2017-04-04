#pragma once

#include <vector>

#include "cocaine/format/base.hpp"
#include "cocaine/format/linear_container.hpp"

namespace cocaine {

template<typename T>
struct display<std::vector<T>> : public linear_container_display<std::vector<T>> {};

template<typename T>
struct display_traits<std::vector<T>> : public lazy_display<std::vector<T>> {};

} // namespace cocaine

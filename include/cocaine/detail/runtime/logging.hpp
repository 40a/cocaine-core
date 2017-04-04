/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_BOOTSTRAP_LOGGING_HPP
#define COCAINE_BOOTSTRAP_LOGGING_HPP

#include <blackhole/factory.hpp>

namespace cocaine { namespace logging {

class console_t;

}}

namespace blackhole {
inline namespace v1 {

template<>
struct factory<cocaine::logging::console_t> : public factory<sink_t> {
    auto type() const noexcept -> const char*;
    auto from(const config::node_t& config) const -> std::unique_ptr<sink_t>;
};

}  // namespace v1
}  // namespace blackhole

#endif

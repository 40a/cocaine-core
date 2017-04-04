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

#ifndef COCAINE_COMMON_HPP
#define COCAINE_COMMON_HPP

#include "cocaine/config.hpp"
#include "cocaine/platform.hpp"

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED

#define COCAINE_DECLARE_NONCOPYABLE(type)   \
    type(const type& other) = delete;       \
                                            \
    type&                                   \
    operator=(const type& other) = delete;

#define COCAINE_UNUSED_(parameter)          \
    parameter __attribute__((unused))

#include "cocaine/forwards.hpp"

#endif

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

#ifndef COCAINE_IO_SLOT_HPP
#define COCAINE_IO_SLOT_HPP

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/tuple.hpp"

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/optional/optional_fwd.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Event>
class basic_slot {
    typedef Event event_type;
    typedef event_traits<event_type> traits_type;

public:
    // Expected dispatch, parameter and upstream types.
    typedef std::vector<hpack::header_t> meta_type;
    typedef typename traits_type::tuple_type tuple_type;
    typedef typename traits_type::sequence_type sequence_type;
    typedef dispatch<typename traits_type::dispatch_type> dispatch_type;
    typedef upstream<typename traits_type::upstream_type> upstream_type;
    typedef boost::optional<std::shared_ptr<dispatch_type>> result_type;

    virtual
   ~basic_slot() = default;

    virtual
    boost::optional<std::shared_ptr<dispatch_type>>
    operator()(const meta_type& meta, tuple_type&& args, upstream_type&& upstream) = 0;
};

template<class Event>
struct is_recursed:
    public std::is_same<typename event_traits<Event>::dispatch_type, typename Event::tag>
{ };

template<class Event>
struct is_terminal:
    public std::is_same<typename event_traits<Event>::dispatch_type, void>
{ };

}} // namespace cocaine::io

#endif

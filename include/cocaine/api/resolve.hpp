/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RESOLVE_HPP
#define COCAINE_RESOLVE_HPP

#include "cocaine/api/connect.hpp"
#include "cocaine/idl/locator.hpp"

namespace cocaine { namespace api {

class resolve_t {
    COCAINE_DECLARE_NONCOPYABLE(resolve_t)

    typedef boost::asio::ip::tcp::endpoint endpoint_type;
    typedef std::function<void(const boost::system::error_code&)> handler_type;

    class resolve_action_t;
    class connect_action_t;

    static const std::vector<endpoint_type> kDefaultEndpoints;

    const std::unique_ptr<logging::log_t> m_log;

    boost::asio::io_service& m_asio;
    client<io::locator_tag>  m_locator;

    struct pending_request_t {
        std::shared_ptr<resolve_action_t> dispatch;
        std::string name;
    };

    std::deque<pending_request_t> m_pending;

public:
    resolve_t(std::unique_ptr<logging::log_t> log, boost::asio::io_service& asio,
              const std::vector<endpoint_type>& endpoints = kDefaultEndpoints);

    void
    resolve(details::basic_client_t& client, const std::string& name, handler_type handle);

    void
    connect(details::basic_client_t& client, const std::vector<endpoint_type>& endpoints, handler_type handle);

private:
    void
    resolve_pending(const boost::system::error_code& ec);
};

}} // namespace cocaine::api

namespace cocaine { namespace error {

enum resolve_errors {
    version_mismatch = 1
};

auto
make_error_code(resolve_errors code) -> boost::system::error_code;

}} // namespace cocaine::error

namespace boost { namespace system {

template<>
struct is_error_code_enum<cocaine::error::resolve_errors>:
    public true_type
{ };

}} // namespace boost::system

#endif

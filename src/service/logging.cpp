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

#include "cocaine/detail/service/logging.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/enum.hpp"

using namespace cocaine::io;
using namespace cocaine::logging;
using namespace cocaine::service;

logging_t::logging_t(context_t& context, reactor_t& reactor, const std::string& name, const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    dispatch<io::log_tag>(name)
{
    typedef logger_t logger_type;

    auto backend = args.as_object().at("backend", "core").as_string();

    try {
        m_logger = std::make_unique<logger_type>(blackhole::repository_t::instance().create<priorities>(backend));
        m_logger->verbosity(context.log(name)->log().verbosity());
    } catch(const std::out_of_range&) {
        throw cocaine::error_t("the '%s' logger is not configured", backend);
    }

    using namespace std::placeholders;

    auto getter = static_cast<priorities(logger_type::*)()const>(&logger_type::verbosity);
    auto setter = static_cast<void(logger_type::*)(priorities)>(&logger_type::verbosity);

    on<io::log::emit>(std::bind(&logging_t::emit, this, _1, _2, _3, _4));
    on<io::log::verbosity>(std::bind(getter, std::ref(*m_logger)));
    on<io::log::set_verbosity>(std::bind(setter, std::ref(*m_logger), _1));
}

auto
logging_t::prototype() const -> const basic_dispatch_t& {
    return *this;
}

void
logging_t::emit(logging::priorities level, const std::string& source, const std::string& message,
                const blackhole::log::attributes_t& attributes)
{
    auto record = m_logger->open_record(level);

    if(record.valid()) {
        record.attributes.insert(attributes.begin(), attributes.end());
        record.attributes.insert(blackhole::keyword::message() = message);
        record.attributes.insert(cocaine::logging::keyword::source() = source);

        m_logger->push(std::move(record));
    }
}
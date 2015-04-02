/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/detail/service/locator.hpp"

#include "cocaine/api/gateway.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/unique_id.hpp"

#include "cocaine/idl/streaming.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/net.hpp"

#include "cocaine/rpc/actor.hpp"

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/traits/vector.hpp"

#include <asio/connect.hpp>

#include <blackhole/scoped_attributes.hpp>

#include <boost/range/adaptor/map.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace asio;
using namespace asio::ip;

using namespace blackhole;

using namespace cocaine::io;
using namespace cocaine::service;

// Locator internals

class locator_t::remote_t:
    public dispatch<event_traits<locator::connect>::upstream_type>,
    public std::enable_shared_from_this<remote_t>
{
    locator_t      *const parent;
    std::string     const uuid;

    // Currently announced services.
    std::set<std::string> active;

public:
    remote_t(locator_t *const parent_, const std::string& uuid_):
        dispatch<event_traits<locator::connect>::upstream_type>(parent_->name() + ":remote"),
        parent(parent_),
        uuid(uuid_)
    {
        typedef io::protocol<event_traits<locator::connect>::upstream_type>::scope protocol;

        using namespace std::placeholders;

        on<protocol::chunk>(std::bind(&remote_t::on_announce, this, _1, _2));
        on<protocol::choke>(std::bind(&remote_t::on_shutdown, this));
    }

    virtual
   ~remote_t() {
        for(auto it = active.begin(); it != active.end(); ++it) {
            parent->m_gateway->cleanup(uuid, *it);
        }
    }

    virtual
    void
    discard(const std::error_code& ec) const;

private:
    void
    on_announce(const std::string& node, const std::map<std::string, results::resolve>& update);

    void
    on_shutdown();
};

void
locator_t::remote_t::discard(const std::error_code& ec) const {
    if(ec.value() == 0) return;

    COCAINE_LOG_ERROR(parent->m_log, "remote node discarded: [%d] %s", ec.value(), ec.message())(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

void
locator_t::remote_t::on_announce(const std::string& node,
                                 const std::map<std::string, results::resolve>& update)
{
    if(node != uuid) {
        COCAINE_LOG_ERROR(parent->m_log, "remote node id mismatch: '%s' vs. '%s'", uuid, node);

        parent->drop_node(uuid);
        return;
    }

    for(auto it = update.begin(); it != update.end(); ++it) {
        std::vector<tcp::endpoint> endpoints;

        // Deactivated services are announced with no endpoints.
        std::tie(endpoints, std::ignore, std::ignore) = it->second;

        if(endpoints.empty()) {
            parent->m_gateway->cleanup(uuid, it->first);
            active.erase (it->first);
        } else {
            parent->m_gateway->consume(uuid, it->first, it->second);
            active.insert(it->first);
        }
    }

    std::ostringstream stream;
    std::ostream_iterator<char> builder(stream);

    boost::spirit::karma::generate(
        builder,
        boost::spirit::karma::string % ", ",
        update | boost::adaptors::map_keys
    );

    COCAINE_LOG_INFO(parent->m_log, "remote node updated %d service(s): %s", update.size(), stream.str())(
        "uuid", uuid
    );
}

void
locator_t::remote_t::on_shutdown() {
    COCAINE_LOG_INFO(parent->m_log, "remote node closed synchronization stream")(
        "uuid", uuid
    );

    parent->drop_node(uuid);
}

// Locator

locator_cfg_t::locator_cfg_t(const std::string& name_, const dynamic_t& root):
    name(name_),
    uuid(root.as_object().at("uuid", unique_id_t().string()).as_string())
{
    restricted = root.as_object().at("restrict", dynamic_t::array_t()).to<std::set<std::string>>();
    restricted.insert(name);
}

locator_t::locator_t(context_t& context, io_service& asio, const std::string& name, const dynamic_t& root):
    category_type(context, asio, name, root),
    dispatch<locator_tag>(name),
    m_context(context),
    m_log(context.log(name)),
    m_cfg(name, root),
    m_asio(asio)
{
    using namespace std::placeholders;

    on<locator::resolve>(std::bind(&locator_t::on_resolve, this, _1, _2));
    on<locator::connect>(std::bind(&locator_t::on_connect, this, _1));
    on<locator::refresh>(std::bind(&locator_t::on_refresh, this, _1));
    on<locator::cluster>(std::bind(&locator_t::on_cluster, this));

    // Service restrictions

    if(!m_cfg.restricted.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", m_cfg.restricted);

        COCAINE_LOG_INFO(m_log, "restricting %d service(s): %s", m_cfg.restricted.size(), stream.str());
    }

    // Context signals slot

    m_signals = std::make_shared<dispatch<context_tag>>(name);
    m_signals->on<context::shutdown>(std::bind(&locator_t::on_context_shutdown, this));

    // Clustering components

    if(root.as_object().count("cluster")) {
        const auto conf = root.as_object().at("cluster").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' for cluster discovery", type);

        m_cluster = m_context.get<api::cluster_t>(type, m_context, *this, name + ":cluster", args);

        m_signals->on<context::service::exposed>(std::bind(&locator_t::on_service, this, _1, _2, 1));
        m_signals->on<context::service::removed>(std::bind(&locator_t::on_service, this, _1, _2, 0));
    }

    if(root.as_object().count("gateway")) {
        const auto conf = root.as_object().at("gateway").as_object();
        const auto type = conf.at("type", "unspecified").as_string();
        const auto args = conf.at("args", dynamic_t::object_t());

        COCAINE_LOG_INFO(m_log, "using '%s' as a cluster accessor", type);

        m_gateway = m_context.get<api::gateway_t>(type, m_context, name + ":gateway", args);
    }

    context.listen(m_signals, asio);

    // It's here to keep the reference alive.
    const auto storage = api::storage(m_context, "core");

    try {
        const auto groups = storage->find("groups", std::vector<std::string>({
            "group",
            "active"
        }));

        if(groups.empty()) return;

        std::ostringstream stream;
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", groups);

        COCAINE_LOG_INFO(m_log, "populating %d routing group(s): %s", groups.size(), stream.str());

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            std::unique_ptr<logging::log_t> log = context.log(name, {
                attribute::make("rg", *it)
            });

            m_routers.unsafe().insert({
                *it,
                continuum_t(std::move(log), storage->get<continuum_t::stored_type>("groups", *it))
            });
        }
    } catch(const storage_error_t& e) {
#if defined(HAVE_GCC48)
        std::throw_with_nested(cocaine::error_t("unable to initialize routing groups"));
#else
        throw cocaine::error_t("unable to initialize routing groups");
#endif
    }
}

locator_t::~locator_t() {
    // Empty.
}

const basic_dispatch_t&
locator_t::prototype() const {
    return *this;
}

io_service&
locator_t::asio() {
    return m_asio;
}

void
locator_t::link_node(const std::string& uuid, const std::vector<tcp::endpoint>& endpoints) {
    auto mapping = m_remotes.synchronize();

    if(!m_gateway || mapping->count(uuid) != 0) {
        return;
    }

    auto channel = std::make_shared<tcp::socket>(m_asio);

    asio::async_connect(*channel, endpoints.begin(), endpoints.end(),
        [=](const std::error_code& ec, std::vector<tcp::endpoint>::const_iterator endpoint)
    {
        auto mapping = m_remotes.synchronize();

        blackhole::scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to connect to a remote node: [%d] %s",
                ec.value(), ec.message());
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "connected to remote node via %s", *endpoint);
        }

        auto& client = mapping->operator[](uuid);

        auto  client_log = std::make_unique<logging::log_t>(*m_log, attribute::set_t({
            attribute::make("endpoint", boost::lexical_cast<std::string>(*endpoint))
        }));

        client.attach(std::move(client_log), std::make_unique<tcp::socket>(std::move(*channel)));
        client.invoke<locator::connect>(std::make_shared<remote_t>(this, uuid), m_cfg.uuid);
    });

    COCAINE_LOG_INFO(m_log, "initiating link to remote node, %llu route(s)", endpoints.size())(
        "uuid", uuid
    );
}

void
locator_t::drop_node(const std::string& uuid) {
    m_remotes.apply([&](remote_map_t& mapping) {
        if(!m_gateway || mapping.count(uuid) == 0) {
            return;
        }

        COCAINE_LOG_INFO(m_log, "stopping synchronization with remote node")(
            "uuid", uuid
        );

        mapping.erase(uuid);
    });

    // Chances are, this node is also connected back to us as a consumer.
    m_streams->erase(uuid);
}

std::string
locator_t::uuid() const {
    return m_cfg.uuid;
}

results::resolve
locator_t::on_resolve(const std::string& name, const std::string& seed) const {
    const auto remapped = m_routers.apply([&](const router_map_t& mapping) -> std::string {
        if(!mapping.count(name)) {
            return name;
        } else {
            return seed.empty() ? mapping.at(name).get() : mapping.at(name).get(seed);
        }
    });

    if(const auto provided = m_context.locate(remapped)) {
        COCAINE_LOG_DEBUG(m_log, "providing service using local actor")(
            "service", remapped
        );

        return results::resolve {
            provided.get().endpoints(),
            provided.get().prototype().version(),
            provided.get().prototype().root()
        };
    }

    if(m_gateway) {
        return m_gateway->resolve(remapped);
    } else {
        throw std::system_error(error::service_not_available);
    }
}

auto
locator_t::on_connect(const std::string& uuid) -> streamed<results::connect> {
    streamed<results::connect> stream;

    if(!m_cluster) {
        // No cluster means there are no streams.
        return stream.close();
    }

    auto mapping = m_streams.synchronize();

    scoped_attributes_t attributes(*m_log, { attribute::make("uuid", uuid) });

    if(mapping->erase(uuid)) {
        COCAINE_LOG_WARNING(m_log, "replacing stale synchronization stream for remote node");
    } else {
        COCAINE_LOG_INFO(m_log, "creating synchronization stream for remote node");
    }

    // Store the stream to synchronize future service updates with the remote node. Updates are
    // sent out on context service signals, and propagate to all nodes in the cluster.
    mapping->insert({uuid, stream});

    if(m_snapshot.empty()) {
        return stream;
    } else {
        return stream.write(results::connect{m_cfg.uuid, m_snapshot});
    }
}

void
locator_t::on_refresh(const std::vector<std::string>& groups) {
    std::map<std::string, continuum_t::stored_type> values;
    std::map<std::string, continuum_t::stored_type>::iterator lb, ub;

    try {
        const auto storage = api::storage(m_context, "core");
        const auto updated = storage->find("groups", std::vector<std::string>({"group", "active"}));

        for(auto it = groups.begin(); it != groups.end(); ++it) {
            if(std::find(updated.begin(), updated.end(), *it) == updated.end()) {
                continue;
            }

            values.insert({*it, storage->get<continuum_t::stored_type>("groups", *it)});
        }
    } catch(const storage_error_t& e) {
        throw std::system_error(error::routing_storage_error);
    }

    auto mapping = m_routers.synchronize();

    for(auto it = groups.begin(); it != groups.end(); ++it) {
        // Routing continuums can't be updated, only erased and reconstructed again. This simplifies
        // the logic greatly and doesn't impose any significant performance penalty.
        mapping->erase(*it);

        std::tie(lb, ub) = values.equal_range(*it);

        if(lb != ub) {
            std::unique_ptr<logging::log_t> log = m_context.log(m_cfg.name, {
                attribute::make("rg", *it)
            });

            mapping->insert({*it, continuum_t(std::move(log), lb->second)});
        }

        COCAINE_LOG_INFO(m_log, "%s routing group %s", lb != ub ? "updated" : "removed", *it);
    };
}

results::cluster
locator_t::on_cluster() const {
    results::cluster result;

    auto mapping = m_remotes.synchronize();

    for(auto it = mapping->begin(); it != mapping->end(); ++it) {
        result[it->first] = endpoint_traits<asio::ip::tcp::endpoint>::cast(it->second.remote_endpoint());
    }

    return result;
}

void
locator_t::on_service(const std::string& name, const results::resolve& meta, bool active) {
    if(m_cfg.restricted.count(name)) {
        return;
    }

    auto mapping = m_streams.synchronize();

    if(!mapping->empty()) {
        const auto response = results::connect { m_cfg.uuid, {{ name, meta }} };

        for(auto it = mapping->begin(); it != mapping->end();) {
            try {
                it->second.write(response);
                it++;
            } catch(...) {
                it = mapping->erase(it);
            }
        }

        COCAINE_LOG_DEBUG(m_log, "synchronized metadata with %llu remote nodes", mapping->size())(
            "service", name
        );
    }

    if(active) {
        m_snapshot[name] = meta;
    } else {
        m_snapshot.erase(name);
    }
}

void
locator_t::on_context_shutdown() {
    m_streams.apply([this](stream_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "cleaning up %d remote node stream(s)", mapping.size());
        }

        for(auto it = mapping.begin(); it != mapping.end();) {
            try {
                it->second.close();
            } catch(...) {
                // Ignore all exceptions. The runtime is being destroyed anyway.
            }

            it = mapping.erase(it);
        }
    });

    m_remotes.apply([this](remote_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(m_log, "cleaning up %d remote node client(s)", mapping.size());
        }

        // Disconnect all the remote nodes.
        mapping.clear();
    });

    COCAINE_LOG_DEBUG(m_log, "shutting down distributed components");

    m_cluster = nullptr;
    m_signals = nullptr;
}

namespace {

// Locator errors

struct locator_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.service.locator";
    }

    virtual
    auto
    message(int code) const -> std::string {
        switch(code) {
          case cocaine::error::locator_errors::service_not_available:
            return "service is not available";
          case cocaine::error::locator_errors::routing_storage_error:
            return "routing storage is unavailable";
        }

        return "cocaine.service.locator error";
    }
};

auto
locator_category() -> const std::error_category& {
    static locator_category_t instance;
    return instance;
}

} // namespace

namespace cocaine { namespace error {

auto
make_error_code(locator_errors code) -> std::error_code {
    return std::error_code(static_cast<int>(code), locator_category());
}

}} // namespace cocaine::error

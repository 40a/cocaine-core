/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include <boost/filesystem/path.hpp>
#include <boost/thread/mutex.hpp>
#include <deque>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/interfaces/storage.hpp"

namespace cocaine {

struct defaults {
    // Default engine policy.
    static const float heartbeat_timeout;
    static const float suicide_timeout;
    static const unsigned int pool_limit;
    static const unsigned int queue_limit;

    // I/O bulk size.
    static const unsigned int io_bulk_size;

    // Default slave binary.
    static const char slave[];

    // Default paths.
    static const char plugin_path[];
    static const char ipc_path[];
    static const char spool_path[];
};

struct config_t {
    config_t(const std::string& config_path);

    boost::filesystem::path config_path,
                            plugin_path,
                            ipc_path,
                            spool_path;

    typedef struct {
        std::string type;
        plugin_config_t config;
    } storage_info_t;

    typedef std::map<
        std::string,
        storage_info_t
    > storage_info_map_t;

    storage_info_map_t storages;

    typedef struct {
        std::string hostname;
    } runtime_info_t;

    runtime_info_t runtime;
};

class context_t:
    public boost::noncopyable
{
    public:
        context_t(config_t config,
                  boost::shared_ptr<logging::sink_t> sink);

        ~context_t();

        // Repository API
        // --------------

        template<class Category>
        typename category_traits<Category>::ptr_type
        get(const std::string& type,
            const typename category_traits<Category>::args_type& args)
        {
            return m_repository->get<Category>(type, args);
        }

        // Networking
        // ----------

        zmq::context_t& io();
        
        // Storage
        // -------

        template<class T>
        typename category_traits<
            storages::storage_concept<T>
        >::ptr_type
        storage(const std::string& name) {
            typedef storages::storage_concept<T> storage_type;

            config_t::storage_info_map_t::const_iterator it(
                config.storages.find(name)
            );
    
            if(it == config.storages.end()) {
                throw configuration_error_t("the '" + name + "' storage doesn't exist");
            }

            return get<storage_type>(
                it->second.type,
                typename category_traits<storage_type>::args_type(
                    it->second.config
                )
            );
        }

        // Logging
        // -------

        boost::shared_ptr<logging::logger_t>
        log(const std::string& name);

    public:
        const config_t config;

    private:
        // Initialization interlocking.
        boost::mutex m_mutex;

        // Logging.    
        boost::shared_ptr<logging::sink_t> m_sink;
        
        // Core subsystems.
        std::auto_ptr<zmq::context_t> m_io;
        std::auto_ptr<repository_t> m_repository;
};

}

#endif

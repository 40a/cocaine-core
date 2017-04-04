#include "cocaine/api/authentication.hpp"

#include <boost/optional/optional.hpp>

#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/hpack/header_definitions.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/repository/authentication.hpp"

namespace cocaine {
namespace api {

auto
authentication_t::identify(const hpack::headers_t& headers) const -> auth::identity_t {
    // Even if there is no credentials provided some authorization components may allow access.
    std::string credentials;
    if (auto header = hpack::header::find_first<hpack::headers::authorization<>>(headers)) {
        credentials = header->value();
    }

    return identify(credentials);
}

auto
authentication(context_t& context, const std::string& name, const std::string& service) ->
    std::shared_ptr<authentication_t>
{
    if (auto cfg = context.config().component_group("authentications").get(name)) {
        return context.repository().get<authentication_t>(cfg->type(), context, name, service, cfg->args());
    }

    throw error_t(error::component_not_found, "authentication component \"{}\" not found in the config", name);
}

} // namespace api
} // namespace cocaine

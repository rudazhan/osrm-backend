#include "osrm/match_parameters.hpp"
#include "osrm/nearest_parameters.hpp"
#include "osrm/route_parameters.hpp"
#include "osrm/table_parameters.hpp"
#include "osrm/trip_parameters.hpp"

#include "osrm/coordinate.hpp"
#include "osrm/engine_config.hpp"
#include "osrm/json_container.hpp"

#include "osrm/osrm.hpp"
#include "osrm/status.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <cstdlib>

int main()
{
    using namespace osrm;

    // Configure based on a .osrm base path, and no datasets in shared mem from osrm-datastore
    EngineConfig config;
    config.storage_config = {"/home/ruda/repo/CLionProjects/osrm-backend/test/cache/testbot/basic.feature/0647704af42c617a23ec23daffba28f8/831a1ecf7bf4a3771979e1da75d071ad/8_a_single_way_with_two_nodes.osrm"};
    config.use_shared_memory = false;

    // Routing machine with several services (such as Route, Table, Nearest, Trip, Match)
    const OSRM osrm{config};

    // The following shows how to use the Route service; configure this service
    RouteParameters params;

    // Route
    params.coordinates.push_back({util::FloatLongitude{1.0008990679362704}, util::FloatLatitude{1.0}});
    params.coordinates.push_back({util::FloatLongitude{1.0}, util::FloatLatitude{1.0}});
    params.annotations = true;
    params.steps = true;

    // Response is in JSON format
    json::Object result;

    // Execute routing request, this does the heavy lifting
    const auto status = osrm.Route(params, result);

    if (status == Status::Ok)
    {
        auto &routes = result.values["routes"].get<json::Array>();

        // Let's just use the first route
        auto &route = routes.values.at(0).get<json::Object>();
        const auto distance = route.values["distance"].get<json::Number>().value;
        const auto duration = route.values["duration"].get<json::Number>().value;
        auto &legs = route.values["legs"].get<json::Array>();
        auto &leg = legs.values.at(0).get<json::Object>();
        auto &annotation = leg.values["annotation"].get<json::Object>();
        auto nodes = annotation.values["nodes"].get<json::Array>().values;
//        auto vnode = std::vector<double>(nodes.size());
        std::vector<double> vnode;
        for (const auto & v : nodes) {
            vnode.push_back(v.get<json::Number>().value);
        }

        // Warn users if extract does not contain the default Berlin coordinates from above
        if (distance == 0 or duration == 0)
        {
            std::cout << "Note: distance or duration is zero. ";
            std::cout << "You are probably doing a query outside of the OSM extract.\n\n";
        }

        std::cout << "Distance: " << distance << " meter\n";
        std::cout << "Duration: " << duration << " seconds\n";
        return EXIT_SUCCESS;
    }
    else if (status == Status::Error)
    {
        const auto code = result.values["code"].get<json::String>().value;
        const auto message = result.values["message"].get<json::String>().value;

        std::cout << "Code: " << code << "\n";
        std::cout << "Message: " << code << "\n";
        return EXIT_FAILURE;
    }
}

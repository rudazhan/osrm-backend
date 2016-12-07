// Implementation as a derived class template of BasicRoutingInterface<>.
#ifndef OSRM_ROUTEDISTRIBUTION_HPP
#define OSRM_ROUTEDISTRIBUTION_HPP

#include <boost/assert.hpp>
#include <iterator>

#include "engine/routing_algorithms/routing_base.hpp"
#include "engine/search_engine_data.hpp"
#include "util/integer_range.hpp"
#include "util/timing_util.hpp"
#include "util/typedefs.hpp"

namespace osrm {
namespace engine {
namespace routing_algorithms {

template <class DataFacadeT>
class RouteDistribution final : public BasicRoutingInterface<DataFacadeT, RouteDistribution<DataFacadeT>> {
    using super = BasicRoutingInterface<DataFacadeT, RouteDistribution<DataFacadeT>>;
    using QueryHeap = SearchEngineData::QueryHeap;
    SearchEngineData &engine_working_data;

public:
    RouteDistribution(SearchEngineData &engine_working_data) : engine_working_data(engine_working_data) {}

    ~RouteDistribution() {}

    void operator()(const DataFacadeT &facade,
                    const std::vector<PhantomNodes> &phantom_nodes_vector,
                    InternalRouteResult &raw_route_data) const
    {
        // super::Search(facade, heap, heap, weight, force_loop_forward, force_loop_reverse)
        // super::UnpackPath(facade, packed_leg_begin, packed_leg_end, phantom_node_pair, unpacked_path)
    }
};
}
}
}

#endif //OSRM_ROUTEDISTRIBUTION_HPP

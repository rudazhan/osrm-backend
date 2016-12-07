// Implementation as a simple function.

#include "engine/routing_algorithms/routing_base.hpp"
#include "engine/search_engine_data.hpp"
#include "engine/engine.hpp"
#include "engine/data_watchdog.hpp"
#include "engine/datafacade/shared_memory_datafacade.hpp"
#include "util/integer_range.hpp"
#include "util/timing_util.hpp"
#include "util/typedefs.hpp"

#include <boost/assert.hpp>

#include <memory>
#include <vector>
#include <iterator>
#include <cstdlib>
#include <iostream>

using QueryHeap = osrm::engine::SearchEngineData::QueryHeap;
using DataWatchdog = osrm::engine::DataWatchdog;
using SharedMemoryDataFacade = osrm::engine::datafacade::SharedMemoryDataFacade;
using BaseDataFacade = osrm::engine::datafacade::BaseDataFacade;

//std::vector<NodeID> RouteDistribution(const ContiguousInternalMemoryDataFacadeBase &facade, const double** &trip_frequency)
//{
//    QueryHeap query_heap;
//    std::vector<NodeID> distribution;
//    auto weight = INVALID_EDGE_WEIGHT;
//
//    // get all edgeBasedNode from facade
//
//    // for each source edgeBasedNode, do forward complete Dijkstra search
//
//    // set up query heap (forward_segment_id, reverse_segment_id)
//    // prevents forcing of loops when offsets are correct.
//    constexpr bool DO_NOT_FORCE_LOOPS = false;
//    if (source_phantom.forward_segment_id.enabled)
//    {
//        query_heap.Insert(source_phantom.forward_segment_id.id,
//                            -source_phantom.GetForwardWeightPlusOffset(),
//                            source_phantom.forward_segment_id.id);
//    }
//    if (source_phantom.reverse_segment_id.enabled)
//    {
//        query_heap.Insert(source_phantom.reverse_segment_id.id,
//                            -source_phantom.GetReverseWeightPlusOffset(),
//                            source_phantom.reverse_segment_id.id);
//    }
//
//    // Search(facade, query_heap, DO_NOT_FORCE_LOOPS; weight, route)
//    while (!query_heap.Empty())
//    {
//        // RoutingStep(facade, query_heap);
//        const NodeID node = query_heap.DeleteMin();
//        if (to_weight < query_heap.GetKey(to))
//        {
//            query_heap.GetData(to).parent = node;
//            query_heap.DecreaseKey(to, to_weight);
//        }
//
//    }
//
//    // Retrieve path from heap (search_heap, target_node_id) -> path
//    std::vector<NodeID> route;
//    for (auto node = target_node_id; node != search_heap.GetData(node).parent; )
//    {
//        node = search_heap.GetData(node).parent;
//        path.emplace_back(node);
//    }
//
//    // weight with trip frequency
//    return distribution;
//}

int main()
{
//    osrm::engine::EngineConfig config;  // defaults to shared memory
//    osrm::engine::Engine engine{config};

    // use shared memory
    if (!DataWatchdog::TryConnect())
        throw osrm::util::exception("No shared memory blocks found, have you forgotten to run osrm-datastore?");
    auto watchdog = std::make_unique<DataWatchdog>();
    BOOST_ASSERT(watchdog);

    DataWatchdog::RegionsLock lock;
    std::shared_ptr<BaseDataFacade> facade;
    std::tie(lock, facade) = watchdog->GetDataFacade();
    auto shared_facade = std::dynamic_pointer_cast<SharedMemoryDataFacade>(facade);

    std::cout << shared_facade->GetNumberOfGeometryNodes() << std::endl;
    std::cout << shared_facade->GetNumberOfGeometryEdges() << std::endl;
    static auto N = shared_facade->GetNumberOfNodes();
    static auto E = shared_facade->GetNumberOfEdges();
    for (auto e = 0; e < 0; e++) {
        std::cout << shared_facade->GetEdgeData(e).id << " ";
        std::cout << shared_facade->GetTarget(e) << std::endl;
    }
    std::cout << facade->GetNumberOfNodes() << std::endl;
//    for (auto i = 0; i < 10; i++) {
//        for(auto edge : shared_facade->GetAdjacentEdgeRange(N-i-1))
//            std::cout << edge << " ";
//        std::cout << std::endl;
//    }

    return EXIT_SUCCESS;
//    auto supply_rate = RouteDistribution(facade, trip_frequency);
}

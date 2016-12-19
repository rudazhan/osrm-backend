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

using std::cout;
using std::endl;

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

// logs from Extractor::run()
//    [info] Raw input contains 9499 nodes, 2648 ways, and 1171 relations
//    [info] usable restrictions: 98                        (OSM relations)
//    [info] Importing number_of_nodes new = 8977 nodes     (OSM Nodes; OSMNodeID, NodeID) locations
//    [info]  - 0 bollard nodes, 2418 traffic lights        (relevant node tags)
//    [info]  and 11489 edges                               (OSM edges) undirected segment
//    [info] Generated 11342 nodes in edge-expanded graph   (.ebg nodes) undirected segment
//    (r-tree of ebg nodes build on-top of OSM coordinates/nodes)
//    [info] Node-based graph contains 6628 edges           (m_query_graph nodes) one-directional compressed edge
//    [info] Edge-expanded graph ...
//    [info]   contains 12548 edges                         (.ebg edges) undirected maneuver; {12548/11342 too small!}
//    [info] large component [11]=6592                      (large component index & size)
// logs from Contractor::Run()
//    [info] merged 25098 edges out of 50192                (contractor_graph before & difference) insignificant.
//    [info] [core] 0 nodes 25484 edges.                    (contractor_graph nodes and edges) both could be zero
//    [info] Serializing compacted graph of 42431 edges     (m_query_graph edges, including shortcuts)
//    [info] Writing CRC32: 1607520478                      (checksum)

    shared_facade->PrintStatistics();
    cout << "m_query_graph:" << endl;
    const auto N = shared_facade->GetNumberOfNodes();
    for (auto n = 0u; n < N; n++) {
        cout << '\t' << "Node " << n << ": ";
        auto edge_range = shared_facade->GetAdjacentEdgeRange(n);
        cout << "first edge " << edge_range.front() << ", ";
        cout << "out degree " << shared_facade->GetOutDegree(n) << endl;
        BOOST_ASSERT_MSG(edge_range.size() == shared_facade->GetOutDegree(n), "Edge range does not match out degree!");
    }
    const auto E = shared_facade->GetNumberOfEdges();
    auto counts = 0u;
    for (auto e = 0u; e < E; e++) {
        cout << '\t' << "Edge " << e << ": ";
        cout << "target " << shared_facade->GetTarget(e) << "; ";
        auto edge_data = shared_facade->GetEdgeData(e);
        if (edge_data.forward) cout << "forward ";
        if (edge_data.backward) cout << "backward ";
        cout << "weight " << edge_data.weight << "; ";
        if (edge_data.shortcut) {
            counts++;
            cout << "is shortcut, ";
            cout << "the middle node of the shortcut " << shared_facade->GetEdgeData(e).id << "; ";
        } else {
            cout << "data in compressed edge " << edge_data.id << "; ";
        }
        cout << endl;
    }
    cout << "Number of m_query_graph shortcut edges: " << counts << endl;
    cout << facade->GetNumberOfNodes() << endl;

//    auto supply_rate = RouteDistribution(facade, trip_frequency);
    return EXIT_SUCCESS;
}

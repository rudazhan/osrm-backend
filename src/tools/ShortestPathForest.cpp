//
// Created by ruda on 2/4/17.
//

#include "engine/routing_algorithms/routing_base.hpp"
#include "engine/engine.hpp"
#include "util/csv_reader.hpp"

using std::cout;
using std::endl;
using std::to_string;
using namespace std::string_literals;
using FileName = std::string;
using Weight = std::uint32_t;
static const Weight INVALID_WEIGHT = std::numeric_limits<Weight>::max();

using StaticGraph = osrm::util::StaticGraph<Weight>;
struct TreeNode {
    NodeID parent;
    TreeNode(NodeID p) : parent(p) {}
};
using BinaryHeap = osrm::util::BinaryHeap<NodeID, NodeID, Weight, TreeNode, osrm::util::UnorderedMapStorage<NodeID, unsigned >>;

StaticGraph ReadEdgeExpandedGraph(FileName ebg_filename) {
    std::ifstream ebg_file{ebg_filename};
    if (!ebg_file) throw osrm::util::exception("locations.csv not found!");
    // discard header line
    ebg_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    // format: source,target,weight,via_geometry
    csv<NodeID, NodeID, Weight, NodeID> ebg_table(ebg_file, ',');
    std::vector<StaticGraph::InputEdge> ebg;
    for (const auto& row : ebg_table) {
        auto source = std::get<0>(row);
        auto target = std::get<1>(row);
        auto weight = std::get<2>(row);
        ebg.push_back(StaticGraph::InputEdge(source, target, weight));
    }
    auto ebns = ebg.back().source + 1;
    return StaticGraph(ebns, ebg);
}

struct ShortestPathTree {
    std::vector<NodeID> parent;
    ShortestPathTree() : parent() {}
    ShortestPathTree(std::size_t size, NodeID init) : parent(std::vector<NodeID>(size, init)) {}
    std::vector<NodeID> reverse_path_to(NodeID target) {
        std::vector<NodeID> reverse_path{target};
        auto node = target;
        auto p_node = parent[target];
        while (p_node != node) {
            reverse_path.push_back(p_node);
            node = p_node;
            p_node = parent[p_node];
        }
        return reverse_path;
    }
};

class ShortestPathForest {
private:
    std::size_t m_size;
    std::vector<ShortestPathTree> trees;
public:
    ShortestPathForest(std::size_t size) : m_size(size), trees(size, ShortestPathTree()) {}
    void plant(const ShortestPathTree& tree, NodeID source) { trees.at(source) = tree; }
    std::vector<NodeID> reverse_path(NodeID source, NodeID target) { return trees.at(source).reverse_path_to(target); }
};

void WriteShortestPathTrees(const StaticGraph& ebg, NodeID begin, NodeID end) {
    auto out_file = "osrm-spt"s + to_string(begin) + "-"s + to_string(end) + ".tsv"s;
    std::ofstream spt_fs{out_file, std::ios_base::trunc};
    if (spt_fs) {
        BOOST_ASSERT_MSG(begin < end, "Source lower range shall be less than upper range!");
        const auto ebns = ebg.GetNumberOfNodes();
//        BOOST_ASSERT_MSG(end <= ebns, "Source upper range shall be no greater less than ebn size!");
        end = std::min(end, ebns);
//        ShortestPathForest spf(end - begin);
        cout << "Handling source:"; // 4s per source (now 0.3s when built)
        for (auto source = begin; source < end; source++) {
            cout << '\t' << source << endl;
            // for each source ebn, do forward complete Dijkstra search
            BinaryHeap heap(ebns);
            ShortestPathTree spt(ebns, INVALID_NODEID);
            // not actually using the data slot of BinaryHeap node array;
            for (auto n = 0u; n < ebns; n++) heap.Insert(n, INVALID_WEIGHT, INVALID_NODEID);
            heap.DecreaseKey(source, 0u);
            while (!heap.Empty()) {
                // Routing Step
                const auto min_node = heap.Min();
                auto min_weight = heap.MinKey();
                heap.DeleteMin();
                // No difference but counterintuitive, used in BasicRoutingInterface::RoutingStep().
//                const auto min_node = heap.DeleteMin();
//                auto min_weight = heap.GetKey(min_node);
                auto end_edge = ebg.EndEdges(min_node);
                for(auto edge = ebg.BeginEdges(min_node); edge < end_edge; edge++) {
                    auto to_node = ebg.GetTarget(edge);
                    if (heap.WasRemoved(to_node)) continue;
                    auto edge_weight = ebg.GetEdgeData(edge);
                    if (min_weight + edge_weight < heap.GetKey(to_node)) {
                        heap.DecreaseKey(to_node, min_weight + edge_weight);
//                        heap.GetData(to_node).parent = min_node;
                        spt.parent[to_node] = min_node;
                    }
                }
            }
//            for (auto n = 0u; n < ebns; n++) spt.parent[n] = heap.GetData(n).parent;
//            spf.plant(spt, source);
//            for (auto n = 0u; n < ebns; n++) spt_fs << '\t' << (spt.parent[n] == INVALID_NODEID ? n : spt.parent.at(n));
            for (auto n = 0u; n < ebns; n++) {
//                auto parent_n = heap.GetData(n).parent;
                auto parent_n = spt.parent[n];
                spt_fs << '\t' << (parent_n == INVALID_NODEID ? n : parent_n);
                // not safe when querying path to unreachable nodes.
//                spt_fs << '\t' << (n == source ? source : parent_n);
            }
            spt_fs << endl;
        }
        // Retrieve path from shortest path forest
//        auto route = spf.reverse_path(source, target);
    } else {
        throw osrm::util::exception("Failed to open " + out_file + " for writing." + SOURCE_REF);
    }
}

// trip frequency weighted all-to-all route distribution
// consider add vector of aggregated trip duration
//std::vector<Weight> RouteDistribution(const StaticGraph& ebg, FileName od_table) {
//    // read trip frequency matrix
//    std::vector<NodeID> distribution;
//    return distribution;
//}

NodeID parse_id(const char* str) { return static_cast<NodeID>(std::strtoul(str, nullptr, 10)); }

// run in 5min40s with 8 threads
// ./write-sources.sh | time parallel -n2 ./osrm-spf
// may join in order (Orz...)
// cat osrm-spt0-789.tsv osrm-spt789-1578.tsv osrm-spt1578-2367.tsv osrm-spt2367-3156.tsv osrm-spt3156-3945.tsv osrm-spt3945-4734.tsv osrm-spt4734-5523.tsv osrm-spt5523-6305.tsv > osrm-spt.tsv
int main(int argc, char *argv[]) {
    if (argc != 3) cout << "usage: " << argv[0] <<" <source_begin> <source_end>" << endl;
    NodeID source_begin = parse_id(argv[1]);
    NodeID source_end = parse_id(argv[2]);
    auto ebg = ReadEdgeExpandedGraph("osrm-ebg.csv"s);
    WriteShortestPathTrees(ebg, source_begin, source_end);

    return EXIT_SUCCESS;
}
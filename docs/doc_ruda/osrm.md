
# OSRM

**Project OSRM** (Open Source Routing Machine) is a shortest path routing program implementing contraction hierarchies.
Dennis Luxen was the original project lead since 2010 while he was a doctoral student at Karlsruhe Institute of Technology, Germany.
The project is currently maintained by the Mapbox Directions team: Patrick Niklaus (KIT, CS), Daniel J. Hofmann (KIT, CS, parallel programming and distributed systems), Moritz Kobitzsch (KIT, PhD in CS, alternative routes), Daniel Patterson.

Lyft potentially uses OSRM: https://github.com/Project-OSRM/osrm-backend/pull/3184#issue-185536720

## Project Structure

For a discussion on documenting the project, see issue #1434.

### Back End

osrm-backend provides executables to convert OSM data into contracted OSRM road network and to load data files into memory.
It provides an HTTP API server and a C++ interface library.

Name patterns of git branches: `master`; releases `5.4`, `5.5`, release candidates (RC) go on master; issues `debug/`, `fix/`; features `feature/`, `refactor/`.

Source code hierarchy: (C++14)

- Interface: `/include`.
    - `/extractor`, `/contractor`, `/storage`, `/server`, `/util`, `/engine`, headers grouped by namespace.
    - `/osrm`, other headers in OSRM libraries (may or may not be in secondary namespaces).
- Implementation: `/src`.
    - `/extractor`, `/contractor`, `/storage`, `/server`, bare bones of executables;
    - `/util`, common helper functions.
    - `/engine`, `/tools`, `/osrm`, executables (preprocessors, memory management, HTTP API), and C++ libraries.
    - `/benchmarks`, benchmarks for map matching and static R-tree programs.
- Library dependencies: `/third_party` (libosmium, variant by Mapbox, mason).
- Tests:
    - `/unit_test`, unit tests (extractor, util, engine, server, library) with Boost.Test;
    - `/features`, `/test`, test files and target directory for Cucumber tests;
    - `/fuzz`, fuzz tests with LLVM libFuzzer.
- Documentation: `/docs`, Markdown documentations ([API Online Docs](http://project-osrm.org/docs/v5.5.0/api/#important-interface-objects); others on GitHub wiki); `/example`, an example program using library osrm; `/profiles`, example OSRM configuration files.
- Build: `/cmake`, cmake scripts and other CMake input files; `/docker`, continuous integration with Docker.
- Helper scripts: `/scripts`, packages install, static analyzer, git bisect, OSM tags, Clang Formatter/Modernizer/Tidy, GDB printing, Node install, timing, updater, MD5 checksum.
- Project configuration files: Doxygen documentation configuration file (Doxyfile) is a custom build target (`make doc` in build directory).

Namespaces under `osrm`:

- `extractor`, OSM graph extraction
    - `guidance`, routing instructions.
    - `lookup`, lookup files for the compressed segments and intersection/barrier penalty of a maneuver.
- `contractor`, OSRM contracted graph
- `storage`, file storage `Storage` (reader interface `PopulateData()`) and shared memory `SharedMemory`.
    - `io`: general file reader.
    - `serialization`: special file readers (datasource, HSGR, edges, nodes).
- `engine`, routing engine
    - `datafacade`, data access interfaces: `BaseDataFacade` is an abstract class with pure virtual member functions only; `ContiguousInternalMemoryDataFacadeBase` implements data access in RAM (shared memory or process-local memory block) and OSRM graph representation.
    - `plugins`, service interface that handles requests.
    - `routing_algorithms`, routing algorithms (basic, shortest path, direct shortest path, alternative path, many-to-many) and map matching interface.
    - `map_matching`, classes for map matching.
    - `trip`, functions for the Trip plugin.
    - `guidance`, routing instructions.
    - `api`, service interface that creates response.
        - `json`, JSON factory (creators)
- `server`, HTTP API server
    - `api`, URL and parameter parsers.
    - `http`, HTTP request and reply.
    - `service`, OSRM service interface.
- `tools`: classes, type aliases and functions for executables; `loadGraph()`;
- `util`: graph_loader and many other headers;
    - `json`, JSON object types for OSRM responses; `cast`, type conversions;
    - `guidance`, routing instruction types and functions; `bearing`, helper functions for bearing;
    - `web_mercator`, coordinate transformation between pseudo Mercator and WGS84; `coordinate_calculation`, distances, angles, center, area, inner point;
    - `viewport`, map viewport; `vector_tile`, constants for vector tile;

OSRM back end workflow:

1. Parse: `osrm-extract` extract a road network from OSM file according to configuration, optionally generate maneuver penalty and compressed segment lookup files (so speed and penalty files can be supplied in the contraction step);
1. Prepare: `osrm-contract`, create the Contracted Hierarchy, customize/overwrite segment speeds and intersection penalties, reduce the percentage of node contraction;
1. Load: `osrm-datastore`, load or update OSRM data in shared memory (in RAM) to avoid downtime or noticeable delay, may set maximum wait until force locking;
1. Serve: `osrm-routed`, start HTTP API server on localhost:5000, loading OSRM files or shared memory;

```shell
./osrm-extract --profile ../profiles/car.lua --generate-edge-lookup bremen-latest.osm.pbf
./osrm-contract bremen-latest.osrm --core 0 --segment-speed-file updates.csv [--turn-penalty-file penalties.csv]
./osrm-datastore --max-wait=1 bremen-latest.osrm
./osrm-routed --shared-memory=yes
```

### Node API

node-osrm is a Javascript binding to the OSRM HTTP API using Node.js.
It is a convenient one-to-one routing interface.

### Front End

osrm-frontend is a web UI for OSRM based on Leaflet Routing Machine.
It provides a router view ([online demo](http://map.project-osrm.org/)), and a debug view ([online demo](http://map.project-osrm.org/debug/)).
The router view uses Mapbox or OpenStreetMap raster tiles and renders with Leaflet.
The debug view uses Mapbox vector tiles and renders with Mapbox GL JS, which shows speeds on the edge graph and highlights small components in pink.

To initiate the front end server on local host (defaults to port 9966), run `npm start`; the debug view is on endpoint `/debug`.
To use your own OSRM engine, edit `/src/leaflet_options.js` and substitute 'router.project-osrm.org' in 'https://router.project-osrm.org/route/v1' with 'localhost:5000'; also change `center` to `L.latLng(40.7500, -74.0000)`.

To debug your own OSRM graph, do the same edit for 'http://router.project-osrm.org/tile/v1/car/tile' in `/debug/index.html`.
You may also change the style option (such as bearing/rotation) to the `mapboxgl.Map` object.


## OSRM Data Models

Terminology:

- abstract graph concepts: node, edge, weight.
    - duplicate/parallel edge: directed edge with the same node pair.
    - incoming edge: an edge whose target is the current node.
    - forward search: source to target with forward edges;
    - backward search: target to source with backward edges;
    - Small/tiny components: strictly connected components of a graph with numbers of nodes less than a threshold affects nearest phantom node snapping; captures areas isolated from the rest of the road network for some reason (invalid turn restrictions, barriers, disconnected, incorrect one-ways, etc).
- unambiguous semantic concepts: location, segment, compressed segment, maneuver.
    - location: a `Coordinate`, identified by an `OSMNodeID`;
        - location of penalty: an intersection (location with over two segments, or tagged as traffic signal) or a barrier (no through traffic).
        - simple location: a location that is not a location of penalty.
    - segment: a pair or locations, has speed and duration;
    - compressed segment: a pair of locations of penalty with simple locations in between, has duration;
    - maneuver: a pair or compressed segments, incurs penalty (traffic signal, turn, u-turn);
    - geometry: undirected segment, compressed segment, or the entire network.
    - phantom node: a location matched on the original geometry.
        - forward weight: the proportional weight in forward direction of the underlying segment till the phantom node.
        - forward offset: the accumulative weight of the preceding segments in forward direction of the underlying compressed segment till the phantom node.
    - Loop: from an undirected compressed segment to itself; necessary if source and target phantom nodes are on the same one.
    - Leg: a consecutive pair of locations in a routing request, a route may consist of multiple legs.
- contraction hierarchy (CH): a node hierarchy of graphs.
    - level: an individual graph in the CH with a distinct number of nodes, indexed from 0 (the original graph) and up.
    - core network: the highest level built for the CH.
    - core node: nodes in the core network, i.e. nodes that remain uncontracted at the highest level of the CH.
    - unpacking: expand a path on the core network potentially with shortcut edges (sequence of edge-based node `NodeID`) down to a path on the original edge-expanded graph (sequence of directed compressed segment `NodeID`).

Notation:

`<u, v>` is a directed edge from node `u` to node `v`;
`<s, …, v, …, t>`: a directed path from source node `s` to target node `t`, with via node `v` and other unspecified ones.
`(u, v)` is either an undirected edge between node `u` and `v` or a segment between these two locations;
`(u, …, w)` is a compressed segment;
`c(u, v)` is the weight of edge `<u, v>`;

Data types:

- ID types (non-scoped):
    - `OSMNodeID`: strongly typed `std::uint64_t`;
    - `NodeID`, `EdgeID`, `NameID`: `std::uint32_t`;
    - `EdgeWeight`: `std::uint32_t`, in deci-second;
    - `SegmentID`: 31-bit `NodeID`, `enabled`;
    - `GeometryID`: 31-bit `NodeID`, `forward`; for directed compressed segments;
- Graph elements in `osrm::extractor`:
    - `QueryNode`: `OSMNodeID`, `FixedLongitude`, `FixedLatitude`;
    - `InternalExtractorEdge`
    - `NodeBasedEdge` for segments extracted from OSM ways: `is_split` (bidirectional segment split into two directed edges); `forward`, `backward` (is enabled); `startpoint` (is an entry point (location of penalty) to the graph).
        - `NodeBasedEdgeWithOSM` adds source and target `OSMNodeID`.
    - `EdgeBasedNode` for segments' identity in the compressed geometry and potential match of directed compressed segments: source and target locations `NodeID`, `name_id`; `component` (31-bit `id`, `is_tiny`), compressed geometry node ID, position within the compressed segment `fwd_segment_position`; forward and reverse `SegmentID` (edge-based node NodeID, is enabled) and `TravelMode`.
    - `EdgeBasedEdge`: edge `NodeID`; source and target node `NodeID`; 30-bit `EdgeWeight` (source compressed segment duration plus penalty); bool `forward`, `backward` (upward/downward graph ?).
- Graph elements in `osrm::engine`:
    - `PhantomNode`, a point matched on the compressed geometry: `location` and `input_location`; `component` (`id`, `is_tiny`), compressed geometry node ID, `fwd_segment_position` (index of component segment); forward and reverse `SegmentID`, weight, and offset, `TravelMode`; `name_id`.
        - `PhantomNodes`, a pair of `PhantomNode` for one segment: `source_phantom` and `target_phantom`.
    - `PathData`, maneuver info: the leading segment (`NameID`, `TravelMode`, `EdgeWeight`, `DatasourceID`) and turn (`LaneTupleIdPair`, via node `NodeID`, pre & post `TurnBearing`, `EntryClassID`, `TurnInstruction`);
- Graph elements in `osrm::util`:
    - `NodeBasedEdgeData`: EdgeID, reversed (each edge may have a `reversed` duplicate so every node also knows its incoming edges), startpoint, distance; name_id, roundabout, circular, `TravelMode`, `LaneDescriptionID`, `RoadClassification`;
- Generic graph templates in `osrm::util`: (Duplicate/parallel edges are allowed in both types of graphs, use `FindSmallestEdge()` for edge data uniqueness.)
    - `DynamicGraph` represents dynamic (can update) directed graph: `node` (index of the first edge, amount of edges), `edge` (index of target node, edge data);
        - In `osrm::util`: `using NodeBasedDynamicGraph = DynamicGraph<NodeBasedEdgeData>;`;
        - In `osrm::contractor::GraphContractor`: `using ContractorGraph = util::DynamicGraph<ContractorEdgeData>;`;
    - `StaticGraph` represents static graph:
        - `node_array` of `number_of_nodes` (`NodeID` to `EdgeID` of its first edge);
        - `edge_array` of `number_of_edges` (`EdgeID` pooled sequential in `NodeID` to target `NodeID` and `EdgeData` of template type);
        - In `osrm::storage`: `using QueryGraph = util::StaticGraph<contractor::QueryEdge::EdgeData>`, as the type of `osrm::engine::datafacade::ContiguousInternalMemoryDataFacadeBase::m_query_graph`;
    - `StaticRTree` for nearest neighbour queries.
        - `m_search_tree`: a vector of `TreeNode` (children `TreeIndex` (index, is_leaf) array of size `BRANCHING_FACTOR`, number of children, minimum bounding `Rectangle`) using shared memory;
        - `m_leaves`: a vector of `LeafNode` (array of `EdgeDataT` objects; number of objects, minimum bounding `Rectangle`);
        - `m_leaves_region`: leaf node file mapped to memory;
        - `m_coordinate_list`: a vector of `Coordinate`;
- Heap
    - In `osrm::util`: `BinaryHeap<>`, a min-heap.
        - `heap` is a vector of `HeapElement`: `Key` index, `Weight`;
        - `inserted_nodes` is a vector of `HeapNode`: `NodeID`, `Key`, `Weight`, `Data` (e.g. parent node);
        - `node_index` is an index (default to vector) from `NodeID` to `Key` index;
    - In `osrm::engine::SearchEngineData`: `QueryHeap` is a `BinaryHeap` with `Key = NodeID`, `Weight = int`, `Data = NodeID` (parent), and uses `std::unordered_map` for `node_index`;
    - forward/reverse heap is a `QueryHeap` of segment IDs for forward/backward search; one for `.ebg`, one for `.hsgr`.
- Query result in `osrm::engine`: `InternalRouteResult` :
    - a vector of `PhantomNodes` for query locations.
    - a vector of `PathData` for the unpacked shortest (and alternative) path, by `routing_algorithms::BasicRoutingInterface::UnpackPath()`.
    - shortest and alternative path length.
    - whether the source and target `PhantomNode` is traversed in the reverse direction of the underlying compressed segment, in the shortest and alternative paths.

`CompressedEdgeContainer` for one-way compressed geometries:

- `m_compressed_oneway_geometries`: one `OnewayEdgeBucket` (vector of `OnewayCompressedEdge`) for each one-way compressed edge, where trivial compressed edges contain only one segment (`OnewayCompressedEdge`);
- `m_compressed_geometry_index`: compressed geometry node id to the first `m_compressed_geometry_nodes` index;
- `m_compressed_geometry_nodes`: internal NodeID comprising a compressed geometry node, pooled in order of geometry id;
- `m_compressed_geometry_fwd_weights`, `m_compressed_geometry_rev_weights`: forward and reverse weights of a segment comprising a compressed geometry node, aligned with its target node;
- `m_forward_edge_id_to_zipped_index_map`, `m_reverse_edge_id_to_zipped_index_map`: one-way compressed edge EdgeID to zipped compressed geometry node id;
- `m_edge_id_to_list_index_map`: one-way compressed edge EdgeID to its `OnewayEdgeBucket` index;
- `m_free_list`: a vector available `OnewayEdgeBucket` indices in `m_edge_id_to_list_index_map`;

### Four Main Graphs

1. **Node-based graph**: geographic representation (node = location; edge = segment); `.nodes`, `.osrm`, `.edges`;
1. **Compressed geometry**: undirected compressed segments; `.geometry`;
1. **Edge-expanded graph**: logical representation (node = directed compressed segment; edge = forward maneuver); has ID for component graphs and marks small components; `.edge_segment_lookup`, `.ebg`, `.enw`, `.edge_penalties`.
1. **Contraction hierarchy** (CH): remove duplicate/parallel edges retaining the shortest, merge forward and reverse edges of equal weight into bidirectional edges, otherwise insert separate directed edges; `.hsgr`, `.core`.

Compress simple locations with two segments in total which are compatible (`NodeBasedEdgeData::IsCompatibleTo()`).

```
        forward_e1    forward_e2       |      forward_e1
     u  ----------> v -----------> w   |    u ----------> w
       <----------   <-----------      |     <----------
        reverse_e2    reverse_e1       |      reverse_e1
```

`incoming_edge = m_node_based_graph->FindEdge(node_along_road_entering, node_at_center_of_intersection)` is a compressed segment with segments `m_compressed_edge_container.GetBucketReference(incoming_edge)`

Given a request on an OSM network:

```
      s
      |
    a---b---c
            |
            d
            |--t
            e
```

`(a, b, c)` gets compressed to `(a, c)`, `(c, d, e)` gets compressed to `(c, e)`;
`path_data` will have entries for segments `(a, b)`, `(b, c)` and `(c, d)` (?);
The phantom node `s'` of `s` has `fwd_segment_position = 0`, `forward_weight = c(a, s')`, and `forward_offset = 0`;
The phantom node `t'` of `t` has `fwd_segment_position = 1`, `forward_weight = c(d, t')`, and `forward_offset = c(c, d)`;
The duration of the turn `<(a, c), (c, e)>` will be counted in the duration of `<a, c>`;
source padding `(a, s')` and target duration `(d, t')` is only captured by the phantom node, which need to be subtracted from or added to the total duration as heap weights;

### File Structures

OSRM uses STXXL, standard template library for extra large (XXL) data sets, for external memory containers.
OSRM uses `boost::filesystem::ofstream` for output file stream, documented here in their creation order.

In `osrm::extractor`, `Extractor::run()` comes in two parts/blocks.
Part one parses OSM data and configuration profile into node-based graph:

1. `.osrm.timestamp`, timestamps of the OSM extract if set;
1. STXXL external memory containers `ExtractionContainers` by `PrepareData()`:
    - `.osrm`, the node-based graph (nodes, edges) with OSM ways split into segments (node pairs): (index, start node id, target node id, direction).
    - `.osrm.restrictions`, a set of turn restrictions;
    - `.osrm.names`, name string and offset of all streets: index (prefix sum based index), str (long consecutive string).
1. `.osrm.properties` by `WriteProfileProperties()`, Lua profile properties that affect run time queries.

Part two builds edge-expanded graph:

1. `Extractor::BuildEdgeExpandedGraph()`: outputs `edge_based_node_list` (segments, `EdgeBasedNode`), `edge_based_edge_list` (`EdgeBasedEdge`), `edge_based_node_weights` (`EdgeWeight`), `node_is_startpoint` (location of penalty), `internal_to_external_node_map` (`QueryNode`); returns graph size.
    1. `GraphCompressor::Compress()` builds compressed geometry `CompressedEdgeContainer`;
    1. `EdgeBasedGraphFactory` class:
        1. `RenumberEdges()` re-generates edge ID of directed compressed segments in `NodeBasedDynamicGraph`, updates `EdgeWeight`, and returns the edge count;
        1. `GenerateEdgeExpandedNodes()` builds `edge_based_node_list`;
        1. `GenerateEdgeExpandedEdges()` builds `edge_based_edge_list` bases on compressed geometry;
            - `.osrm.edges`, segment geometry ID and guidance: a vector of `OriginalEdgeData` (of compressed geometry node; `NameID`, `TravelMode`, `EntryClassID`, `LaneDataID`, `TurnInstruction`, pre/post `TurnBearing`);
            - `.osrm.edge_segment_lookup` (if `--generate-edge-lookup`), lookup file between maneuver EdgeID and the geometry of its source compressed segment: one `SegmentHeaderBlock` (index, first_node.node_id) plus multiple `SegmentBlock` (to.node_id, segment_length, edge weight);
            - `.osrm.edge_penalties` (if `--generate-edge-lookup`), lookup file for penalty and component `OSMNodeID` of the restriction (implicit EdgeID): `PenaltyBlock` (fixed_penalty; NodeID of from, via, and to nodes);
            - `.osrm.tld`, turn lane data: size and elements of a vector of `LaneTupleIdPair`: `LaneTuple`, `LaneDescriptionID`;
    1. `.osrm.tls` by `Extractor::WriteTurnLaneData()`, turn lane descriptions: `turn_lane_offsets`, `turn_lane_masks`;
    1. `.osrm.geometry` by `CompressedEdgeContainer::SerializeInternalVector()`, compressed geometry: (number and content of) geometry indices EdgeID; (number and content of) NodeID, forward and reverse `EdgeWeight`.
    1. `.osrm.icd` by `Extractor::WriteIntersectionClassificationData()`, intersection classification data (bearing and entry): `fingerprint`, `node_based_intersection_classes`, `bearing_class_range_table`, `total_bearings`, `bearings`, `entry_classes`;
1. `.osrm.enw`, edge-based node (compressed segment) weights: a vector of `EdgeWeight`;
1. `Extractor::FindComponents()` marks small components and assigns component ID to `edge_based_node_list`, using Tarjan's strongly connected components algorithm.
1. `Extractor::BuildRTree()` for R-tree index of locations, segments:
    - `.osrm.ramIndex`, stage 1 index (index/nodes of the R-Tree), has a fixed size of 8MB;
    - `.osrm.fileIndex`, stage 2 index (leaves of the R-Tree);
1. `.osrm.nodes` by `Extractor::WriteNodeMapping()`, OSM nodes referenced in OSM ways (may be tagged as bollard or traffic_signals) and its OSRM internal NodeID (implicit):, OSMNodeID; lon, lat.
1. `.osrm.ebg` by `Extractor::WriteEdgeBasedGraph()`, edge-based graph edges (maneuvers): `fingerprint`, `number_of_used_edges`, `max_edge_id`; a sequence of `EdgeBasedEdge`.

In `osrm::contractor`, `Contractor::Run()` builds the CH:

1. `Contractor::LoadEdgeExpandedGraph()` reads `.ebg`, `.edge_segment_lookup`, `.edge_penalties`, speed and penalty files, `.datasource_names`, `.datasource_indexes`; `.fileIndex`; `.nodes`, `.geometry`;
    - `.osrm.datasource_names` by lambda `save_datastore_names()`, list of edge weight update files: filename without path or extension;
    - `.osrm.datasource_indexes` by lambda `save_datasource_indexes()`, index of edge weight profile and update files: size and content of `m_geometry_datasource` (a vector of unsigned 8-bit integer).
1. `osrm::storage::io::FileReader::DeserializeVector(node_weights)` reads segment weights `.enw`;
1. `Contractor::ReadNodeLevels()` reads contraction level cache `.level`;
1. `Contractor::ContractGraph()` takes `edge_based_edge_list` and `node_weights` and returns a core network:
    1. `GraphContractor()` merge directed edges of equal weight into bidirectional edges;
    1. `GraphContractor::Run()` builds the CH up to `core_factor` (default to 1) of nodes are contracted;
1. `.osrm.hsgr` by `Contractor::WriteContractedGraph()`, core network: `fingerprint`, `edges_crc32`, `node_array_size`, `contracted_edge_count`; `NodeArrayEntry` (`NodeID`), `EdgeArrayEntry` (`NodeID`, `QueryEdge::EdgeData`).
1. `.osrm.core` by `Contractor::WriteCoreNodeMarker()`, indicator of core nodes per `NodeID`: `unpacked_bool_flags`.
1. `.osrm.level` by `Contractor::WriteNodeLevels()`, caches (approximate) contraction levels to speed up subsequent contractor runs: `node_levels` (`float`, should be called node_priorities).

### Shared Memory Data Facade

OSRM shared memory management:

- One shared memory segment persists (key 0x00041ff2; 12 bytes);
- Two shared memory segments for one dataset (region; 888 bytes + varying bytes);
- Two regions alter for the current and previous dataset (sets of keys: 0x01041ff2, 0x02041ff2; 0x03041ff2, 0x04041ff2);
- The attached processes counters of the master segment and the current data region increment by 1 if in use (e.g. debugging) and decrement (to 0) if process stopped.

Linux shared memory utilities:

```shell
# IPC show: shm segments, output pid
# For example shm key 0x00000000 is for the GNOME session.
ipcs --shmems
ipcs --pid
# IPC remove: shared memory id
ipcrm shm <shmid>
```

In namespace `osrm::storage`, `Storage::Run()` loads OSRM files: `PopulateLayout()` read file sizes; `PopulateData()` read file data.

`osrm::engine::datafacade::ContiguousInternalMemoryDataFacadeBase`:

1. Node-based graph:
    - `m_osmnodeid_list`: internal `NodeID` one-to-one `OSMNodeID`
    - `m_coordinate_list`: internal `NodeID` one-to-one `Coordinate`;
    - `m_name_ID_list`: edge-expanded `EdgeID` into index of range table;
    - `m_name_table`: `RangeTable`; adjacent ranges;
    - `m_names_char_list`: strings of edge name;
1. Compressed geometry:
    - `m_via_geometry_list`: maneuver `EdgeID` to source compressed geometry node `GeometryID`;
    - `m_geometry_indices`: compressed geometry node `EdgeID` into index of its first `NodeID`, including the one-past-last index;
    - `m_geometry_node_list`: `NodeID` comprising a compressed geometry node, pooled sequential in `GeometryID::id` / `EdgeID`;
    - `m_geometry_fwd_weight_list`, `m_geometry_rev_weight_list`: forward / reverse `EdgeWeight` (in deci-seconds) per segment;
1. Edge-expanded graph
    - `m_static_rtree`: `SharedRTree` of .ebg nodes built on-top of OSM coordinates, which is a `StaticRTree` with edge data type `RTreeLeaf = extractor::EdgeBasedNode` and coordinate list from shared memory;
    - `m_geospatial_query`: `SharedGeospatialQuery` for edge and nearest phantom node search, `using SharedGeospatialQuery = GeospatialQuery<SharedRTree, BaseDataFacade>`;
1. Contraction hierarchy:
    - `m_is_core_node`: `NodeID` to bool;
    - `m_query_graph` core network: `QueryGraph`, a `StaticGraph` with `QueryEdge::EdgeData`, by `serialization::readHSGR()`;
        - `QueryEdge::EdgeData`: `forward`, `backward` (is direction enabled); `shortcut` (is a shortcut edge); `weight`; `NodeID` (contracted node of the shortcut, or the source directed compressed segment).
1. Miscellaneous:
    - metadata: `m_timestamp`, `m_check_sum`, `m_profile_properties`, `file_index_path`;
    - data source: `m_datasource_list`, `m_datasource_name_data`, `m_datasource_name_offsets` `m_datasource_name_lengths`;
    - guidance: `m_lane_data_id`, `m_turn_instruction_list`, `m_travel_mode_list`, `m_pre_turn_bearing`, `m_post_turn_bearing`, `m_bearing_ranges_table`, `m_bearing_values_table`, `m_bearing_class_id_table`, `m_entry_class_id_list`, `m_entry_class_table`, `m_lane_description_offsets`, `m_lane_description_masks`, `m_lane_tupel_id_pairs`;
    - (never used): `m_name_begin_indices`;


## Routing Algorithms

Routing algorithms are implemented as functors in namespace `osrm::engine::routing_algorithms`:

1. `BasicRoutingInterface`: class template with common routing functions.
1. `DirectShortestPathRouting`: Direct shortest path algorithm does not accept via locations.
    - `Search()`, `SearchWithCore()` uses forward and reverse QueryHeap in two-target/bidirectional Dijkstra routing steps; uses `RetrievePackedPathFromHeap()` to get path on CH graph.
1. `ShortestPathRouting`: This general shortest path algorithm always computes two queries for each leg (source forward/reverse -> target forward/reverse), allowing U-turn at the target phantom node.
    - This is only necessary in case of via locations where the directions of the next start node is constrained by the previous route (source forward/reverse -> target forward; source forward/reverse -> target reverse)
1. `AlternativeRouting`: Namespace scope parameters are set such that alternative are at most 15% longer (`VIAPATH_EPSILON`) and shares at most 75% with the shortest (`VIAPATH_GAMMA`); Not sure what `VIAPATH_ALPHA` is.
1. `ManyToManyRouting` is optimized to compute a weight table for a set of source and target phantom nodes, no shortest path tree generated.
    - Only uses `GetLoopWeight()` from `BasicRoutingInterface`;
    - used in `Table` for distance tables and `Trip` for duration tables.
    - Bidirectional Dijkstra: `BackwardRoutingStep()` and `ForwardRoutingStep()` reduce the heap (`DeleteMin()`), check whether to grow the heap with `StallAtNode()`, and grow the heap (`Insert()` and `DecreaseKey()`) with `RelaxOutgoingEdges()`.

In case both forward and reverse segments matching a phantom node are enabled, the shortest path search may have multiple source or target nodes with different offsets.
For example, the forward heap starts with `a(-100), b(0)` and the reverse heap starts with `c(0), d(100)`.
This is equivalent to running a bidirectional Dijkstra search on the following modified graph with edge weights `c(y, a) = -100, c(y, b) = 0, c(c, z) = 0, c(d, z) = 100`.
Since the graph contains negative edge weight, the minimum weight shall be offset to the stop criterion.

```
      a --- d
     /  \ /  \
    y    X    z
     \  / \  /
      b --- c
```


## Response JSON Object

(default namespace `osrm::engine`)

**"waypoints"** by `api::BaseAPI::MakeWaypoints()` (`osrm::util::json::Object[]`) describes all waypoints on a route.

- location: coordinates `[longitude, latitude]` on the street;
- name: name of the street;
- hint: a Base64 string as the unique internal identifier of a location on the road network, which can be used on subsequent requests until a data update;

**"routes"** by `api::RouteAPI::MakeRoute()` (`osrm::util::json::Object[]`; `guidance::Route`) represents a route through multiple waypoints.

- "distance" (Number): distance in meters traveled by the route.
- "duration" (Number): estimated travel time in seconds.
- "geometry" (String): route geometry which depends on parameters `geometries` and `overview`.
- **"legs"**: (`osrm::util::json::Object[]`; `guidance::RouteLeg`) the section of a route between waypoints.
    - "distance": distance in meters traveled by the route leg.
    - "duration": estimated travel time in seconds.
    - "steps": (`osrm::util::json::Object[]`; `guidance::RouteStep`) a maneuver (such as a turn or merge) followed by a distance of travel along a single way (if parameter `steps` is true).
        - distance: distance in meters traveled from the maneuver to the subsequent step.
        - duration: estimated travel time in seconds.
        - geometry: unsimplified geometry of the route segment, formatted according to parameter `geometries`.
        - name: The name of the way traveled along.
        - ref: reference number or code for the way, if available.
        - pronunciation: the pronunciation hint of the way name, if available.
        - destinations: destinations of the way.
        - mode: mode of transportation.
        - maneuver: a `guidance::StepManeuver` object representing the maneuver.
            - location: `[longitude, latitude]` coordinates of the turn.
            - bearing_before: the bearing of travel immediately before the maneuver.
            - bearing_after: the bearing of travel immediately after the maneuver.
            - type: the type of maneuver.
            - modifier: the direction change of the maneuver, optional.
            - exit: the number of the exit to take, optional.
        - intersections: `Intersection` objects representing cross-ways that are passed along the route step.
            - location: `[longitude, latitude]` coordinates of the intersection.
            - bearings: bearing values of roads at the intersection.
            - entry: whether entry is allowed for each road at the intersection. (possibility of entry for all available turns)
            - in: index of the entering road at the intersection.
            - out: index of the exiting road at the intersection.
            - lanes: `Lane` objects for the available turn lanes at the intersection (`indications`, types of the turn lane; `valid`, whether the lane is a valid choice in the current maneuver).
    - "summary": a string containing names of the two major roads used in the route (if parameter `steps` is true).
    - **"annotation"**: (`vector<osrm::util::json::Object>`) for nodes and consecutive node pairs along the route leg (if parameter `annotations` is true).
        - "distance": distance in meters between each consecutive pair of nodes.
        - "duration": estimated travel time in seconds between each consecutive pair of nodes.
        - "datasources": index of the data source for the speed between each consecutive pair of nodes; 0 for the default profile, other sources are supplied to `osrm-contract` with flag `--segment-speed-file`.
        - **"nodes"**: (`vector<Number>`) the OSMNodeID for each node excluding query locations.


## osrm-backend Targets

### osrm-extract

Input:

- Profile: Lua scripts as configuration files.
- OSM data file (XML or PBF);

Default profiles are available in source repo under `/profiles`, some parts of which you may want to customize:
(Default `car.lua` weights: duration along 100m primary road ~6.9s; traffic light 2s, u-turn 20s, 90° right-turn ~2.1s, 90° left-turn ~5.4s, 180° turns 7.5s)
(`testbot.lua` weights: duration along 100m primary road equals 10s; traffic light 7s, u-turn 20s, no turn penalty)

- variables:
    - speed reduction factors: `speed_reduction` (over speed limits), `side_road_speed_multiplier` (for "side_road=yes,rotary");
    - fixed penalties: `u_turn_penalty` (seconds), `traffic_signal_penalty` (seconds; for any passing route);
    - angled turn penalty parameters: `turn_penalty` (maximum turn penalty, approached at ±180°), `turn_bias` (biasing right turns to left turns, if in right-side-driving area);
- functions:
    - `node_function()` (parse access tags, barrier tags, and traffic signal),
    - `way_function()` (sets integer forward_speed and backward_speed),
    - `turn_function()` (from clockwise turn angle to turn penalty in deci-seconds; approaches 0 at 0°).

OSM data structures used in OSRM: `/taginfo.json`.

- node: `barrier`, `bollard` (no through traffic);
- way:
  - `highway` classifications: road_types (including living_street), highway_classes (broadest), motorway_types (all primary roads), link_types (all link roads);
  - speed considerations: `maxspeed` (advisory; forward, backward), `side_road`, `surface`, `tracktype`, `smoothness`, `width`;
  - `oneway`
  - `lanes` total number of traffic lanes (psv, forward, backward);
  - `turn:lanes` turn indications (forward, backward);
  - `<vehicle>:lanes` legal access per vehicle type (forward, backward);
  - `impassable`, `status`
  - `route`, `bridge`, `hov` (lanes; forward, backward), `toll`;
  - ferries and piers: `duration`; movable bridges: `capacity:car`;
  - others: `name` (pronunciation), `ref`, `junction`, `service`;
  - ignored: `area`;
- relation: `type=restriction`
- legal accessibility of nodes and ways: `motorcar`, `motor_vehicle`, `vehicle`, `access`;

Maneuver penalty computation:

1. osrm-extract (simplified, see `car.lua` for complications)
    1. compute segment duration: length / (min(highway_speed, maxspeed) * speed_reduction);
    1. compressed segment duration is the sum of component segment durations;
    1. compute turn penalty: turn_function(maneuver angle), or u_turn_penalty if the maneuver is a loop;
    1. add traffic_signal_penalty if the via location is a traffic signal;
    1. maneuver penalty is the source compressed segment duration plus intersection penalties;
1. osrm-contract
    1. overwrite segment traffic speeds where segment speed file is provided;
    1. overwrite turn penalties where turn penalty file is provided;
    1. update maneuver penalty;

Output:

- Temporary files: `.osrm`, `.osrm.restrictions`;
- Input files for `osrm-contract`: `.osrm.ebg`, `.osrm.enw`;
    - (if --generate-edge-lookup) `.osrm.edge_penalties`, `.osrm.edge_segment_lookup`;
- Input files for both `osrm-contract` and server: `.osrm.geometry`, `.osrm.nodes`;
- Server files: `.osrm.edges`, `.osrm.names`, `.osrm.ramIndex`, `.osrm.fileIndex`, `.osrm.icd`, `.osrm.properties`, `.osrm.timestamp`, `.osrm.tld`, `.osrm.tls`.

### osrm-contract

Input:

- Weight updates:
  - Segment traffic speed (flag `--segment-speed-file`): CSV files with format `from_osm_id,to_osm_id,edge_speed_in_km_h` where each line should be a restriction on a directional OSM node pair;
  - Intersection penalty (flag `--turn-penalty-file`): CSV files with format `from_osm_id,via_osm_id,to_osm_id,penalty_in_secs` where each line should be a penalty on an OSM-style relation, negative penalties are accepted;
- OSRM data files: `.osrm.ebg`, `.osrm.edge_penalties`, `.osrm.edge_segment_lookup`, `.osrm.enw`, `.osrm.geometry`, `.osrm.nodes`;
- OSRM Cache: `.osrm.level`.

Output:

- Server files: `.osrm.hsgr`, `.osrm.core`, `.osrm.datasource_names`, `.osrm.datasource_indexes`;
- Cache file: `.osrm.level`.

### osrm-routed

Input to initiate server:

- Flags: ip, port, shared memory, max table size, max num matching locations;
- OSRM server files: `.osrm.hsgr`, `.osrm.nodes`, `.osrm.edges`, `.osrm.ramIndex`, `.osrm.fileIndex`, `.osrm.names`, `.osrm.geometry`, `.osrm.icd`, `.osrm.properties`, `.osrm.timestamp`, `.osrm.tld`, `.osrm.tls`, `.osrm.core`, `.osrm.datasource_names`, `.osrm.datasource_indexes`.

HTTP request:

```
http://{server}/{service}/{version}/{profile}/{coordinates}[.{format}][?{query}]
http://{server}/tile/{version}/{profile}/tile({x},{y},{zoom}).mvt
```

- server: default to `127.0.0.1:5000`.
- service ("plugin"): `route`, `trip`, `table`, `nearest`, `match`; `tile`.
- version: protocol version implemented by the service, typically `v1`.
- profile: name of the Lua profile supplied to `osrm-extract`.
- coordinates: string with pattern `{longitude},{latitude};{longitude},{latitude}[...]` or `polyline({polyline})` (follows Google's polyline format).
- format: optional, defaults to `json`, which is the only supported option at the moment.
- query: optional, URL query string with pattern `option=value[&option=value]` where general options are
  - `bearings={bearing};{bearing}[...]` limit the search to segments with bearing in center-width interval `{value},{range}`, measured in degrees clockwise from true north;
  - `radiuses={radius};{radius}[...]` limit the search to given non-negative values in meters, defaults to 'unlimited';
  - `hints={hint};{hint}[...]` are Base64 strings which identify locations in the road network.

HTTP response:

- code: response code;
- message: optional human-readable error message;
- All other status types are service dependent.

Service-specific URL query options and HTTP response:

1. `nearest`
  - `number=1`, number of nearest segments to return.
  - `waypoints`, Waypoint objects sorted by `distance` to the input coordinate.
1. `route` (viaroute)
  - `alternative=false`, also search for an alternative route. (SearchEngine::shortestPath(), alternativePaths())
  - `steps=false`, route steps for each route leg.
  - `geometries=polyline`, format of route geometry, "polyline" are encoded from precision 5 `[latitude, longitude]` coordinates, "geojson" are GeoJSON geometry of type LineString or Point.
  - `annotations=false`, additional metadata for each coordinate along the route geometry.
  - `overview=simplified`, add overview geometry, simplified relative to the highest zoom level.
  - `continue_straight=default`, force going straight at waypoints.
  - `waypoints`, all waypoints in route order;
  - `routes`, Route objects ordered by descending recommendation rank.
1. `table`
  - `sources=all`, indices of source locations in `coordinates`.
  - `destinations=all`, indices of destination locations in `coordinates`.
  - `durations`, the duration matrix, `durations[i][j]` is the duration in seconds from waypoint i to waypoint j.
  - `sources`, Waypoint objects of all sources.
  - `destinations`, Waypoint objects of all destinations.
1. `match`
  - `steps=false`, `geometries=polyline`, `annotations=false`, `overview=simplified`, see options to service `route`.
  - `timestamps={timestamp};{timestamp}[...]`, (strictly increasing, interger) timestamps of the input locations.
  - General option `radiuses` are interpreted as standard deviations of GPS positioning accuracy, radius defaults to 5.
  - `tracepoints`, Ẁaypoint objects for all points in the trace, null for outlier, Ẁaypoint includes indices of and within each Route object.
  - `matchings`, Route objects that assemble the trace, Route includes confidence of matching.
1. `trip`
  - `steps=false`, `geometries=polyline`, `annotations=false`, `overview=simplified`, see options to service `route`.
  - `waypoints`, all Waypoints in trip order.
  - `trips`, Route objects that assemble the trace.
1. `tile`: accepts no URL query; returns a Mapbox Vector Tile (`.mvt`), which is a binary encoded blob with `Content-Type: application/x-protobuf`.

NOTE: The discovery of alternative routes depends on the contraction order of roads and reproducibility cannot be guaranteed.

Response process:

1. `SearchEngine::GetCoordinatesForNodeID()` gets lng/lat for the start phantom node, the start of each road segment, and the end phantom node; duplicates are removed;
1. `DescriptionFactory::Run()` calculates durations and bearings of each road segment, leaving out very short ones.
1. `DescriptionFactory::BuildRouteSummary()` computes total length and distance.
1. The result is formated as `osrm::util::json::Value`, which is a variant of types Null, True, False, Number, String, Array, and Object: `mapbox::util::variant<String, Number, mapbox::util::recursive_wrapper<Object>, mapbox::util::recursive_wrapper<Array>, True, False, Null>`.

### C++ libraries

Four libraries are built from this project: `osrm_extract`, `osrm_contract`, `osrm_store` (for corresponding preprocessing executables); `osrm` (interface).
Example usage of `libosrm` at `/example/example.cpp`.

Class `osrm::OSRM` represents an Open Source Routing Machine with access to services, each calls the namesake member function of class `osrm::engine::Engine`.
Class `osrm::engine::Engine` is a resource handle for `osrm::engine::DataWatchdog` (manages shared memory), or `osrm::engine::datafacade::BaseDataFacade` (reads OSRM graph data files on disk) and `osrm::storage::SharedBarriers` (thread management).
An OSRM engine can be constructed with `osrm::engine::EngineConfig`: either `use_shared_memory` or set path to OSRM data files in `storage::StorageConfig`.

Service functions: `osrm::engine::Status f(engine::api::RouteParameters, osrm::util::json::Object)`.

1. `Route()`, shortest/fastest/optimal path queries for coordinates;
1. `Trip()`, approximate the fastest round trip between coordinates using farthest insertion algorithm (traveling salesman problem);
1. `Table()`, distance tables for O/D coordinate vectors;
1. `Nearest()`, nearest street segment for coordinate;
1. `Match()`, snaps noisy coordinate traces to the road network (map matching);
1. `Tile()`, Mapbox Vector Tiles (MVT) of the routing graph geometry and a single layer containing `speed` (integer weight) and `is_small` (boolean for small component), from zoom level 12 and higher;

`osrm::engine::Engine` service functions runs query
```C++
RunQuery(const std::unique_ptr<osrm::storage::SharedBarriers> &lock, // Mutex to protect access to boolean variables (pending_update and query)
          osrm::engine::datafacade::BaseDataFacade &facade,          // Exposes all data access interfaces to the algorithms via base class ptr
          const ParameterT &parameters,                              // Parameters to the OSRM services.
          PluginT &plugin,
          ResultT &result)
```
against a plugin which is registered like
```C++
route_plugin = create<ViaRoutePlugin>(*query_data_facade,             // data access interfaces via derived class ptr: shared memory or files on disk.
                                      config.max_locations_viaroute); // maximum number of allowed locations for the services (default to unlimited).
```
`RunQuery()` calls member function `HandleRequest(parameters, result)` of a derived plugin class, all of which are based from the `BasePlugin` class and declared in namespace `osrm::engine::plugins`.

For shortest path, `ViaRoutePlugin::HandleRequest()`:
call `GetPhantomNodes()` and `SnapPhantomNodes()` inherited from `BasePlugin`;
`build_phantom_pairs` for each consecutive pair of snapped phantom nodes, enable forward and reverse directions if possible;
then call an appropriate routing algorithm in namespace `osrm::engine::routing_algorithms` such as `ShortestPathRouting<datafacade::BaseDataFacade>()`, writing to `raw_route` of type `InternalRouteResult`;
at last, call `osrm::engine::api::RouteAPI::MakeResponse()` to write internal result to a JSON object (with root elements "code", "routes", "waypoints"; see variable `reference` in `/unit_tests/library/route.cpp` test case `test_route_same_coordinates_fixture`).


## Cucumber Tests

Cucumber is a test framework with plain language syntax.
osrm-backend uses [the JavaScript implementation of Cucumber](https://github.com/cucumber/cucumber-js), and [library][/features/lib], [support code][/features/support] and [step definitions][/features/step_definitions] for the tests are all written in JavaScript.
See [wiki page](https://github.com/Project-OSRM/osrm-backend/wiki/Cucumber-Test-Suite) for a general introduction on Cucumber.

Test files are located in `/features/<feature>`, each containing many scenarios.
Tests are written using the [Gherkin syntax](https://github.com/cucumber/`ber/wiki/Gherkin).
Background section specifies for all scenarios in a feature.
Node maps (`Given the node map`) create grids defined by the pipe character `|`, which by default starts from lng/lat (1,1) and fixed coordinate increments equivalent to about 100m at equator.
Nodes are spaced on the grid with letters a-z, optional locations can be specified with numbers 0-9.
Lat/Lon tables (`Given the node locations`) define nodes with lat/lon columns, useful for tests relevant to the Mercator projection and have huge distances.
Tested routes are specified (`When I route I should get`), with nodes or positions for start and end or waypoints; expectations are specified as route (list of ways) and alternatively as time, distance or turns.
Routability tables (`Then routability should be`) specify routability tests independent of node specification.

```gherkin
Scenario: simple
    Given the profile "car"
    Given the node map
    And the nodes
    And the ways
    And the relations
    When I route I should get
```

Run tests with Node:

`npm install` to install Node dependencies; may need to install extra packages (browserify, brfs, babelify; babel-cli, uglifyjs).
`npm link` to link cucumber as executable.
`npm run test` runs all cucumber tests.
Other scripts are run from `npm run-script`: clean-test, lint, build-api-docs.

Typical usage of the cucumber executable:

```shell
cucumber                                        # all features in /features
cucumber features/car                           # all features in a specific folder
cucumber features/car/restrictions.feature:6    # only a specific feature or scenario at a specific line
cucumber --name "A single way with two nodes"   # only execute the scenarios with matching name
cucumber --tags @restrictions                   # only execute the features or scenarios with matching tags
cucumber --profile verify                       # use profile defined in cucumber.js, defaults to "default"
```

Procedures of executing one cucumber test scenario:

1. The working directory for cucumber tests is `/test`, all output is redirected to a log file under subdirectory `logs/`.
1. Create an OSM file based on the description in the test scenario, profile script which defaults to `/profiles/bicycle.lua` is copied to `profile.ini` (not found).
1. Process the OSM file through `osrm-extract` and `osrm-contract`.
1. Start OSRM server `osrm-routed` with configuration file `server.ini` (not found) and the generated OSRM data files.
1. Make an HTTP request against the OSRM server, parse results ("name" of "waypoints") and compare with the expectations.
1. Terminal shows a passed route as a green row and a failed route as two rows: the expected result in a yellow row, and the actual result in a grey row.

Try the most basic feature `/features/testbot/basic.feature`.


## Exploiting OSRM compressed graph

TPEP trips **map matching** on EPSG:3395 (Coordinate - edge-based node ID) ->
    `StaticRTree::Nearest()` for compressed geometry node ID, or directed compressed segment SegmentID (attribute trips equally in case of two-way traffic)
Aggregate trip intensity per pair of edge-based node ->
    intensity = trips / compressed segment length
**Visualize** OSRM directed compressed segments as thematic map on network: {Linear scale transaction plot}
    `.nodes` (I); `.geometry` (II); {.edge_segment_lookup (segment_length), .enw; (.edges, .ebg, .edge_penalties,)}
    GIS GUI or libraries:
        OPL -> OSM -> GeoJSON;
    osrm-frontend: `vector<EdgeWeight>` as `speed` (integer weight) returned by `Engine::Tile()`;

Data processing pipeline:

```
    download OpenStreetMap extract;
    osmupdate + osmosis (polygon & tag filter) + osmium + (JOSM manual edit);
    osrm-extract + osrm-contract + osrm-datastore;
    OSRM custom code (graphs in exchange format - OPL; map matching; columnar data with id);
    osmium + ogr2ogr;
    QGIS (csv non-geometry layer, join table), GRASS, R mapping packages, osrm-frontend;
```

<!-- Progress line -->

{Bootstrap traffic speed into a segment-speed-file (OSMNodeID, speed in km/h)} ->
    compute maximum likelihood estimate of duration table across edge-based node IDs; (use R)
    compare with subsample of OSRM duration `Table()` computed from traffic speed estimates;
Generate shortest path tree from each edge-based node (starting & ending at midpoint) ->
    New interface for trip frequency weighted all-to-all route distribution, `RouteDistribution(ebg, enw, edge_penalties, trip intensity matrix) -> vector<EdgeWeight>`:
    Uni-directional Dijkstra search on non-contracted edge-expanded graph on compressed geometry (`.ebg`, `.enw`, `.edge_penalties`);
    edge-based node weighted by trip intensity and `enw / length` gives taxi supply rate on the corresponding compressed edge.
{Alternative routes are unnecessary if source and target nodes are close to each other and end result does not need high accuracy.}
More visualizations {supply rate plot; plot of inferred demand rate; supply-demand diff/imbalance}

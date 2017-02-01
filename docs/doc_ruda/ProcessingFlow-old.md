A description of the OSRM processing flow, from importing raw OSM data, to computing routes and serving requests.

The description was created based on the following revision:

commit f557e1efb4db5009b71e20e203a99ec52e61b85d
Date:   Sat Apr 6 17:49:37 2013 +0200

# Extraction
The osrm-extract binary parses the raw OSM data, and stores it in an intermediate format.

* Settings: .lua file
* Input: Raw OSM data. (.xml or .pbf file)
* Output: Intermediate OSRM format. (.osrm and .names files)

### Parsing OSM data   
The first step is to read the raw OSM data (in either PBF or XML format) and let the LUA script parse and filter the data. One thread reads chunks of raw OSM data, while another thread processes the data. The data is first stored in temporary files using STXXL vectors.

##### Example Data
Suppose the following input data is processed by the Testbot profile:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<osm generator="osrm-test" version="0.6">
  <node id="1" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z" lon="1.0026972038088113" lat="1.0">
    <tag k="name" v="d"/>
  </node>
  <node id="2" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z" lon="1.0" lat="0.9991009320637295">
    <tag k="name" v="a"/>
  </node>
  <node id="3" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z" lon="1.0008990679362704" lat="0.9991009320637295">
    <tag k="name" v="b"/>
  </node>
  <node id="4" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z" lon="1.001798135872541" lat="0.9991009320637295">
    <tag k="name" v="c"/>
  </node>
  <node id="5" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z" lon="1.0026972038088113" lat="0.998201864127459">
    <tag k="name" v="e"/>
  </node>
  <way id="6" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z">
    <nd ref="2"/>
    <nd ref="3"/>
    <nd ref="4"/>
    <tag k="highway" v="primary"/>
    <tag k="oneway" v=""/>
    <tag k="name" v="abc"/>
  </way>
  <way id="7" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z">
    <nd ref="4"/>
    <nd ref="1"/>
    <tag k="highway" v="primary"/>
    <tag k="oneway" v="yes"/>
    <tag k="name" v="cd"/>
  </way>
  <way id="8" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z">
    <nd ref="4"/>
    <nd ref="5"/>
    <tag k="highway" v="river"/>
    <tag k="oneway" v=""/>
    <tag k="name" v="ce"/>
  </way>
  <way id="9" version="1" uid="1" user="osrm" timestamp="2000-00-00T00:00:00Z">
    <nd ref="1"/>
    <nd ref="5"/>
    <tag k="highway" v="primary"/>
    <tag k="oneway" v=""/>
    <tag k="name" v="de"/>
  </way>
</osm>
```

The data represents the following OSM nodes and ways:

```
Node map:
 |   |   |   | d |
 | a | b | c |   |
 |   |   |   | e |

Ways:
 | nodes | highway | oneway |
 | ----- | ------- | ------ |
 | abc   | primary |        |
 | cd    | primary | yes    |
 | ce    | river   |        |
 | de    | primary |        |
```

##### Parsing Nodes
[[Profiles]] contain LUA script code. Each node is passed to the LUA node_function() as an ImportNode object. LUA inspects the OSM tags, and sets member variables on the ImportNode object, to indicate whether the node is a barrier, has signal penalty, etc.

When node_function() returns, ExtractorCallbacks::nodeFunction() adds the node to an STXXL disk vector stored in ExtractionContainer:

```
 | index | id |    
 | ----- | -- |    
 | 0     | 1  | # d
 | 1     | 2  | # a
 | 2     | 3  | # b
 | 3     | 4  | # c
 | 4     | 5  | # e
```

Node that the node ids are copied from the input file, and doesn't have to be (and in real life are not) consecutive, or start from zero. The ordering of the nodes in the vector follows the ordering in the input file.
 
##### Parsing Ways
PBFParser::parseWay() first stores all ways in a local vector, with each way containing a list of the nodes it passes:

```
 | index | id | node ids |      
 | ----- | -- | -------- |      
 | 0     | 6  | 2,3,4    | # abc
 | 1     | 7  | 4,1      | # cd 
 | 2     | 8  | 4,5      | # ce 
 | 3     | 9  | 1,5      | # de 
```

As with nodes, the ids are read from the input file and can be arbitrary.

Each way is then passed to the [[Profiles]] LUA way_function() as an ExtractionWay object. LUA inspects the OSM tags, and sets member variables on the ExtractionWay object, to indicate whether the way is oneway, maxspeed, etc.

The Testbot profile returns the following ways in our example:

```
 | way  | direction     |
 | ---- | ------------- |
 | abc  | bidirectional | 
 | cd   | oneway        |
 | ce   | bidirectional |
 | de   | bidirectional |
```

ExtractorCallbacks::wayFunction() now splits the way into segments from node to node.

If an ExtractionWay has different settings in the forward/backward direction, two InternalExtractorEdges are stored for each segment, each marked as oneway. If settings are the same in both directions, a single edge is stored, marked as bidirectional.

In our example, the following edges result:

```
 | index | start node id | target node id | direction       | 
 | ----  | ------------- | -------------- | --------------- | 
 | 0     | 2             | 3              | bidirectional   | # ab / ba
 | 1     | 3             | 4              | bidirectional   | # bc / cb
 | 2     | 4             | 1              | oneway          | # cd
 | 3     | 4             | 5              | oneway          | # ce
 | 4     | 5             | 4              | oneway          | # ec
 | 6     | 1             | 5              | bidirectional   | # de / ed
```

In our example, way ce is a 'river', and has been split because the LUA script returns a different speed for each direction.

Note that the node ids in the table refer to OSM ids, not vector indexes. The edges are stored an STXXL disk vector kept in ExtractionContainers.


### Writing intermediate format
ExtractionContainers::PrepareData() now does various internal sorting and preprocessing:

* Sort nodes in usedNodeIDs
* Erase duplicate nodes in usedNodeIDs
* Sort nodes in allNodes
* Sort ways in wayStartEndVector.
* Sort restrictions in restrictionsVector by from, then fix starts.
* Sort restrictions in restrictionsVector by to, then fix ends.
* Write restrictions to an .restrictions file.
* Write used nodes to a the .osrm file:

```
 | index | id | ... |    
 | ----- | -- | --- |   
 | 0     | 1  |     | # d
 | 1     | 2  |     | # a
 | 2     | 3  |     | # b
 | 3     | 4  |     | # c
 | 4     | 5  |     | # e
```

* Sort edges by start coord, and set start coord.
* Sort edges by target coord, and set target coord, distance and weight, and convert direction to 0 (bidirectional) or 1 (oneway).
* Write (append) edges to the .osrm file:

```
 | index | start node id | target node id | direction | ... |  
 | ----- | ------------- | -------------- | --------- | --- |        
 | 0     | 4             |  1             | 1         |     | # cd     
 | 1     | 2             |  3             | 0         |     | # ab / ba
 | 2     | 3             |  4             | 0         |     | # bc / cb
 | 3     | 5             |  4             | 1         |     | # ec     
 | 4     | 1             |  5             | 0         |     | # de / ed
 | 5     | 4             |  5             | 1         |     | # ce     
```

* Write strings to a .names file:

```
 | index | str |
 | ----- | --- |
 | 0     |     | # first item is always an empty string
 | 1     | abc |
 | 2     | cd  |
 | 3     | ce  |
 | 4     | de  |
```

### Cleaning up
The temporary STXXL disk vector files are deleted.


# Preparation
The osrm-contract binary preprocesses the intermediate data, and stores the result in an internal format that allows fast route calculation.

* Settings: .lua file
* Input: Intermediate OSRM format. (.osrm and .names files)
* Output: OSRM server data. (.hgsr, .edges, .nodes, .ramIndex, .ramFiles files)

### Read intermediate format
First readBinaryOSRMGraphFromStream() (defined in GraphLoader.h) reads the .osrm file.

First nodes are read. For each node read, a _Node is created and added to a vector, and also to an ExternalNodeMap that maps from OSM id to indexes. In our example, the map looks like:

```
 | id | to |
 | -- | -- |
 | 1  | 0  |
 | 2  | 1  |
 | 3  | 2  |
 | 4  | 3  |
 | 5  | 4  |
```

Separate vectors of bollard nodes and traffic signals nodes are also build.

For each edge read, an ImportEdge object is created. The ExternalNodeMap is used to translate OSM ids to internal indexes. Source and target is swapped if needed, to ensure that the target index is always bigger than the source index. The direction is translated to two bools: forward and backward.

Our example produces:

```
 | index | source        | target         | forward | backward | ... |     
 | ----- | ------------- | -------------- | ------- | -------- | --- |     
 | 0     | 0             | 3              | false   | true     |     | # dc
 | 1     | 1             | 2              | true    | true     |     | # ab / ba
 | 2     | 2             | 3              | true    | true     |     | # bc / cb
 | 3     | 3             | 4              | false   | true     |     | # ce
 | 4     | 0             | 4              | true    | true     |     | # de / ed
 | 5     | 3             | 4              | true    | false    |     | # ce
```

Finally, duplicated edges with the same source and target nodes are removed, keeping the edge with the smallest weight, and adjusting the forward/backward setting of the edge if needed.

In our example, there are no duplicate edges, so the edge list contains the same items, but the order (and thus the indexes) change so that it's sorted by source indexes:

```
 | index | source        | target         | forward | backward | ... |          
 | ----- | ------------- | -------------- | ------- | -------- | --- |          
 | 0     | 0             | 3              | false   | true     |     | # cd     
 | 1     | 0             | 4              | true    | true     |     | # de / ed
 | 2     | 1             | 2              | true    | true     |     | # ab / ba
 | 3     | 2             | 3              | true    | true     |     | # bc / cb
 | 4     | 3             | 4              | true    | false    |     | # ce     
 | 5     | 3             | 4              | false   | true     |     | # ec     
```
 

### Create edge-expanded graph
(See [Graph Representation](https://github.com/DennisOSRM/Project-OSRM/wiki/Graph-representation))

The EdgeBasedGraphFactory constructor first converts all edges to unidirectional _NodeBasedEdges. Each bidirectional edge is converted to two unidirectional edges:

```
 | index | source node | target node | data.edgeBasedNodeID | forward | backward |      |
 | ----- | ----------- | ----------- | -------------------- | ------- | -------- |      |
 | 0     | 3           | 0           | 0                    | true    | false    | # cd |
 | 1     | 0           | 4           | 1                    | true    | true     | # de |
 | 2     | 4           | 0           | 2                    | true    | true     | # ed |
 | 3     | 1           | 2           | 3                    | true    | true     | # ab |
 | 4     | 2           | 1           | 4                    | true    | true     | # ba |
 | 5     | 2           | 3           | 5                    | true    | true     | # bc |
 | 6     | 3           | 2           | 6                    | true    | true     | # cb |
 | 7     | 3           | 4           | 7                    | true    | false    | # ce |
 | 8     | 4           | 3           | 8                    | true    | false    | # ec |
```

data.edgeBasedNodeID simply follow the index. Edges are then sorted according to source, then target:

```
 | index | source node | target node | data.edgeBasedNodeID | forward | backward |      |
 | ----- | ----------- | ----------- | -------------------- | ------- | -------- |      |
 | 0     | 0           | 4           | 1                    | true    | true     | # de |
 | 1     | 1           | 2           | 3                    | true    | true     | # ab |
 | 2     | 2           | 1           | 4                    | true    | true     | # ba |
 | 3     | 2           | 3           | 5                    | true    | true     | # bc |
 | 4     | 3           | 0           | 0                    | true    | false    | # cd |
 | 5     | 3           | 2           | 6                    | true    | true     | # cb |
 | 6     | 3           | 4           | 7                    | true    | false    | # ce |
 | 7     | 4           | 0           | 2                    | true    | true     | # ed |
 | 8     | 4           | 3           | 8                    | true    | false    | # ec |
```

Note that dc is not in the table, since cd is oneway in our original input data. (It's unclear why the forward/backward fields are needed in unidirectional edges..?)

An _NodeBasedDynamicGraph is then created, passing the edges to its constructor. A map is build that allows easy listing of edges connected to a specific node. The map consists of a vector of DynamicGraph::Nodes, each storing the index of the first edge, plus the number of edges related to the node, i.e. a range of edges from the unidirectional edge table. An extra Node is added to the table with dummy values, and we get:

```
 | index | firstEdge | edges |
 | ----- | --------- | ----- |
 | 0     | 0         | 1     |
 | 1     | 1         | 1     |
 | 2     | 2         | 2     | 
 | 3     | 4         | 3     |
 | 4     | 7         | 2     | # ex: outgoing edges from node 4 are edges 7,8 
 | 5     | 9         | 0     |
```

Also in the _NodeBasedDynamicGraph constructor, each edge is copied to a DeallocatingVector of Edges, which preserves the target and data, but not the source:

```
 | index | target | data.edgeBasedNodeID  |
 | ----- | ------ | --------------------- |
 | 0     | 4      | 1                     | # de
 | 1     | 2      | 3                     | # ab
 | 2     | 1      | 4                     | # ba
 | 3     | 3      | 5                     | # bc
 | 4     | 0      | 0                     | # cd
 | 5     | 2      | 6                     | # cb
 | 6     | 4      | 7                     | # ce
 | 7     | 0      | 2                     | # ed
 | 8     | 3      | 8                     | # ec
```

EdgeBasedGraphFactory::Run() now does the main edge expansion.

First each edge is copied to an EdgeBasedNode (which really is just either direction of a segment):

```
 | index | edge  | node   | target (node) |     
 | ----- | ----- | ------ | ------------- |     
 | 0     | 0     | 0      | 4             | # de
 | 1     | 1     | 1      | 2             | # ab
 | 2     | 2     | 2      | 1             | # ba
 | 3     | 3     | 2      | 3             | # bc
 | 4     | 4     | 3      | 0             | # cd
 | 5     | 5     | 3      | 2             | # cb
 | 6     | 6     | 3      | 4             | # ce
 | 7     | 7     | 4      | 0             | # ed
 | 8     | 8     | 4      | 3             | # ec
```

This list simply describing movements between two OSM nodes, following an OSM segments in either direction. As such it doesn't describe any turns.

Each possible movement/turn in the network is now processed, whether it's turning, going straight, doing an u-turn, etc. This is done by looping though nodes and for each:

* look at its outgoing edges
* for each edge, go to the target node
* look through the target nodes' outgoing edges

In effect, we look at all unique combinations of two connected edges - and thus moves/turns.

Moves prohibited by turn restrictions are skipped. The rest are (optionally) passed to the LUA script, which calculates a penalty for each movement depending on the angle.

A list of OriginalEdgeData is written to an .edges file (in blocks of 10000 edges), describing the moves possible from each edge based node (=node based edge):

```
 | index | edge based node index | turn instruction |               
 | ----- | ----------------------| ---------------- |               
 | 0     | 4                     | 4                | # sharp right 
 | 1     | 2                     | 0                | # no turn     
 | 2     | 1                     | 5                | # u-turn      
 | 3     | 3                     | 8                | # slight left 
 | 4     | 3                     | 2                | # slight right
 | 5     | 0                     | 4                | # sharp right 
 | 6     | 2                     | 0                | # no turn     
 | 7     | 4                     | 6                | # sharp left  
 | 8     | 0                     | 5                | # u-turn      
 | 9     | 3                     | 3                | # right       
 | 10    | 3                     | 8                | # slight left 
```

The turn instructions are enums defined in TurnInstruction.h.

A list of EdgeBasedEdges is build, representing all the possible moves in the network:

```
 | index | source | target | edge | forward | backwards | 
 | ----- | ------ | ------ | ---- | ------- | --------- | 
 | 0     | 1      | 8      | 0    | true    | false     | # de-ec
 | 1     | 3      | 5      | 1    | true    | false     | # ab-bc
 | 2     | 4      | 3      | 2    | true    | false     | # ba-ab
 | 3     | 5      | 0      | 3    | true    | false     | # bc-cd
 | 4     | 5      | 7      | 4    | true    | false     | # bc-ce
 | 5     | 0      | 1      | 5    | true    | false     | # cd-de
 | 6     | 6      | 4      | 6    | true    | false     | # cb-ba
 | 7     | 7      | 2      | 7    | true    | false     | # ce-ed
 | 8     | 2      | 1      | 8    | true    | false     | # ed-de
 | 9     | 8      | 0      | 9    | true    | false     | # ec-cd
 | 10    | 8      | 6      | 10   | true    | false     | # ec-cb
```

Note that the source and target fields references data.edgeBasedNodeID in the edge list created earlier, and not the index. When later unpacking a computed path, the ids returned match the ids in this table. 

(It's unknown why forward and backwards settings are needed here. They're always set to the same values.)


### Write Node Map
A vector of NodeInfos are written to to an .nodes file.

### Build Grid

WritableGrid::ConstructGrid() creates a data structure that enables fast lookup of nearest node from a location. (a type of Location Sensitive Hashing?) The result is written to a .ramIndex and a .fileIndex. The .ramIndex file has a fixed size of 8MB.

### Contract Edges
First the Contractor constructor converts each edges to two _ContractorEdge objects, one forward, and one backward. It's then sorted according to source and target:

```
 | index | source | target | edge | forward | backward |
 | 0     | 0      | 1      | 5    | true    | false    |
 | 1     | 0      | 5      | 3    | false   | true     |
 | 2     | 0      | 8      | 9    | false   | true     |
 | 3     | 1      | 0      | 5    | false   | true     |
 | 4     | 1      | 2      | 8    | false   | true     |
 | 5     | 1      | 8      | 0    | true    | false    |
 | 6     | 2      | 1      | 8    | true    | false    |
 | 7     | 2      | 7      | 7    | false   | true     |
 | 8     | 3      | 4      | 2    | false   | true     |
 | 9     | 3      | 5      | 1    | true    | false    |
 | 10    | 4      | 3      | 2    | true    | false    |
 | 11    | 4      | 6      | 6    | false   | true     |
 | 12    | 5      | 0      | 3    | true    | false    |
 | 13    | 5      | 3      | 1    | false   | true     |
 | 14    | 5      | 7      | 4    | true    | false    |
 | 15    | 6      | 4      | 6    | true    | false    |
 | 16    | 6      | 8      | 10   | false   | true     |
 | 17    | 7      | 2      | 7    | true    | false    |
 | 18    | 7      | 5      | 4    | false   | true     |
 | 19    | 8      | 0      | 9    | true    | false    |
 | 20    | 8      | 1      | 0    | false   | true     |
 | 21    | 8      | 6      | 10   | true    | false    |
```

Some processing is done including removing of parallel edges, merging into bidirectional edges, inserting separate edges. In our example, no change happens.


Contractor::Run() now performs the core contraction hierarchy algorithm.

GetEdges::GetEdges() is then called to get a list of the contracted edges, which is then saved to a .hsgr file.


# Route Server
osrm-routed is the routing server that handles HTTP request, and returns computed paths. It reads the prepared data produced by osrm-contract.

* Settings: command line flags (ip, port, shared memory, max table size, max num matching locations)
* Input: OSRM server data (.hgsr, .edges, .nodes, .ramIndex, .ramFiles files), incoming HTTP requests.
* Output: HTTP replies.

### Loading data
A QueryObjectsStorage object is created. The QueryObjectsStorage constructor calls readHSGRFromStream() (defined in GraphLoader.h), which reads the .hgsr file containing contracted nodes and edges.

The QueryObjectsStorage constructor then creates a NodeInformationHelpDesk object.

The NodeInformationHelpDesk constructor in turn creates a ReadOnlyGrid, which is a subclass of NNGrid.

NodeInformationHelpDesk::initNNGrid() first read node locations from the .nodes file into a list of _Coordinates:

```
 | index | lat    | lon    |
 | ----- | ---    | ---    |
 | 0     | 100000 | 100269 | # d
 | 1     | 99910  | 100000 | # a
 | 2     | 99910  | 100089 | # b
 | 3     | 99910  | 100179 | # c
 | 4     | 99820  | 100269 | # e
 | 5     | 99820  | 100269 | # it seems the last node is duplicated?
```

The.edges files is now read, and the via node, name id and turn instruction stored in three separate lists (here shown in one table):

```
 | index | via node id | name id | turn |
 | ----- | ----------- | ------- | ---- |
 | 0     | 4           | 3       | 4    | # e, sharp right
 | 1     | 2           | 1       | 0    | # b, no turn
 | 2     | 1           | 1       | 5    | # a, u-turn
 | 3     | 3           | 2       | 8    | # c, slight left
 | 4     | 3           | 3       | 2    | # c, slight right
 | 5     | 0           | 4       | 4    | # d, sharp right
```

The via node id is the original id of the OSM node that the turn happens at.

NNGrid::OpenIndexFiles() then reads the ram file into RAM.

Finally the .names file is read.

### Starting the server
Various "plugins" are now registered, however they're just internal code. Each plugin object listens to a specific http path. The ViaRoutePlugin constructor creates a SearchEngine, which in turns creates a AlternativeRouting and a ShortestPathRouting (both of which are subclasses of BasicRoutingInterface).

Server::Run() is then called, which will keep running and handle request until the process is stopped.

Connection, RequestHandler and various Plugin subclass objects are involved in handling incoming http requests. 

### Parse incoming request
The typical routing request is handled in ViaRoutePlugin::HandleRequest().

Assume a route is requested from node d to node a. The following request is send:

```
/viaroute?loc=1.0,1.0026972038088113&loc=0.9991009320637295,1.0&instructions=true&output=json
```

A RawRouteData object is created, which will store both incoming parameters and computed output path.

Incoming locations points are extracted from the request and stored in rawRoute.rawViaNodeCoordinates:

```
 | index | lat    | lon    |
 | ----- | ------ | ------ |
 | 0     | 100000 | 100269 | # from
 | 1     | 99910  | 100000 | # to
```

If via points are included in the request, the list contains more than 2 locations.

### Create Phantom Nodes
Because a route might start or anywhere on the map, a "phantom node" is now generated for each of the locations in rawRoute.rawViaNodeCoordinates. A phantom node is a point on the closest edge based node, where the route starts or ends. 

SearchEngine::FindPhantomNodeForCoordinate() is called, which passed on the call to NNGrid::FindPhantomNodeForCoordinate().

To find the closest edge based node (a segment), GetFileIndexForLatLon() is used to find a file bucket, and GetContentsOfFileBucketEnumerated() is then used to load all the edges in the bucket. ComputeDistance() is then used to find which of the edges in the bucket is closest to the location. If two bidirectional edges are on top of each other, two one with the lowest index is selected. This makes it easy to later look at the opposite edge.

Edge based nodes belonging to tiny components are ignored.

Each phantom node returned is pushed to rawRoute.segmentEndCoordinates:

```
 | index | edgeBasedNode | name id | lat    | lon    | ratio |      
 | ----- | ------------- | ------- | ------ | ------ | ----- |      
 | 0     | 0             | 2       | 100000 | 100269 | 1.0   | # end of cd 
 | 1     | 3             | 1       | 99910  | 100000 | 0.0   | # start of ab
```

Ratio indicates where on the segment the route starts; 0 is at the beginning, 1 at the end.

In our example, the start and end point are right on top of an OSM node (and the ratio is therefore 0 or 1). Since many edges might touch a node, which edge is selected is somewhat random, and depends on the ordering of edges.

Phantom nodes are now copied to a list of phantom pairs, one for each leg of the trip (section between start/via/end points:

```
 | index | start edge | target edge | .. | 
 | ----- | ---------- | ----------- | -- |
 | 0     | 0          | 3           |    |
```

Each item contains two phantom nodes, but now all the data is shown in the table above. In the example, the first (and only) item simply contains the two phantom nodes found above.

### Compute Path
If an alternative path was requested, SearchEngine::alternativePaths() is now called, otherwise, SearchEngine::shortestPath() is called. The actual routing is defined as an "()" operator. The routing algorithm uses a few inherited methods in BasicRoutingInterface, including RoutingStep().

After running the contraction hierarchy search algorithm, the computed route is available in a packed format consisting of a vector of NodeID's.

BasicRoutingInterface::UnpackPath() is called to convert this to to a vector of _PathData. This includes fetching data for each edge of the route, including the street name, turn instruction, etc:

```
 | edge id | name id | instruction     |        
 | 5       | 4       | 4 (sharp right) | # cd-de
 | 0       | 3       | 4 (sharp right) | # ce-ec
 | 10      | 1       | 8 (slight left) | # ec-cb
 | 6       | 1       | 0 (no turn)     | # cb-ba
```

### Post-process route
The final step is to construct an HTTP reply. Depending on the requested format, either a JSONDescriptor or GPXDescriptor is created.

JSONDescriptor contains two DescriptionFactories: descriptionFactory and alternateDescriptionFactory.

JSONDescriptor::Run() adds segments to the DescriptionFactory, using SearchEngine::GetCoordinatesForNodeID() to get coordinates for the start of each edge based node. First the start phantom is inserted, then the edges of the computed route, and then the end phantom node. The 1st item is deleted if the 2nd has the same coordinate, which is the case in our example. For this reason, the start phantom is not present in our example:

```
 | index | lat    | lon    | name id | duration | bearing | turn | necessary |
 | ----- | ------ | ------ | ------- | -------- | ------- | ---- | --------- |
 | 0     | 100000 | 100269 | 4       | 0        | 0       | 10   | true      | # d on de (from route)
 | 1     | 99820  | 100269 | 3       | 0        | 0       | 4    | true      | # e on ce (from route)
 | 2     | 99910  | 100179 | 1       | 0        | 0       | 8    | true      | # c on abc (from route)
 | 3     | 99910  | 100089 | 1       | 0        | 0       | 0    | false     | # b on abc (from route)
 | 4     | 99910  | 100000 | 1       | 0        | 0       | 0    | true      | # a on abc (from phantom node)
```

JSONDescriptor::Run() then call DescriptionFactory::Run() which does some post-processing and filtering of the instructions list. First durations are calculated, then very short segments are filtered out, and then bearings are calculated, and we arrive at:

```
 | index | lat        | lon         | name id | duration | bearing | turn | necessary |
 | ----- | ---------- | ----------- | ------- | -------- | ------- | ---- | --------- |
 | 0     | 100000     | 100269      | 4       | 200.207  | 180     | 10   | true      | # d on de
 | 1     | 99820      | 100269      | 3       | 141.557  | 315.004 | 4    | true      | # e on ce
 | 2     | 99910      | 100179      | 1       | 199.065  | 270     | 8    | true      | # c on abc
 | 3     | 99910      | 100089      | 1       | 100.088  | 270     | 0    | true      | # b on abc
 | 4     | 99910      | 100000      | 1       | 98.9764  | 0       | 0    | true      | # a on abc
```

The series of of locations represent a polyline, covering all the nodes we pass on the computed route. A Douglas-Peucker algorithm is run to build a generalized (simplified) geometry, taking the zoom level into account.

JSONDescriptor then calls DescriptionFactory::BuildRouteSummary() to compute total length and distance.

### Format reply

JSONDescriptor manually formats the results as JSON by concatenating strings:

```json
{
    "version": 0.3,
    "status": 0,
    "status_message": "Found route between points",
    "route_geometry": "_ibEyybEfJ?sDrD?rD?pD",
    "route_instructions": [
        ["10", "de", 200, 0, 20, "200m", "S", 180],
        ["4", "ce", 141, 1, 14, "141m", "NW", 315],
        ["8", "abc", 199, 2, 10, "199m", "W", 270],
        ["15", "", 0, 4, 0, "", "N", 0.0]
    ],
    "route_summary": {
        "total_distance": 541,
        "total_time": 55,
        "start_point": "cd",
        "end_point": "abc"
    },
    "alternative_geometries": [],
    "alternative_instructions": [],
    "alternative_summaries": [],
    "route_name": ["de", "abc"],
    "alternative_names": [
        ["", ""]
    ],
    "via_points": [
        [1.00000, 1.00269],
        [0.99910, 1.00000]
    ],
    "hint_data": {
        "checksum": 392941890,
        "locations": ["AAAAAAIAAACOAAAA____fwAAAAAAAPA_oIYBAK2HAQA", "AwAAAAEAAAAAAAAAYwAAAAAAAAAAAAAARoYBAKCGAQD"]
    },
    "transactionId": "OSRM Routing Engine JSON Descriptor (v0.3)"
}
```

ViaRoutePlugin formats the HTTP reply, which includes the JSON as well as HTTP headers
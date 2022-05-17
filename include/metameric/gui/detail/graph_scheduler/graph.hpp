#pragma once

#include <algorithm>
#include <deque>
#include <list>
#include <memory>
#include <string>
#include <ranges>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>

namespace met::detail {
  template <typename KeyTy  = std::string>
  struct AbstractDirectedGraphNode {
    KeyTy _key;

    AbstractDirectedGraphNode(const KeyTy &key)
    : _key(key) {}

    virtual void eval() = 0;

    KeyTy key() const { return _key; }
  };

  template <typename KeyTy  = std::string>
  struct AbstractDirectedGraphEdge {
    KeyTy _a_key, _b_key;

    AbstractDirectedGraphEdge(const KeyTy &a_key, const KeyTy &b_key)
    : _a_key(a_key), _b_key(b_key) {}

    virtual void eval() = 0;
    KeyTy a_key() const { return _a_key; }
    KeyTy b_key() const { return _b_key; }
  };

  template <typename KeyTy = std::string>
  struct DirectedGraphNode : public AbstractDirectedGraphNode<KeyTy> {
    DirectedGraphNode(const KeyTy &key)
    : AbstractDirectedGraphNode<KeyTy>(key) { }

    void eval() override { }
  };

  template <typename KeyTy = std::string>
  struct DirectedGraphEdge : public AbstractDirectedGraphEdge<KeyTy> {
    DirectedGraphEdge(const KeyTy &a_key, const KeyTy &b_key)
    : AbstractDirectedGraphEdge<KeyTy>(a_key, b_key) {}

    void eval() override { }
  };

  template <typename KeyTy  = std::string, 
            typename NodeTy = AbstractDirectedGraphNode<KeyTy>,
            typename EdgeTy = AbstractDirectedGraphEdge<KeyTy>>
  struct DirectedGraph {

    using KeyType  = KeyTy;
    using NodeType = std::unique_ptr<NodeTy>;
    using EdgeType = std::unique_ptr<EdgeTy>;
    using NodeRefr = NodeTy *; // raw ptr instead of std::reference_wrapper<NodeType>;
    using EdgeRefr = EdgeTy *; // raw ptr instead of std::reference_wrapper<EdgeType>;

    std::unordered_map<KeyType, NodeType>            _nodes;
    std::unordered_map<KeyType, std::list<EdgeType>> _edges;
    std::unordered_map<KeyType, std::list<KeyType>>  _adjac;
    std::unordered_map<KeyType, std::list<KeyType>>  _adjac_inv;
    std::vector<NodeRefr>                            _bfs_nodes;
    std::vector<std::list<EdgeRefr>>                 _bfs_edges;

    template <typename _NodeTy, typename... Args>
    void create_node(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<NodeTy, _NodeTy>);

      _nodes.insert({ key, std::make_unique<_NodeTy>(key, args...) });
      _adjac.insert({ key, { } });
      _adjac_inv.insert({ key, { } });
      _edges.insert({ key, { } });
    }

    template <typename _NodeTy>
    void insert_node(const KeyType &key, _NodeTy &&node) {
      static_assert(std::is_base_of_v<NodeTy, _NodeTy>);

      _nodes.insert({ key, std::make_unique<_NodeTy>(std::move(node)) });
      _adjac.insert({ key, { } });
      _adjac_inv.insert({ key, { } });
      _edges.insert({ key, { } });
    }

    void create_edge(const KeyType &a, const KeyType &b) {
      // _edges
      _adjac[a].push_back(b);
      _adjac_inv[b].push_back(a);
    }

    template <typename _EdgeTy, typename... Args>
    void create_edge(const KeyType &a, const KeyType &b, Args... args) {
      static_assert(std::is_base_of_v<EdgeTy, _EdgeTy>);

      _edges[a].push_back(std::make_unique<_EdgeTy>(a, b, args...));
      _adjac[a].push_back(b);
      _adjac_inv[b].push_back(a);
    }

    void compile() {
      std::deque<KeyType> queue;
      std::unordered_map<KeyType, bool> visits;

      // Define lambda shorthands
      constexpr auto key_get = [](auto &v) { return [&v](const KeyType &key) { return v[key]; }; };
      constexpr auto is_false = [](const bool &b) { return !b; };
      auto is_entry = [&](const KeyType &key) { return _adjac_inv[key].empty(); };

      // Find entry nodes (those with no edges into them)
      auto entries = std::views::keys(_nodes) | std::views::filter(is_entry);

      // Assemble initial traveral queue based on entry nodes
      std::ranges::copy(entries, std::back_inserter(queue));

      // Remove original BFS traversal order
      _bfs_nodes.clear();
      _bfs_edges.clear();
      _bfs_nodes.reserve(_nodes.size());
      _bfs_edges.reserve(_nodes.size());

      // Recompute BFS traversal order
      while (!queue.empty()) {
        // Pop new node to visit from queue
        KeyType key = queue.front();
        queue.pop_front();

        // Guard if this node has been visited
        if (visits[key])
          continue;

        // Guard if not all prior nodes have been visited
        const auto visit_list = _adjac_inv[key] | std::views::transform(key_get(visits));
        if (std::ranges::any_of(visit_list, is_false))
          continue;

        // Push connected nodes on queue and flag node as visited
        std::ranges::copy(_adjac[key], std::back_insert_iterator(queue));
        visits[key] = true;

        // Store reference to node in traversal order
        _bfs_nodes.push_back(_nodes[key].get());
      }
    }

    void traverse() {
      fmt::print("traverse_bfs_map()...\n");
      std::ranges::for_each(_bfs_nodes, [&](auto &r) { 
        fmt::print("\t{}\n", r->key());
        r->eval();
      });
    }
  };
  
  void graph_example() {
    detail::DirectedGraph graph;

    /* map-based approach */
    using Node = detail::DirectedGraphNode<std::string>;
    using Edge = detail::DirectedGraphEdge<std::string>;

    graph.create_node<Node>("node_0");
    graph.create_node<Node>("node_1");
    graph.create_node<Node>("node_2");
    graph.create_node<Node>("node_3");

    graph.create_edge<Edge>("node_0", "node_1");
    graph.create_edge<Edge>("node_1", "node_3");
    graph.create_edge<Edge>("node_0", "node_3");
    graph.create_edge<Edge>("node_3", "node_2");
    
    graph.compile();
    graph.traverse();
  }
} // namespace met::detail
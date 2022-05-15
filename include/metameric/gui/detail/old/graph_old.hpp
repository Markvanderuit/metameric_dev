#pragma once

#include <functional>
#include <map>
#include <string>
#include <fmt/core.h>

using uint = unsigned int;

// For enum class T, declare bitflag operators and has_flag(T, T) boolean operator
#define met_declare_bitflag(T)\
  constexpr T operator~(T a) { return (T) (~ (uint) a); }\
  constexpr T operator|(T a, T b) { return (T) ((uint) a | (uint) b); }\
  constexpr T operator&(T a, T b) { return (T) ((uint) a & (uint) b); }\
  constexpr T operator^(T a, T b) { return (T) ((uint) a ^ (uint) b); }\
  constexpr T& operator|=(T &a, T b) { return a = a | b; }\
  constexpr T& operator&=(T &a, T b) { return a = a & b; }\
  constexpr T& operator^=(T &a, T b) { return a = a ^ b; }\
  constexpr bool has_flag(T flags, T t) { return (uint) (flags & t) != 0u; }

namespace met {
  enum class ResourceStateFlags : uint {
    eNone                 = 0x000u,
    eIsReferenced         = 0x001u,
    eIsInitialized        = 0x002u
  };
  met_declare_bitflag(ResourceStateFlags)

  enum class ResourceBarrierFlags {
    eAll  = 0x001
  };
  met_declare_bitflag(ResourceBarrierFlags)

  struct VirtualResource {
    ResourceStateFlags _state_flags = ResourceStateFlags::eNone;

    bool is_complete() const {
      return has_flag(_state_flags, ResourceStateFlags::eIsInitialized)
          && has_flag(_state_flags, ResourceStateFlags::eIsReferenced);
    }

    virtual bool is_concrete() const { return false; }
  };

  template <typename T>
  struct ConcreteResource : public VirtualResource { 
    T& get() { return t; }

    virtual bool is_concrete() const override { return true; }

  private:
    T t = { };
  };


  /**
   * Application resource manager
   */
  class ResourceManager {
    size_t _id_head;
    std::map<size_t, std::string> _resource_id_to_name;
    std::map<std::string, size_t> _resource_name_to_id;
    std::map<size_t, std::unique_ptr<VirtualResource>> _resources; 

    size_t instantiate_virtual(const std::string &name) {
      if (auto iter = _resource_name_to_id.find(name); 
               iter != _resource_name_to_id.end()) {
        return iter->second;
      }

      size_t id = _id_head++; // Probably a bad idea

      _resource_id_to_name.emplace(id, name);
      _resource_name_to_id.emplace(name, id);
      _resources.emplace(id, std::make_unique<VirtualResource>());

      return id;
    }

    template <typename T>
    size_t instantiate_real(const std::string &name, T &&t) {
      size_t id = instantiate_virtual(name);
      auto flags = _resources[id]->_state_flags;
      _resources[id] = std::make_unique<ConcreteResource>({ flags }, t);
      return id;
    }

  public:
    ResourceManager() : _id_head(0) { }

    void reference_resource(const std::string &name) {
      size_t id = instantiate_virtual(name);
      _resources.at(id)->_state_flags |= ResourceStateFlags::eIsReferenced;
    }

    template <typename T>
    void initialize_resource(const std::string &name, T &&t) {
      size_t id = instantiate_real<T>(name, t);
      _resources.at(id)->_state_flags |= ResourceStateFlags::eIsInitialized;
    }

    void reset_resource(const std::string &name) {
      size_t id = instantiate_virtual(name);
      _resources.at(id) = std::make_unique<VirtualResource>();
    }

    void destroy_resource(const std::string &name) {
      size_t id = _resource_name_to_id[name];
      _resources.erase(id);
      _resource_id_to_name.erase(id);
      _resource_name_to_id.erase(name);
    }

    template <typename T>
    T & get_resource(const std::string &name) const {
      size_t id = _resource_name_to_id[name];
      return dynamic_cast<std::unique_ptr<ConcreteResource<T>>>(_resources.at(id))->get();
    }

    void reset() {
      for (auto &[key, item] : _resources) {
        item = std::make_unique<VirtualResource>();
      }
    }

    bool is_complete() const {
      for (auto &[key, item] : _resources) {
        if (!item->is_complete()) {
           // reference is either uninitialized, or unreferenced
           return false;
         }
      }
      return true;
    }

    void merge(ResourceManager &o) {
      _resource_id_to_name.merge(o._resource_id_to_name);
      _resource_name_to_id.merge(o._resource_name_to_id);
      _resources.merge(o._resources);
    }
  };

  template <typename T>
  class GraphResource {

  };

  template <typename T, typename TInfo>
  struct GraphResourceInfo {

  };

  struct GraphNodeBuilder {
    using EdgeBuffer = std::pair<std::string, std::string>;
    std::vector<EdgeBuffer> _prev, _next;
    std::string _node_name;

    template <typename T, typename TInfo>
    void create_and_write(const std::string &output_name, TInfo resource_info) {
      GraphResourceInfo<T, TInfo> resource;
      
      // Step 1. Initialize actual resource [i]
      // Step 2. Initialize virtual resource [node + output, linked to i]
    }
    
    // Obtain a specific input from a given named resource
    void read(const std::string &node_name, 
              const std::string &output_name) {
      _prev.push_back({ node_name, output_name });

      // Step 1. Initialize virtual resource [node + output, linked to i]
    }

    void read_and_write(const std::string &node_name, 
                        const std::string &prev_output_name,
                        const std::string &output_name) {
      _prev.push_back({ node_name, prev_output_name });
      _next.push_back({ node_name, output_name });

      // Step 1. Initialize virtual resource [node1 + output1, linked to i]
      // Step 2. Initialize virtual resource [node2 + output2, linked to i]
    }
  };

  struct GraphNodeData {
    template <typename T>
    std::unique_ptr<GraphResource<T>>& get(std::string_view name) {
      return nullptr;
    }
  };

  // FWD
  struct VirtualNode;
  struct ConcreteNode;

  struct VirtualNode {
    
  };

  struct ConcreteNode : public VirtualNode {

  };

  struct GraphBuilder {    
    std::unordered_map<std::string, std::unique_ptr<VirtualNode>> _virtual_tasks;
    std::unordered_map<std::string, std::unique_ptr<VirtualNode>> _virtual_resources;

    using TaskBuildFunction = std::function<void(GraphNodeBuilder &)>;
    using TaskRunFunction = std::function<void(GraphNodeData &)>;
    
    void add_task(const std::string &name,
                  TaskBuildFunction build_function,
                  TaskRunFunction run_function) {
      // 1. Set up GraphNodeBuilder and run build_function(...) with it
      // 2. Consume GraphNodeBuilder; insert virtual nodes/resources into graph data

      GraphNodeBuilder builder; // Construct using resource backend
      build_function(builder);
    }

    void remove_task(const std::string &name) {
      // ...
    }

    void compile() {

    }

    void run() {

    }
  };



  /**
   * Application graph manager.
   */
  /* class GraphManager {
    enum class NodeType { eResource, eTask  };
    struct AbstractNode;
    using Edge = std::pair<AbstractNode *, AbstractNode *>;
    struct AbstractNode { std::vector<Edge> in, out; };
    template <NodeType T>
    struct Node : public AbstractNode { int id; };

  private:
    ResourceManager _resource_manager;

  public:
    void create_task_node();
    void destroy_task_node();

    void create_resource_node();
    void destroy_resource_node();

    void compile();
    
    void run();
  }; */
} // namespace met
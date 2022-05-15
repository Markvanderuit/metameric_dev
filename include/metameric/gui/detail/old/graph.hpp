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
  
  /**
   * Wrapper object structure to store and hold a collection of potentially
   * uninitialized but constructible objects.
   */
  template <typename T>
  class VirtualObjectMap {
    struct VirtualObject {
      virtual bool is_concrete() const { return false; }
    };

    template <typename T>
    struct ConcreteObject {
      T t;

      virtual bool is_concrete() const override { return true; }
    };

    size_t _id_head;
    std::unordered_map<size_t, std::string> _object_id_to_name; // unnecessary
    std::unordered_map<std::string, size_t> _object_name_to_id;
    std::map<size_t, std::unique_ptr<VirtualObject>> _objects; 
    
    size_t ensure_virtual(const std::string &name) {
      if (auto iter = _object_name_to_id.find(name); 
               iter != _object_name_to_id.end()) {
        return iter->second; // Exists
      }

      size_t id = _id_head++; // Probably a bad idea

      _object_id_to_name.emplace(id, name);
      _object_name_to_id.emplace(name, id);
      _objects.emplace(id, std::make_unique<VirtualObject>());

      return id;
    }

    template <typename T>
    size_t ensure_concrete(const std::string &name, T &&t) {
      size_t id = ensure_virtual(name);
      _objects[id] = std::make_unique<ConcreteObject<T>>(_objects[id]->to_concrete<T>(t));
      return id;
    }

  public:
    VirtualObjectMap()
    : _id_head(0) { }

    template <typename T>
    void set(const std::string &name, T &&t) {
      size_t id = ensure_concrete(name, t);
    }

    bool contains(const std::string &name) {
      return _object_name_to_id.contains(name);
    }

    void touch(const std::string &name) {
      ensure_virtual(name);
    }

    void clear(const std::string &name) {
      size_t id = ensure_virtual(name);
      _objects[id] = std::make_unique<VirtualObject>();
    }

    void erase(const std::string &name) {
      size_t id = ensure_virtual(name);
      _objects.erase(id);
      _object_id_to_name.erase(id);
      _object_name_to_id.erase(name);
    }

    /* std::unique_ptr<Virtual> & at_virtual(const std::string &name) {
      size_t id = ensure_virtual(name);
      return _objects[id];
    } */

    template <typename T>
    T & at_concrete(const std::string &name) {
      size_t id = ensure_virtual(name);
      return dynamic_pointer_cast<std::unique_ptr<ConcreteObject<T>>&>(_objects[id])->t;
    }
  };

  struct VirtualResource;
  template <typename T>
  struct ConcreteResource;

  struct VirtualTask;
  template <typename T>
  struct ConcreteTask;

  struct GraphTaskBuilder {
    using Prev = std::pair<std::string, std::string>;
    using Next = std::pair<std::string, std::string>;

    std::vector<Prev> _prev;
    std::vector<Next> _next;
  };

  struct GraphBuilder {
    VirtualObjectMap<VirtualResource> _resource_map;
    VirtualObjectMap<VirtualTask> _task_map;

    using TaskBuildFunction = std::function<void(void)>;
    using TaskRunFunction = std::function<void(void)>;

    void add_task_node(const std::string &name,
                       TaskBuildFunction build_function,
                       TaskRunFunction run_function) {
      _task_map.touch(name);
      
      build_function(); // insert builder object
      // ...
    }

  };
  
} // namespace met
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace met {
  namespace detail {
    template <typename T>
    struct ConcreteResource; // fwd

    struct VirtualResource {
      template <typename T>
      T & value_as() {
        return ((ConcreteResource<T>*) this)->_value;
      }
    };
    
    template <typename T>
    struct ConcreteResource : VirtualResource {
      // Forwarding constructor
      ConcreteResource(T &&value) 
      : _value(std::move(value)) 
      { }

      T _value;
    };

    class ResourceHandle {
      using PtrType = std::shared_ptr<detail::VirtualResource>;

      PtrType _resource;
    public:
      ResourceHandle(const PtrType &resource)
      : _resource(resource)
      { }

      template <typename T>
      T & value_as() {
        return _resource->value_as<T>;
      }
    };
  } // namepsace detail

  class TaskResources {
    using KeyType = std::string;
    using PtrType = std::shared_ptr<detail::VirtualResource>;

    template <typename T>
    PtrType constr_ptr(T && object) { 
      return std::make_shared<detail::ConcreteResource<T>>(std::move(object));
    }

    std::unordered_map<KeyType, PtrType> _resources;

  public:
    /* Move into */

    void insert(const KeyType &resource_name, PtrType &&object) {
      _resources.insert({ resource_name, std::move(object) });
    }

    template <typename T>
    void insert(const KeyType &resource_name, T &&object) {
      _resources.insert({ resource_name, constr_ptr(std::move(object)) });
    }

    /* Create/remove */

    template <typename T, typename TInfo>
    void create(const KeyType &resource_name, TInfo info_object) {
      _resources.insert({ resource_name, constr_ptr(std::move(T(info_object))) });
    }

    void erase(const KeyType &resource_name) {
      _resources.erase(std::string(resource_name));
    }

    /* Accessors */

    template <typename T>
    T & at(const KeyType &resource_name) const {
      return _resources.at(resource_name)->value_as<T>();
    }
  };
  
  using ReadHandle    = std::pair<std::string, std::string>;
  using WriteHandle   = std::pair<ReadHandle, std::string>;

  struct ResourceHolder {
    TaskResources resources;
    std::list<ReadHandle> reads;
    std::list<WriteHandle> writes;
  };

  /* class ApplicationResources {
    std::unordered_map<std::string, TaskResources> _resources;

  public:
    void emplace(const std::string &task_name, TaskResources &&resources) {
      _resources.emplace(task_name, resources);
    }

    void erase(const std::string &task_name) {
      _resources.erase(task_name);
    }

    TaskResources & at(const std::string &task_name) {
      return _resources.at(task_name);
    }
  }; */
} // namespace met
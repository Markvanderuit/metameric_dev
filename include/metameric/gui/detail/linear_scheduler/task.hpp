#pragma once

#include <list>
#include <string>
#include <memory>
#include <unordered_map>
#include <metameric/gui/detail/linear_scheduler/resource.hpp>

namespace met::detail {
  // fwd
  class TaskInitInfo;
  class TaskEvalInfo;

  /**
   * Abstract base class for application tasks.
   */
  class AbstractTask {
    std::string _name;

  public:
    const std::string &name() const { return _name; } 
    void set_name(const std::string &name) { _name = name; } 

    AbstractTask(const std::string &name) : _name(name) { }

    // Override and implement
    virtual void init(TaskInitInfo &) = 0;
    virtual void eval(TaskEvalInfo &) = 0;
  };

  /**
   * Abstract base class that consumes application tasks and updates the environment
   * in which they exist.
   */
  class AbstractTaskInfo {
  protected:
    using KeyType         = std::string;
    using RsrcType        = std::unique_ptr<AbstractResource>;
    using TaskType        = std::unique_ptr<AbstractTask>;
    using RsrcMapType     = std::unordered_map<KeyType, RsrcType>;
    using ApplRsrcMapType = std::unordered_map<KeyType, RsrcMapType>;

    RsrcMapType     &_task_resource_registry;
    ApplRsrcMapType &_appl_resource_registry;

    AbstractTaskInfo(ApplRsrcMapType &appl_resource_registry, const AbstractTask &task)
    : _appl_resource_registry(appl_resource_registry),
      _task_resource_registry(appl_resource_registry[task.name()]) { };
    
  public:

    /* Public data registries */

    std::unordered_map<KeyType, RsrcType>     add_resource_registry;
    std::list<std::pair<KeyType, TaskType>>   add_task_registry;
    std::list<KeyType>                        remove_resource_registry;
    std::list<KeyType>                        remove_task_registry;

    /* Create, add, remove resources */

    template <typename Ty, typename InfoTy>
    void emplace_resource(const KeyType &key, InfoTy info) {
      add_resource_registry.emplace(key, std::make_unique<detail::Resource<Ty>>(Ty(info)));
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      add_resource_registry.emplace(key, std::make_unique<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void remove_resource(const KeyType &key) {
      remove_resource_registry.push_back(key);
    }

    /* Create, add, remove secondary tasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_unique<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_unique<Ty>(std::move(task)));
    }
    
    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_unique<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_unique<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key) {
      remove_task_registry.push_back(key);
    }

    /* Access existing resources */

    template <typename T>
    T & get_resource(const KeyType &key) {
      return _task_resource_registry.at(key)->get<T>();
    }

    template <typename T>
    T & get_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _appl_resource_registry.at(task_key).at(rsrc_key)->get<T>();
    }
  };

  class TaskInitInfo : public AbstractTaskInfo {
  public:
    TaskInitInfo(ApplRsrcMapType &appl_resource_registry, AbstractTask &task)
    : AbstractTaskInfo(appl_resource_registry, task) {
      // By consuming the task in this object, we initialize the task
      task.init(*this);
    }
  };

  class TaskEvalInfo : public AbstractTaskInfo {
  public:
    TaskEvalInfo(ApplRsrcMapType &appl_resource_registry, AbstractTask &task)
    : AbstractTaskInfo(appl_resource_registry, task) {
      // By consuming the task in this object, we eval/run the task
      task.eval(*this);
    }
  };
} // met::detail
#pragma once

#include <memory>
#include <list>
#include <string>
#include <unordered_map>
#include <metameric/core/math.hpp>
#include <metameric/core/detail/scheduler_resource.hpp>

namespace met::detail {
  // fwd
  class TaskInitInfo;
  class TaskEvalInfo;
  class TaskDstrInfo;

  // Global key for resources with no managing task
  const std::string resource_global_key = "global";

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
    virtual void init(TaskInitInfo &) { };
    virtual void eval(TaskEvalInfo &) = 0;
    virtual void dstr(TaskDstrInfo &) { };
  };

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

  enum class TaskSignalFlags : uint {
    eNone       = 0x000u,

    // Signal that tasks and owned resources are to be destroyed after run
    eClearTasks = 0x001u,

    // Signal that tasks and all resources are to be destroyed after run
    eClearAll   = 0x002u,
  };
  met_declare_bitflag(TaskSignalFlags);

  /**
   * Abstract base class that consumes application tasks and updates the environment
   * in which they exist.
   */
  class AbstractTaskInfo {
  protected:
    using KeyType         = std::string;
    using RsrcType        = std::shared_ptr<AbstractResource>;
    using TaskType        = std::shared_ptr<AbstractTask>;
    using RsrcMapType     = std::unordered_map<KeyType, RsrcType>;
    using ApplRsrcMapType = std::unordered_map<KeyType, RsrcMapType>;

    RsrcMapType     &_task_resource_registry;
    ApplRsrcMapType &_appl_resource_registry;

    AbstractTaskInfo(ApplRsrcMapType &appl_resource_registry, const AbstractTask &task)
    : _appl_resource_registry(appl_resource_registry),
      _task_resource_registry(appl_resource_registry[task.name()]) { };
    
  public:

    /* Public data registries */

    std::unordered_map<KeyType, RsrcType>   add_resource_registry;
    std::list<std::pair<KeyType, TaskType>> add_task_registry;
    std::list<KeyType>                      remove_resource_registry;
    std::list<KeyType>                      remove_task_registry;
    TaskSignalFlags                         signal_flags = TaskSignalFlags::eNone;

    /* Create, add, remove resources */

    template <typename Ty, typename InfoTy = Ty::InfoType>
    void emplace_resource(const KeyType &key, InfoTy info) {
      add_resource_registry.emplace(key, std::make_shared<detail::Resource<Ty>>(Ty(info)));
    }
  
    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      add_resource_registry.emplace(key, std::make_shared<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void erase_resource(const KeyType &key) {
      remove_resource_registry.push_back(key);
    }

    /* Create, add, remove subtasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_shared<Ty>(std::move(task)));
    }
    
    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_shared<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key) {
      remove_task_registry.push_back(key);
    }

    /* Access existing resources */

    template <typename T>
    T & get_resource(const KeyType &key) {
      if (auto i = _task_resource_registry.find(key); i != _task_resource_registry.end()) {
        return i->second->get_as<T>();
      } else {
        return get_resource<T>(std::string(resource_global_key), key);
      }
    }

    template <typename T>
    T & get_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _appl_resource_registry.at(task_key).at(rsrc_key)->get_as<T>();
    }
  };

  struct TaskInitInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we initialize the task
    TaskInitInfo(ApplRsrcMapType &appl_resource_registry, AbstractTask &task)
    : AbstractTaskInfo(appl_resource_registry, task) {
      task.init(*this);
    }
  };

  struct TaskEvalInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we eval/run the task
    TaskEvalInfo(ApplRsrcMapType &appl_resource_registry, AbstractTask &task)
    : AbstractTaskInfo(appl_resource_registry, task) {
      task.eval(*this);
    }
  };

  struct TaskDstrInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we eval/run the task
    TaskDstrInfo(ApplRsrcMapType &appl_resource_registry, AbstractTask &task)
    : AbstractTaskInfo(appl_resource_registry, task) {
      task.dstr(*this);
    }
  };
} // met::detail
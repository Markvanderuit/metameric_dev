#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>
#include <metameric/core/detail/scheduler_resource.hpp>
#include <memory>
#include <list>
#include <string>
#include <unordered_map>

namespace met {
  // Global key for resources with no managing task
  const std::string global_key = "global";
} // namespace met

namespace met::detail {
  // fwd
  class TaskInitInfo;
  class TaskEvalInfo;
  class TaskDstrInfo;

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
    using ApplTaskVecType = const std::vector<TaskType>; 

    RsrcMapType     &_task_rsrc_registry;
    ApplRsrcMapType &_appl_rsrc_registry;
    ApplTaskVecType &_appl_task_registry;

    AbstractTaskInfo(ApplRsrcMapType &appl_rsrc_registry,
                     ApplTaskVecType &appl_task_registry,
                     const AbstractTask &task)
    : _appl_rsrc_registry(appl_rsrc_registry),
      _appl_task_registry(appl_task_registry),
      _task_rsrc_registry(appl_rsrc_registry[task.name()]) { };
    
  public:

    /* Public data registries */

    std::unordered_map<KeyType, RsrcType>   add_rsrc_registry;
    std::list<std::pair<KeyType, TaskType>> add_task_registry;
    std::list<KeyType>                      rem_rsrc_registry;
    std::list<KeyType>                      rem_task_registry;
    TaskSignalFlags                         signal_flags = TaskSignalFlags::eNone;

    /* Create, add, remove resources */

    template <typename Ty, typename InfoTy = Ty::InfoType>
    void emplace_resource(const KeyType &key, InfoTy info) {
      add_rsrc_registry.emplace(key, std::make_shared<detail::Resource<Ty>>(Ty(info)));
    }

    template <typename Ty>
    void insert_resource(const KeyType &key, Ty &&rsrc) {
      add_rsrc_registry.emplace(key, std::make_shared<detail::Resource<Ty>>(std::move(rsrc)));
    }

    void remove_resource(const KeyType &key) {
      rem_rsrc_registry.push_back(key);
    }

    /* Create, add, remove subtasks */
    
    template <typename Ty, typename... Args>
    void emplace_task(const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_shared<Ty>(key, args...));
    }

    template <typename Ty, typename... Args>
    void emplace_task_after(const KeyType &prev, const KeyType &key, Args... args) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_shared<Ty>(key, args...));
    }

    template <typename Ty>
    void insert_task(const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back("", std::make_shared<Ty>(std::move(task)));
    }

    template <typename Ty>
    void insert_task_after(const KeyType &prev, const KeyType &key, Ty &&task) {
      static_assert(std::is_base_of_v<AbstractTask, Ty>);
      add_task_registry.emplace_back(prev, std::make_shared<Ty>(std::move(task)));
    }

    void remove_task(const KeyType &key) {
      rem_task_registry.push_back(key);
    }

    /* Access existing resources */

    template <typename T>
    T & get_resource(const KeyType &key) {
      if (auto i = _task_rsrc_registry.find(key); i != _task_rsrc_registry.end()) {
        return i->second->get_as<T>();
      } else {
        return get_resource<T>(global_key, key);
      }
    }

    template <typename T>
    T & get_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _appl_rsrc_registry.at(task_key).at(rsrc_key)->get_as<T>();
    }

    bool has_resource(const KeyType &task_key, const KeyType &rsrc_key) {
      return _appl_rsrc_registry.contains(task_key)
          && _appl_rsrc_registry.at(task_key).contains(rsrc_key);
    }

    /* miscellaneous, debug info */

    // String output of current task schedule
    std::vector<KeyType> schedule_list() const {
      std::vector<std::string> v(_appl_task_registry.size());
      std::ranges::transform(_appl_task_registry, v.begin(), [](const auto &t) { return t->name(); });
      return v;
    }
  };

  struct TaskInitInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we initialize the task
    TaskInitInfo(ApplRsrcMapType &appl_rsrc_registry,
                 ApplTaskVecType &appl_task_registry,
                 AbstractTask &task)
    : AbstractTaskInfo(appl_rsrc_registry, appl_task_registry, task) {
      task.init(*this);
    }
  };

  struct TaskEvalInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we eval/run the task
    TaskEvalInfo(ApplRsrcMapType &appl_rsrc_registry,
                 ApplTaskVecType &appl_task_registry,
                 AbstractTask &task)
    : AbstractTaskInfo(appl_rsrc_registry, appl_task_registry, task) {
      task.eval(*this);
    }
  };

  struct TaskDstrInfo : public AbstractTaskInfo {
    // By consuming the task in this object, we eval/run the task
    TaskDstrInfo(ApplRsrcMapType &appl_rsrc_registry,
                 ApplTaskVecType &appl_task_registry,
                 AbstractTask &task)
    : AbstractTaskInfo(appl_rsrc_registry, appl_task_registry, task) {
      task.dstr(*this);
    }
  };
} // met::detail
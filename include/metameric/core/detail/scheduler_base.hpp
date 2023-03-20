#pragma once

#include <metameric/core/utility.hpp>
#include <memory>
#include <unordered_map>

namespace met {
  // Forward declarations
  struct SchedulerBase;
  struct SchedulerHandle;

  namespace detail {
    // Virtual base class for tasks submitted to application scheduler
    // Implementations contain majority of program code
    struct TaskBase {
      // Override and implement
      virtual void init(SchedulerHandle &) { };
      virtual void eval(SchedulerHandle &) { };
      virtual void dstr(SchedulerHandle &) { };
    };

    // Abstract base class for application resources;
    template <typename> struct RsrcImpl; // FWD of implementing class
    struct RsrcBase {
      template <typename T>
      T & get() {
        return static_cast<RsrcImpl<T> *>(this)->m_object;
      }

      template <typename T>
      const T & get() const {
        return static_cast<RsrcImpl<T> *>(this)->m_object;
      }
    };

    // Implementation class for application resource to hold specific type
    template <typename T>
    struct RsrcImpl : RsrcBase {
      T m_object;

      RsrcImpl(T &&object)
      : m_object(std::move(object)) { }
    };

    // Shorthands for common types
    using TaskNode = std::shared_ptr<TaskBase>;
    using RsrcNode = std::shared_ptr<RsrcBase>;

    class SchedulerBase {
    protected:
      // Info object for adding a new task to a schedule
      struct AddTaskInfo {
        std::string prnt_key = "";      // Key of parent task to which task is appended
        std::string task_key = "";      // Key of task
        TaskNode    task     = nullptr; // Pointer to task
      };

      // Info object for removing a task from the schedule
      struct RemTaskInfo {
        std::string prnt_key = "";      // Key of parent task to which task is appended
        std::string task_key = "";      // Key of task
      };

      // Info object for adding a new resource to a task
      struct AddRsrcInfo {
        std::string task_key = "";
        std::string rsrc_key = "";
        RsrcNode    rsrc     = nullptr; // Pointer to resource
      };

      // Info object for removing a resource from a task 
      struct RemRsrcInfo {
        std::string task_key = "";
        std::string rsrc_key = "";
      };

      // Info object for querying a resource from a task 
      struct GetRsrcInfo {
        std::string task_key = "";
        std::string rsrc_key = "";
      };

      // Virtual functions implementing add/remove tasks
      virtual void add_task_impl(AddTaskInfo &&) = 0;
      virtual void rem_task_impl(RemTaskInfo &&) = 0;

      // Virtual functions implementing add/remove/get resources
      virtual RsrcBase* add_rsrc_impl(AddRsrcInfo &&)      = 0; // nullable
      virtual RsrcBase* get_rsrc_impl(GetRsrcInfo &&)      = 0; // nullable
      virtual void      rem_rsrc_impl(RemRsrcInfo &&)      = 0;

      // Used internally; implement as "global" for scheduler, and task key for schedule handle
      virtual const std::string &task_default_key() const = 0;
      
    public: /* Add/remove task functions */
      template <typename Ty, typename... Args>
      void emplace_task(const std::string &key, Args... args) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl(AddTaskInfo { .prnt_key = "",
                                    .task_key = key,
                                    .task     = std::make_shared<Ty>(args...)});
      }

      template <typename Ty, typename... Args>
      void emplace_subtask(const std::string &prnt, const std::string &key, Args... args) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl(AddTaskInfo { .prnt_key = prnt,
                                    .task_key = key,
                                    .task     = std::make_shared<Ty>(args...)});
      }

      template <typename Ty>
      void insert_task(const std::string &key, Ty &&task) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl(AddTaskInfo { .prnt_key = "",
                                    .task_key = key,
                                    .task = std::make_shared<Ty>(std::move(task)) });
      }

      template <typename Ty>
      void insert_subtask(const std::string &prnt, const std::string &key, Ty &&task) {
        static_assert(std::is_base_of_v<TaskBase, Ty>);
        met_trace();
        add_task_impl(AddTaskInfo { .prnt_key = prnt,
                                    .task_key = key,
                                    .task = std::make_shared<Ty>(std::move(task)) });
      }

      void remove_task(const std::string &key) {
        met_trace();
        rem_task_impl(RemTaskInfo { .task_key = key });
      }

      void remove_subtask(const std::string &prnt, const std::string &key) {
        met_trace();
        rem_task_impl(RemTaskInfo { .prnt_key = prnt, .task_key = key });
      }

    public: /* Add/remove/get resource functions */
      template <typename Ty, typename InfoTy = Ty::InfoType>
      Ty& emplace_resource(const std::string &key, InfoTy info) {
        met_trace();
        auto *rsrc = add_rsrc_impl(AddRsrcInfo { .task_key = task_default_key(),
                                                 .rsrc_key = key,
                                                 .rsrc     = std::make_unique<detail::RsrcImpl<Ty>>(Ty(info)) });
        return rsrc->get<Ty>();
      }

      template <typename Ty>
      void insert_resource(const std::string &key, Ty &&rsrc) {
        met_trace();
        add_rsrc_impl(AddRsrcInfo { .task_key = task_default_key(),
                                    .rsrc_key = key,
                                    .rsrc     = std::make_unique<detail::RsrcImpl<Ty>>(std::move(rsrc)) });
      }

      void remove_resource(const std::string &key) {
        met_trace();
        rem_rsrc_impl(RemRsrcInfo { .task_key = task_default_key(), .rsrc_key = key });
      }

      template <typename Ty>
      const Ty & get_resource(const std::string &key) const {
        met_trace();
        auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_default_key(), .rsrc_key = key });
        debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_default_key(), key));
        return ptr->get<Ty>();
      }

      template <typename Ty>
      Ty & get_resource(const std::string &key) {
        met_trace();
        auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_default_key(), .rsrc_key = key });
        debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_default_key(), key));
        return ptr->get<Ty>();
      }

      template <typename Ty>
      const Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) const {
        met_trace();
        auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key });
        debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key, rsrc_key));
        return ptr->get<Ty>();
      }

      template <typename Ty>
      Ty & get_resource(const std::string &task_key, const std::string &rsrc_key) {
        met_trace();
        auto ptr = get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key });
        debug::check_expr_rel(ptr, fmt::format("get_resource failed for {}, {}", task_key, rsrc_key));
        return ptr->get<Ty>();
      }
      
      bool has_resource(const std::string &task_key, const std::string &rsrc_key) {
        met_trace();
        return get_rsrc_impl(GetRsrcInfo { .task_key = task_key, .rsrc_key = rsrc_key }) != nullptr;
      }

    public: /* Miscellaneous functions */
      // Rebuild underlying schedule from current list of submitted tasks/resources
      virtual void build() = 0;

      // Clear out tasks and owned resources; preserve_global t/f -> retain non-owned resources
      virtual void clear(bool preserve_global = true) = 0;

      // Debug output string formatted task schedule
      virtual std::vector<std::string> schedule() const = 0;
      using RsrcMap = std::unordered_map<std::string, std::unordered_map<std::string, RsrcNode>>;
      virtual const RsrcMap &resources() const = 0;
    };
  } // namespace detail

  // String key referring to global, non-owned resources in scheduler resource management
  const std::string global_key = "global";

  // Virtual base class for application sscheduler
  struct SchedulerBase : public detail::SchedulerBase {
  protected:
    virtual const std::string &task_default_key() const { return global_key; }

  public:
    // Run the currently built schedule
    virtual void run() = 0;
  };

  // Virtual base class for scheduler handle, passed to task nodes
  struct SchedulerHandle : public detail::SchedulerBase {
  protected:
    virtual const std::string &task_default_key() const { return task_key(); }

  public:
    // Get key of the current active task
    virtual const std::string &task_key() const = 0;
  };
} // namespace met::detail
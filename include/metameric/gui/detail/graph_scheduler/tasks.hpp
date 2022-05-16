#pragma once

#include <list>
#include <memory>
#include <string>
#include <span>
#include <vector>

namespace met {
  class CreateTaskInfo; // fwd
  class RuntimeTaskInfo; // fwd

  class AbstractTask {
    std::string _name;

  public:
    const std::string &name() const { return _name; } 

    AbstractTask(const std::string &name) : _name(name) {} 

    virtual void create(CreateTaskInfo &) = 0;
    virtual void run(RuntimeTaskInfo &) = 0;
  };

  class CreateTaskInfo {
    using KeyType       = std::string;
    using RsrcPtrType   = std::shared_ptr<detail::VirtualResource>;
    using TaskPtrType   = std::unique_ptr<AbstractTask>;
    using CreateHandle  = std::pair<KeyType, RsrcPtrType>;
    using ReadHandle    = std::pair<KeyType, KeyType>;
    using WriteHandle   = std::pair<ReadHandle, KeyType>;

    KeyType _task_name;

  public:
    /* const */

    CreateTaskInfo(AbstractTask &task)
    : _task_name(task.name()) {
      task.create(*this);
    }

    /* public resource lists */

    std::list<CreateHandle> resources;
    std::list<ReadHandle>   reads;
    std::list<WriteHandle>  writes;

    /* resource create/read/write functions */

    template <typename Ty, typename Tinfo>
    ReadHandle create_resource(const KeyType &name, Tinfo info_object) {
      resources.push_back({ name, 
                           std::make_shared<detail::ConcreteResource<Ty>>(info_object) });
      reads.push_back({ _task_name, name });
      return reads.back();
    }

    template <typename Ty>
    ReadHandle emplace_resource(const KeyType &name, Ty &&object) {
      resources.push_back({ name, 
                            std::make_shared<detail::ConcreteResource<Ty>>(std::move(object)) });
      reads.push_back({ _task_name, name });
      return reads.back();
    }

    ReadHandle read_resource(const KeyType &task_name, const KeyType &handle_name) {
      reads.push_back({ task_name, handle_name });
      return reads.back();
    }

    void write_resource(const ReadHandle &handle, const KeyType &output_name) {
      writes.push_back({ handle, output_name });
    }
  };

  class RuntimeTaskInfo {
    using KeyType                 = std::string;
    using PtrType                 = std::unique_ptr<AbstractTask>;
    using PairType                = std::pair<KeyType, PtrType>;
    using ReadHandle              = std::pair<KeyType, KeyType>;
    using WriteHandle             = std::pair<ReadHandle, KeyType>;
    using ResourcesMapType        = TaskResources;

    using GlobalResourcesMapType  = std::unordered_map<KeyType, ResourceHolder>;
    
    // Passed in registry references
    ResourcesMapType        &_task_resources;
    GlobalResourcesMapType  &_application_resources;

  public:
    /* Constructor */

    RuntimeTaskInfo(GlobalResourcesMapType &resources, const PtrType &task)
    : _application_resources(resources), 
      _task_resources(resources.at(task->name()).resources) 
    { }

    /* Resource access */

    template <typename T>
    T & get_resource(const KeyType &resource_name) const {
      return _task_resources.at<T>(resource_name);
    }

    template <typename T>
    T & get_resource(const KeyType &task_name, const KeyType &resource_name) const {
      return _application_resources.at(task_name).resources.at<T>(resource_name);
    }

    /* Task scheduling  */

    std::list<KeyType>  erases;
    std::list<PairType> inserts;

    template <typename  T, typename... Args>
    void insert_task(const KeyType &task_name, Args... args) {
      inserts.push_back({ KeyType(), std::make_unique<T>(task_name, args...) });
    }

    template <typename  T, typename... Args>
    void insert_task_after(const KeyType &other_task_name, 
                           const KeyType &task_name, 
                           Args... args) {
      inserts.push_back({ other_task_name, std::make_unique<T>(task_name, args...) });
    }

    void remove_task(const KeyType &task_name) {
      erases.push_back(task_name);
    }
  };

  class ApplicationTasks {
    using KeyType = std::string;
    using PtrType = std::unique_ptr<AbstractTask>;

    template <typename T>
    PtrType constr_ptr(T && object) { 
      return std::move(std::make_unique<T>(std::move(object)));
    }

    std::vector<PtrType> _data;

  public:
    void insert(PtrType &&object) {
      _data.emplace_back(std::move(object));
    }

    void insert_after(const std::string &name, PtrType &&object) {
      auto i = std::ranges::find_if(_data, [name](auto &t) { return t->name() == name; });
      if (i != _data.end()) i++;
      _data.insert(i, std::move(object));
    }

    template <typename T>
    void insert(T &&object) {
      _data.push_back(constr_ptr<T>(std::move(object)));
    }

    template <typename T>
    void insert_after(const std::string &name, T &&object) {
      auto i = std::ranges::find_if(_data, [name](auto &t) { return t->name() == name; });
      if (i != _data.end()) i++;
      _data.insert(i, constr_ptr<T>(std::move(object)));
    }

    void erase(const std::string &name) {
      std::erase_if(_data, [name](auto &t) { return t->name() == name; });
    }

    std::span<std::unique_ptr<AbstractTask>> data() {
      return std::span(_data);
    }
  };

  class LambdaTask : public AbstractTask {
    using CreateType = std::function<void(CreateTaskInfo &)>;
    using RunType = std::function<void(RuntimeTaskInfo &)>;
    
    CreateType _create;
    RunType _run;

  public:
    LambdaTask(const std::string &name, CreateType create, RunType run)
    : AbstractTask(name), 
      _create(create), 
      _run(run) 
    { }

    void create(CreateTaskInfo &info) override {
      _create(info);
    }

    void run(RuntimeTaskInfo &info) override {
      _run(info);
    }
  };
} // namespace met
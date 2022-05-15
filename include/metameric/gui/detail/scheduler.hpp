#pragma once

#include <metameric/gui/detail/graph.hpp>

namespace met {
  struct AbstractTask {
    std::string _name;

    AbstractTask(const std::string &name)
    : _name(name) { }    

    virtual void init() = 0;
    virtual void exec() = 0;
  };

  struct Scheduler {


    template <typename Ty, typename... Args>
    void create_task(Args... args) {
      // ...
    }

    void compile() {
      // ...
    }

    void run() {
      // ...
    }
  };
} // namespace met
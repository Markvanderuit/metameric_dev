#include <metameric/app/application.hpp>
#include <fmt/core.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <ranges>
#include <unordered_map>
#include <vector>

/* enum class ResourceFlag { eReadWrite, eRead, eWrite }; */

// FWD
/* struct AbstractTaskNode;
struct AbstractResourceNode;
template <typename T> struct TaskNode;
template <typename T> struct ResourceNode;

struct AbstractResourceNode {
  template <typename T>
  T & get_as() {
    return static_cast<ResourceNode<T> *>(this)->m_object;
  }

  template <typename T>
  const T & get_as() const {
    return static_cast<ResourceNode<T> *>(this)->m_object;
  }
};

template <typename T>
struct ResourceNode {
  ResourceNode(const std::string &name, T &&object)
  : m_name(name), m_object(std::move(object)) { }

  std::string m_name;
  T           m_object;
};



class GraphScheduler {
  using RsrcNode = std::shared_ptr<AbstractResourceNode>;
  using TaskNode = std::shared_ptr<AbstractTaskNode>;
  using RsrcRegs = std::unordered_map<
    std::string, 
    std::unordered_map<std::string, RsrcNode>
  >;
  using TaskRegs = std::vector<TaskNode>;

  RsrcRegs m_resources;
  TaskRegs m_tasks;
  

};

constexpr void test_umap() {
  constexpr std::unordered_map<std::string, int> map;
  map["1"] = 1;
  map["2"] = 2;
} */

int main() {



  /* try { */
    met::create_application({ .color_mode    = met::AppColorMode::eDark });
  /* } catch (const std::exception &e) {
    fmt::print(stderr, "{}\n", e.what());
    return EXIT_FAILURE;
  } */
  return EXIT_SUCCESS;
}
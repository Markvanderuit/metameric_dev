#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/spectrum.hpp>
#include <ranges>
#include <vector>

namespace met {
  // FWD
  struct BasisTreeNode;

  struct BasisTreeNode {
  public: /* public data components */
    // Node data
    uint depth;
    eig::Array2f bbox_min, bbox_max; // Bounding box
    Basis basis;                     // Basis functions over spectra in bounding box region
    
    // Child node data
    std::vector<BasisTreeNode> children;
    
  public: /* public methods */
    Basis traverse(const eig::Array2f &xy) const {
      if (depth == 1)
        return basis;
        
      if (auto it = std::ranges::find_if(children, [&](const auto &n) { return n.is_in_node(xy); }); it != children.end()) {
        return it->traverse(xy);
      } else {
        return basis;
      }
    }

    bool is_in_node(const eig::Array2f &xy) const {
      return xy.max(bbox_min).min(bbox_max).isApprox(xy);
    }
  };
}
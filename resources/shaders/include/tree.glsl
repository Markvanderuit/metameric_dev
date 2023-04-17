#ifndef TREE_GLSL_GUARD
#define TREE_GLSL_GUARD

// Magic numbers
#define TREE_KNODE_3D 8
#define TREE_KNLOG_3D 3

// Fun stuff for fast modulo and division and stuff
const uint tree_bitmask = ~((~0u) << TREE_KNLOG_3D);

// Tree traversal data
struct TreeStack {
  uint lvl;
  uint loc;
  uint stack;
};

TreeStack tree_root() {
  return TreeStack(0, 0, 1);
}

void tree_descend(inout TreeStack tree) {
  // Move down tree
  tree.lvl++;
  tree.loc = tree.loc * TREE_KNODE_3D + 1u;

  // Push unvisited locations on stack
  tree.stack |= (tree_bitmask << (TREE_KNLOG_3D * tree.lvl));
}

void tree_ascend(inout TreeStack tree) {
  // Find distance to next level on stack
  uint nextLvl = findMSB(tree.stack) / TREE_KNLOG_3D;
  uint dist = tree.lvl - nextLvl;

  // Move dist up to where next available stack position is
  // and one to the right
  if (dist == 0) {
    tree.loc++;
  } else {
    tree.loc >>= TREE_KNLOG_3D * dist;
  }
  tree.lvl = nextLvl;

  // Pop visited location from stack
  uint shift = TREE_KNLOG_3D * tree.lvl;
  uint b = (tree.stack >> shift) - 1;
  tree.stack &= ~(tree_bitmask << shift);
  tree.stack |= b << shift;
}

#endif // TREE_GLSL_GUARD
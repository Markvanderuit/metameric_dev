// #include <catch2/catch_test_macros.hpp>
// #include <metameric/core/math.hpp>
// #include <metameric/core/utility.hpp>
// #include <bit>

// // Test in metameric namespace
// using namespace met;

// int find_msb(uint i) {
//   return 31 - std::countl_zero(i);
// }

// uint bit_count(uint i) {
//   return std::popcount(i);
// }

// TEST_CASE("Stack-based traversal") {
//   uint s_stack[24];
  
//   s_stack[0]  = 1u << 24;

//   // Initialize stack with (0, 0) as node 
//   uint stackc = 1;
//   uint stack_value = s_stack[stackc - 1];

//   // Get first node data, should be (0, 0)
//   uint node_first  = stack_value & 0x00FFFFFF;
//   int  node_bit    = find_msb(stack_value >> 24);

//   SECTION("First stack value is (0, 0)") {
//     CHECK(node_first == 0);
//     CHECK(node_bit   == 0);
//   } 

//   // Remove flagged child bit from stack value
//   stack_value &= (~(1u << (node_bit + 24)));
  
//   SECTION("Did remove child bit from stack") {
//     CHECK(find_msb(stack_value >> 24) == -1);
//   }

//   if (bit_count(stack_value >> 24) == 0)
//     stackc--;

//   SECTION("Did decrease stack count") {
//     CHECK(stackc == 0);
//   }

  
// }
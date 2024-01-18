#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <metameric/core/distribution.hpp>
#include <metameric/core/math.hpp>
#include <metameric/core/utility.hpp>

// Test in metameric namespace
using namespace met;

eig::Vector2f square_to_unif_disk_concentric(eig::Vector2f sample_2d) {
  sample_2d = 2.f * sample_2d.array() - 1.f;

  bool is_zero = sample_2d.isZero();
  bool quad_1_or_3 = std::abs(sample_2d.x()) < std::abs(sample_2d.y());

  float r  = quad_1_or_3 ? sample_2d.y() : sample_2d.x(),
        rp = quad_1_or_3 ? sample_2d.x() : sample_2d.y();
  
  float phi = 0.25f * M_PI * rp / r;
  if (quad_1_or_3)
    phi = .5f * M_PI - phi;
  if (is_zero)
    phi = 0.f;

  return eig::Vector2f(r * cos(phi), r * sin(phi));
}

eig::Vector3f square_to_unif_hemisphere(eig::Vector2f sample_2d) {
  eig::Vector2f p = square_to_unif_disk_concentric(sample_2d);
  float z = 1.f - p.dot(p);
  p *= std::sqrt(z + 1.f);
  return eig::Vector3f(p.x(), p.y(), z);
}

struct Frame {
  eig::Vector3f n;
  eig::Vector3f s, t;

  Frame(eig::Vector3f n)
  : n(n) {

    float s = n.z() >= 0.f ? 1.f : -1.f;
    float a = -1.f / (s + n.z());
    float b = n.x() * n.y() * a; 

    this->s = eig::Vector3f(n.x() * n.x() * a *  s + 1.f,
                            b * s,
                            n.x() * -s);
    this->t = eig::Vector3f(b,
                            n.y() * n.y() * a + s,
                           -n.y());
  }

  eig::Vector3f to_local(eig::Vector3f v) {
    return eig::Vector3f(v.dot(s),
                         v.dot(t),
                         v.dot(n));
  }

  eig::Vector3f to_world(eig::Vector3f v) {
    return n * v.z() + t * v.y() + s * v.x();
  }
};

TEST_CASE("Frame") {
  UniformSampler sampler(5);

  // Build random frame
  auto n = sampler.next_nd<3>().matrix().normalized().eval();
  Frame frm = n;

  // Assert orthonormality
  REQUIRE_THAT(frm.n.dot(frm.s), Catch::Matchers::WithinAbs(0.f, 0.0001f));
  REQUIRE_THAT(frm.n.dot(frm.t), Catch::Matchers::WithinAbs(0.f, 0.0001f));
  REQUIRE_THAT(frm.s.dot(frm.t), Catch::Matchers::WithinAbs(0.f, 0.0001f));
} // TEST_CASE

TEST_CASE("Emitter sampling") {
  UniformSampler sampler(5);

  eig::Vector3f c = 0.f;
  eig::Vector3f o = sampler.next_nd<3>();

  // Build frame
  eig::Vector3f n = (o - c).normalized();
  Frame frm = n;

  for (uint i = 0; i < 32; ++i) {
    auto p = frm.to_world(square_to_unif_hemisphere(sampler.next_nd<2>()));
    
    // Assert that vectors are within length
    float t = std::sqrt(p.dot(p));
    REQUIRE_THAT(t, Catch::Matchers::WithinAbs(1.f, 0.0001f));

    // Assert that vectors lie on positive side of hemisphere
    REQUIRE(p.dot(n) >= 0.f);
  }

  eig::Vector3f v = { 1.5, 1.5, 1.0 };
  v.normalize();
  v -= eig::Vector3f(0.5, 0.5, 0.0);
  v *= 3;
  v += eig::Vector3f(0.5, 0.5, 0.0);
  fmt::print("{}\n", v);
} // TEST_CASE

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
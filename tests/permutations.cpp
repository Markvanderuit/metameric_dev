// #include <catch2/catch_test_macros.hpp>
// #include <metameric/core/math.hpp>
// #include <metameric/core/distribution.hpp>
// #include <metameric/core/utility.hpp>
// #include <string>
// #include <bitset>
// #include <fmt/core.h>
// #include <fmt/ranges.h>

// using namespace met;

// namespace met {
//   struct PathElement {
//     uint i;
//     bool b;
//   };
//   using Path = std::vector<PathElement>;
// }

// TEST_CASE("Permutations") {
//   uint n = 2;
//   uint n_permutations = std::pow(2u, n);

//   // Generate all permutatioons
//   std::vector<met::Path> paths;
//   {
//     met::Path default_path(n);
//     for (uint i = 0; i < default_path.size(); ++i)
//       default_path[i] = { i, false };
//     paths.push_back(default_path);
    
//     uint curr_n = 0;
//     while (curr_n < n) {
//       uint curr_size = paths.size();
//       for (uint j = 0; j < curr_size; ++j) {
//         met::Path new_path = paths[j];
//         new_path[curr_n].b = true;
//         paths.push_back(new_path);
//       }
//       curr_n++;
//     }
//     CHECK(paths.size() == n_permutations);
//   }
  
//   // Partial-sort all paths, moving unnecessary vertices to the front
//   for (auto &path : paths)
//     rng::sort(path, {}, &met::PathElement::b);
  
//   // Count the nr. of paths with [0, 1, 2, ... n] choices of true
//   std::vector<uint> true_counts_per_path(paths.size(), 0);
//   rng::transform(paths, true_counts_per_path.begin(), 
//     [](const auto &path) { return rng::count(path, true, &met::PathElement::b); });

//   std::vector<uint> true_counts_counted(n + 1, 0); // [0, 1, 2, ..., D]
//   for (uint i = 0; i < true_counts_counted.size(); ++i)
//     true_counts_counted[i] = rng::count(true_counts_per_path, i);

//   fmt::print("Given path n of {}\n", n);
//   fmt::print("Total path count    {}\n", paths.size());
//   for (uint i = 0; i < true_counts_counted.size(); ++i) {
//     fmt::print("\tPaths with {} true: {}\n", i, true_counts_counted[i]);
//   }
  
//   std::vector<std::vector<Path>> stripped_paths(n + 1);
//   for (const auto &path : paths) {
//     auto remainder = path | vws::filter([](const auto &p) { return p.b == false; }) | rng::to<std::vector>();
//     stripped_paths[remainder.size()].push_back(remainder);
//   }

//   /* for (uint i = 0; i < stripped_paths.size(); ++i) {
//     uint pow = n - i;
    
//     fmt::print("Path {} = ", i);
//     fmt::print("r^{} * (", pow);
//     for (const auto &path : stripped_paths[i]) {
//       guard_continue(!path.empty());
//       // fmt::print("[");

//       for (auto el : path | vws::take(1))
//         fmt::print("w{}", el.i);
//       for (auto el : path | vws::drop(1))
//         fmt::print(" * w{}", el.i);

//       if (stripped_paths.size() > 1)
//         fmt::print(" + ");
//       // else
//       //   fmt::print("]");
//     }
//     fmt::print(") ({} perms)\n", stripped_paths[i].size());
//   } */

//   /* for (uint i = 0; i < true_counts_per_path.size(); ++i) {
//     Path path = paths[i];
//     auto remainder = path | vws::filter([](const auto &p) { return p.b == false; });

//     fmt::print("Path {} = ", i);
//     fmt::print("r^{} * (", true_counts_per_path[i]);
//     for (PathElement v : remainder) fmt::print("{},", v.i);
//     fmt::print(")\n");
//   } */

//   /* for (const auto &v : true_counts_counted) {
//     fmt::print("Path: ");
//     fmt::print("{}", v);
//     // for (const auto &elem : path)
//     //   fmt::print("[{}, {}], ", elem.i, elem.b);
//     fmt::print("\n");
//   } */
// }

// TEST_CASE("Product to sum") {
//   PCGSampler sampler(4);
//   std::uniform_int_distribution<uint> distr(0, 10);

//   // Generate inputs
//   uint n = 6; // Path length
//   std::vector<uint> a(n);
//   std::vector<uint> b(n);
//   rng::for_each(a, [&](uint &i) { i = distr(sampler); });
//   rng::for_each(b, [&](uint &i) { i = distr(sampler); });

//   // Product of sums
//   uint prod_of_sums = 1;
//   for (uint i = 0; i < n; ++i) {
//     prod_of_sums *= (a[i] + b[i]);
//   }

//   // Sum of products
//   uint sum_of_prods = 0;
//   for (uint perm = 0; perm < std::pow<uint>(2u, n); ++perm) {
//     // Use bit representation to go over all permutations
//     std::bitset<32> flags(perm);
    
//     uint r = 1;
//     for (uint i = 0; i < n; ++i) {
//       r *= flags[i] ? a[i] : b[i];
//     }
//     sum_of_prods += r;
//   }

//   fmt::print("Product of sums: {}\n", prod_of_sums);
//   fmt::print("Sum of products: {}\n", sum_of_prods);

//   // Assert equality
//   CHECK(prod_of_sums == sum_of_prods);
// }
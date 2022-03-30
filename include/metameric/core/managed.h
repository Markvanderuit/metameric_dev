#pragma once

#include <atomic>
#include <metameric/core/fwd.h>

namespace metameric {

enum class ManagedLocation {
  cpu,
  cuda,
  gl
};

template <typename Object>
class Managed {
  // Default constructor
  Managed() { }

  // Copy constructor
  Managed(const Managed &) { }

  // Construct a Managed from an object
  Managed(const Object &object)
  : object(object) {

  }
private:
  Object object;
};

/* class Managed {
  // Default constructor
  Managed() { }

  // Copy constructor
  Managed(const Managed &) { }

  int ref_count() const { return _ref_count; };
  void inc_ref() const { ++_ref_count; };
  void dec_ref(bool deallocate = true) const noexcept;

  virtual Managed* cpu() const;
  virtual Managed* cuda() const;
  virtual Managed* gl() const;

protected:
  // Virtual protected destructor
  virtual ~Managed();

private:
  mutable std::atomic<int> _ref_count;
}; */

} // namespace metameric

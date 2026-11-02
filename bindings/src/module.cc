// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>

namespace ofod::bindings {
void BindMath(nanobind::module_& m);
}  // namespace ofod::bindings

NB_MODULE(_core, m) {
  m.doc() = "Bindings for the ofod11 C++ library";
  ofod::bindings::BindMath(m);
}

// Copyright 2026 Denis Zhereshchin

#include <nanobind/nanobind.h>

#include "bindings.h"

NB_MODULE(_core, m) {
  m.doc() = "Bindings for the ofod11 C++ library";
  // Order matters: later modules use types registered by earlier ones.
  nanobind::module_ core = m.def_submodule("core", "ofod core types");
  ofod::bindings::BindCore(core);
  nanobind::module_ math = m.def_submodule("math", "ofod::math");
  ofod::bindings::BindMath(math);
  nanobind::module_ instruments =
      m.def_submodule("instruments", "ofod::instruments");
  ofod::bindings::BindInstruments(instruments);
  nanobind::module_ curves = m.def_submodule("curves", "ofod::curves");
  ofod::bindings::BindCurves(curves);
  nanobind::module_ pricers = m.def_submodule("pricers", "ofod::pricers");
  ofod::bindings::BindPricers(pricers);
}

#pragma once

#define GETTER(T, N) \
  T get_##N() const { return N; }

#define GETTER_SETTER(T, N) \
  GETTER(T, N)              \
  void set_##N(T x) { N = x; }
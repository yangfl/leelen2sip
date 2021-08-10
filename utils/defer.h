#ifndef CEDILLA_DEFER_H
#define CEDILLA_DEFER_H

#define __defer_CONCAT_IMPL(x, y) x ## y
#define __defer_CONCAT(x, y) __defer_CONCAT_IMPL(x, y)
#define __generate_defer(counter, ...) \
  void __defer_CONCAT(__defer_func_, counter) () { __VA_ARGS__; } \
  __attribute__((unused, cleanup(__defer_CONCAT(__defer_func_, counter)))) \
  char __defer_CONCAT(__defer_var_, counter)
#define defer(...) __generate_defer(__COUNTER__, __VA_ARGS__)

#endif /* CEDILLA_DEFER_H */

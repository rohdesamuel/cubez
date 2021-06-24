#ifndef CUBEZ_FINALLY__H
#define CUBEZ_FINALLY__H

template <typename F>
class Finally {
  F f;
public:
  template <typename Func>
  Finally(Func&& func) : f(std::forward<Func>(func)) {}
  ~Finally() {
    f();
  }
};

template <typename F>
Finally<F> make_finally(F&& f) {
  return{ std::forward<F>(f) };
}

#endif  // CUBEZ_FINALLY__H
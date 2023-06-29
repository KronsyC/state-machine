#include <concepts>
#include <functional>
#include <type_traits>

template <bool cond, typename T, typename F>
constexpr static auto conditional_type_fn = []() {
  if constexpr (cond) {
    return std::function<void(T)>{};
  } else {
    return F{};
  }
};

template <bool cond, typename T, typename F>
using conditional_t = decltype(conditional_type_fn<cond, T, F>());

template<typename State_T = void>
auto test(){
  constexpr bool is_userdef = !std::is_void_v<State_T>;
  using Callback_t = conditional_t<is_userdef, std::function<void(State_T)>, void>;
}

int main() {
  test();
  return 0;
}

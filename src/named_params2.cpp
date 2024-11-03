#include "common.hpp"

#include <cassert>
#include <experimental/meta>
#include <initializer_list>
#include <memory>
#include <print>

template<fixed_string Name, typename T>
struct param_struct {
   inline static constexpr auto name = Name;
   T value;
};

template<fixed_string name, typename T>
constexpr auto param(T&& value)
{
   return param_struct<name, decltype(std::forward<T>(value))>{std::forward<T>(value)};
}

template<typename T>
inline static constexpr bool is_named_param = false;

template<fixed_string Name, typename T>
inline static constexpr bool is_named_param<param_struct<Name, T>> = true;

template<std::meta::info Info, fixed_string... Names>
consteval auto get_param_mapping(std::size_t start_offset)
{
   static constexpr auto max_size = std::ranges::max({Names.size...});
   std::array<std::size_t, sizeof...(Names)> to_ret;
   std::size_t loc = 0;
   [:expand(std::initializer_list<fixed_string<max_size>>{Names...}):] >> [&]<auto Name> {
      static constexpr std::size_t name_index = []() {
         const auto params = std::meta::parameters_of(Info);
         const auto param_loc = std::ranges::find(params, Name.view(), std::meta::identifier_of);
         return std::ranges::distance(params.begin(), param_loc);
      }();
      static_assert(name_index != std::meta::parameters_of(Info).size(), "Illegal parameter name");
      to_ret[name_index - start_offset] = loc;
      loc += 1;
   };
   return to_ret;
}

template<std::array ParamMapping, typename... ParamTypes>
constexpr auto get_tuple_params(ParamTypes&&... args)
{
   return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return std::forward_as_tuple(std::forward_like<ParamTypes...[ParamMapping[I]]>(args...[ParamMapping[I]])...);
   }(std::make_index_sequence<sizeof...(ParamTypes)>{});
}

consteval std::meta::info make_index_seq_impl(std::size_t low, std::size_t high)
{
   std::vector<std::meta::info> indicies;
   for (std::size_t i = low; i < high; ++i) {
      indicies.push_back(std::meta::reflect_value(i));
   }
   return std::meta::substitute(^^std::index_sequence, indicies);
}

template<std::size_t Low, std::size_t High>
using make_index_seq = [:make_index_seq_impl(Low, High):];

template<std::meta::info Info, typename Self, typename... ParamTypes>
constexpr decltype(auto) call_with_param_names_impl(Self&& self, ParamTypes&&... args)
{
   static constexpr std::array<bool, sizeof...(ParamTypes)> is_named{is_named_param<ParamTypes>...};
   static constexpr int first_named_param = std::ranges::distance(is_named.begin(), std::ranges::find(is_named, true));
   static constexpr int first_value_param = std::ranges::distance(is_named.begin(), std::ranges::find(is_named, false));
   static constexpr int last_named_param
      = std::ranges::distance(is_named.begin(), std::ranges::find(std::ranges::views::reverse(is_named), true).base());
   static constexpr int last_value_param
      = std::ranges::distance(is_named.begin(), std::ranges::find(std::ranges::views::reverse(is_named), false).base());
   static_assert(first_named_param >= last_value_param, "Named parameters must come after value parameters");

   const auto get_value_params = [&]<std::size_t... I>(std::index_sequence<I...>) {
      return std::forward_as_tuple(std::forward<ParamTypes...[I]>(args...[I])...);
   };
   const auto get_named_params = [&]<std::size_t... I>(std::index_sequence<I...>) {
      // clang-format off
      static constexpr auto param_mapping = get_param_mapping<Info, ParamTypes...[I]::name...>(first_named_param);
      // clang-format on
      return get_tuple_params<param_mapping>(std::forward_like<ParamTypes...[I]>(args...[I].value)...);
   };

   // clang-format off
   if constexpr (std::same_as<std::remove_cvref_t<Self>, std::monostate>) {
      return std::apply(&[:Info:],
                        std::tuple_cat(
                           get_value_params(std::make_index_sequence<last_value_param>{}),
                           get_named_params(make_index_seq<first_named_param, last_named_param>{})));
   }
   else {
      return std::apply(&[:Info:],
                        std::tuple_cat(
                           std::forward_as_tuple(std::forward<Self>(self)),
                           get_value_params(std::make_index_sequence<last_value_param>{}),
                           get_named_params(make_index_seq<first_named_param, last_named_param>{})));
   }
   // clang-format on
}

template<std::meta::info Info, typename... ParamTypes>
   requires(std::meta::is_function(Info))
constexpr decltype(auto) call_with_param_names(ParamTypes&&... args)
{
   if constexpr (std::meta::is_class_member(Info) && !std::meta::is_static_member(Info)) {
      // Member function
      static_assert(std::convertible_to<
                    std::add_lvalue_reference_t<std::remove_cvref_t<ParamTypes...[0]>>,
                    std::add_lvalue_reference_t<[:std::meta::parent_of(Info):]>>);
      static_assert(sizeof...(ParamTypes) - 1 == std::meta::parameters_of(Info).size());
      return call_with_param_names_impl<Info>(std::forward<ParamTypes>(args)...);
   }
   else {
      // Normal function
      static_assert(sizeof...(ParamTypes) == std::meta::parameters_of(Info).size());
      return call_with_param_names_impl<Info>(std::monostate{}, std::forward<ParamTypes>(args)...);
   }
}

constexpr struct {
} named_constructor;

template<typename T>
consteval bool has_annotation(std::meta::info info, const T& to_check)
{
   for (const auto& annotation : std::meta::annotations_of(info)) {
      if (std::meta::value_of(annotation) == std::meta::reflect_value(to_check)) {
         return true;
      }
   }
   return false;
}

// Can't query annotations of a constructor template so this only works for non-templated constructors
template<typename ToConstruct, typename... ParamTypes>
   requires(std::is_class_v<ToConstruct>)
[[nodiscard]] constexpr ToConstruct named_construct(ParamTypes&&... args)
{
   static constexpr auto constructor = []() {
      const auto named_constructor_range
         = std::meta::members_of(^^ToConstruct)
         | std::views::filter([](const auto& info) { return std::meta::is_constructor(info); })
         | std::views::filter([](const auto& info) { return has_annotation(info, named_constructor); })
         | std::ranges::to<std::vector>();
      assert(std::ranges::distance(named_constructor_range) == 1);
      return named_constructor_range[0];
   }();
   // Copy paste from above :(
   // Really don't see a way to make these into a common function since we can't splice the constructor
   static constexpr std::array<bool, sizeof...(ParamTypes)> is_named{is_named_param<ParamTypes>...};
   static constexpr int first_named_param = std::ranges::distance(is_named.begin(), std::ranges::find(is_named, true));
   static constexpr int first_value_param = std::ranges::distance(is_named.begin(), std::ranges::find(is_named, false));
   static constexpr int last_named_param
      = std::ranges::distance(is_named.begin(), std::ranges::find(std::ranges::views::reverse(is_named), true).base());
   static constexpr int last_value_param
      = std::ranges::distance(is_named.begin(), std::ranges::find(std::ranges::views::reverse(is_named), false).base());
   static_assert(first_named_param >= last_value_param, "Named parameters must come after value parameters");

   const auto get_value_params = [&]<std::size_t... I>(std::index_sequence<I...>) {
      return std::forward_as_tuple(std::forward<ParamTypes...[I]>(args...[I])...);
   };
   const auto get_named_params = [&]<std::size_t... I>(std::index_sequence<I...>) {
      // clang-format off
      static constexpr auto param_mapping = get_param_mapping<constructor, ParamTypes...[I]::name...>(first_named_param);
      // clang-format on
      return get_tuple_params<param_mapping>(std::forward_like<ParamTypes...[I]>(args...[I].value)...);
   };

   const auto builder = []<typename... U>(U&&... args) {
      // Use parens since it could have an initializer_list overload
      return ToConstruct(std::forward<U>(args)...);
   };

   return std::apply(
      builder,
      std::tuple_cat(
         get_value_params(std::make_index_sequence<last_value_param>{}),
         get_named_params(make_index_seq<first_named_param, last_named_param>{})));
}

void func(int a, int b, int c) { std::println("in func with a = {}, b = {}, c = {}", a, b, c); }

struct s {
   // clang-format off
   [[=named_constructor]] s(int a, int b, int c) { std::println("in s::s with a = {}, b = {}, c = {}", a, b, c); }
   // clang-format on

   s() {}

   void func(int a, int b, int c) { std::println("in s::func with a = {}, b = {}, c = {}", a, b, c); }
};

constexpr int add(int lhs, int rhs) { return lhs + rhs; }

int main()
{
   call_with_param_names<^^func>(10, param<"c">(30), param<"b">(20));
   call_with_param_names<^^s::func>(s{}, 10, 20, param<"c">(30));
   static_assert(call_with_param_names<^^add>(param<"rhs">(20), param<"lhs">(10)) == 30);
   [[maybe_unused]] const auto my_s = named_construct<s>(10, param<"c">(30), param<"b">(20));
}

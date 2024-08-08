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

template<std::meta::info Info, std::size_t StartOffset, fixed_string... Names>
consteval auto get_param_mapping()
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
      to_ret[name_index - StartOffset] = loc;
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

template<std::size_t Low, std::size_t High>
consteval std::meta::info make_index_seq_impl()
{
   std::vector<std::meta::info> indicies;
   for (std::size_t i = Low; i < High; ++i) {
      indicies.push_back(std::meta::reflect_value(i));
   }
   return std::meta::substitute(^std::index_sequence, indicies);
}

template<std::size_t Low, std::size_t High>
using make_index_seq = [:make_index_seq_impl<Low, High>():];

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
      static constexpr auto param_mapping = get_param_mapping<Info, first_named_param, ParamTypes...[I] ::name...>();
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
   if constexpr (
      std::meta::is_function(Info) && std::meta::is_class_member(Info) && !std::meta::is_static_member(Info)) {
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

void func(int a, int b, int c) { std::println("in func with a = {}, b = {}, c = {}", a, b, c); }

struct s {
   void func(int a, int b, int c) { std::println("in s::func with a = {}, b = {}, c = {}", a, b, c); }
};

int main()
{
   call_with_param_names<^func>(10, param<"c">(30), param<"b">(20));
   call_with_param_names<^s::func>(s{}, 10, 20, param<"c">(30));
}

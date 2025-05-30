#include "common.hpp"

#include <array>
#include <cassert>
#include <experimental/meta>
#include <optional>
#include <ranges>
#include <type_traits>

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
consteval std::optional<std::array<std::size_t, sizeof...(Names)>> get_param_mapping(std::size_t start_offset)
{
   static constexpr auto max_size = std::ranges::max({Names.size...});
   std::array<std::size_t, sizeof...(Names)> to_ret;
   std::size_t loc = 0;
   template for (constexpr auto name : std::to_array<fixed_string<max_size>>({Names...}))
   {
      static constexpr std::size_t name_index = [&]() {
         const auto params = std::meta::parameters_of(Info);
         const auto param_loc = std::ranges::find(params, name.view(), std::meta::identifier_of);
         return std::ranges::distance(params.begin(), param_loc);
      }();
      if (name_index == std::meta::parameters_of(Info).size()) {
         return std::nullopt;
      }
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
      indicies.push_back(std::meta::reflect_constant(i));
   }
   return std::meta::substitute(^^std::index_sequence, indicies);
}

template<std::size_t Low, std::size_t High>
using make_index_seq = [:make_index_seq_impl(Low, High):];

// TODO: noexcept specifier?  Would be extremely difficult though...
// TODO: default parameters
// This also messes up when things have the same number of arguments of differing types
// Fixing this would be non-trivial
template<typename ToConstruct, typename... ParamTypes>
   requires(std::is_class_v<ToConstruct>)
[[nodiscard]] constexpr ToConstruct named_construct(ParamTypes&&... args)
{
   static constexpr auto constructors = []() {
      const auto named_constructor_range
         = std::meta::members_of(^^ToConstruct, std::meta::access_context::unchecked())
         | std::views::filter([](const auto& info) {
              return std::meta::is_constructor(info) || std::meta::is_constructor_template(info);
           })
         | std::views::filter(
              [](const auto& info) { return std::meta::parameters_of(info).size() == sizeof...(ParamTypes); })
         | std::ranges::to<std::vector>();
      return define_static_array(named_constructor_range);
   }();

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
      static constexpr auto param_mappings = []() {
         // clang-format off
         std::array<std::optional<std::array<std::size_t, sizeof...(I)>>, constructors.size()> to_ret;
         std::size_t i = 0;
         template for(constexpr auto constructor : constructors) {
            to_ret[i] = get_param_mapping<constructor, ParamTypes...[I]::name...>(first_named_param);
            ++i;
         }
         // clang-format on
         return to_ret;
      }();

      static constexpr auto index = []() {
         std::size_t valid_index = -1;
         for (std::size_t i = 0; i < param_mappings.size(); ++i) {
            if (param_mappings[i].has_value()) {
               assert(valid_index == -1);
               valid_index = i;
            }
         }
         // for (auto&& [i, param_map] : std::views::enumerate(param_mappings)) {
         //    if (param_map.has_value()) {
         //       assert(valid_index == -1);
         //       valid_index = i;
         //    }
         // }
         return valid_index;
      }();

      // static constexpr auto valid_mappings = param_mappings | std::views::filter([](const auto& opt) {
      // opt.has_value(); });

      return get_tuple_params<*param_mappings[index]>(std::forward_like<ParamTypes...[I]>(args...[I].value)...);
   };

   const auto builder = []<typename... U>(U&&... args) noexcept(std::is_nothrow_constructible_v<ToConstruct, U...>) {
      // Use parens since it could have an initializer_list overload
      return ToConstruct(std::forward<U>(args)...);
   };

   return std::apply(
      builder,
      std::tuple_cat(
         get_value_params(std::make_index_sequence<last_value_param>{}),
         get_named_params(make_index_seq<first_named_param, last_named_param>{})));
}

struct s {
   constexpr s(int x, int y, int mult) noexcept : x{x * mult}, y{y * mult} {}

   constexpr s(int x, int y) noexcept : x{x}, y{y} {}

   int x;
   int y;
};

int main()
{
   constexpr auto s1 = named_construct<s>(param<"y">(20), param<"x">(10));
   static_assert(s1.x == 10 && s1.y == 20);
   constexpr auto s2 = named_construct<s>(10, 20, param<"mult">(2));
   static_assert(s2.x == 20 && s2.y == 40);
}

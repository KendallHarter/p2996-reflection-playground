#include "common.hpp"

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

template<std::meta::info Info, fixed_string... Names>
consteval auto get_param_mapping()
{
   static constexpr auto max_size = std::ranges::max({Names.size()...});
   std::array<std::size_t, sizeof...(Names)> to_ret;
   std::size_t loc = 0;
   template for (constexpr auto name : std::to_array<fixed_string<max_size>>({Names...}))
   {
      static constexpr std::size_t name_index = [&]() {
         const auto params = std::meta::parameters_of(Info);
         const auto param_loc = std::ranges::find(params, name.view(), std::meta::identifier_of);
         return std::ranges::distance(params.begin(), param_loc);
      }();
      static_assert(name_index != std::meta::parameters_of(Info).size(), "Illegal parameter name");
      to_ret[name_index] = loc;
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

consteval bool all_params_have_consistent_identifiers(std::meta::info func)
{
   const auto params = std::meta::parameters_of(func);
   for (const auto& param : params) {
      if (!std::meta::has_identifier(param)) {
         return false;
      }
   };
   return true;
}

consteval bool all_params_have_identifiers(std::meta::info func)
{
   const auto params = std::meta::parameters_of(func);
   for (const auto& param : params) {
      if (!std::meta::has_identifier(param)) {
         return false;
      }
   };
   return true;
}

template<std::meta::info Info, typename... ParamTypes>
   requires(
      std::meta::is_function(Info) && all_params_have_consistent_identifiers(Info) && all_params_have_identifiers(Info))
constexpr decltype(auto) call_with_param_names(ParamTypes&&... args)
{
   if constexpr (
      std::meta::is_function(Info) && std::meta::is_class_member(Info) && !std::meta::is_static_member(Info)) {
      // Member function
      using parent = [:std::meta::parent_of(Info):];
      static_assert(std::convertible_to<
                    std::add_lvalue_reference_t<std::remove_cvref_t<ParamTypes...[0]>>,
                    std::add_lvalue_reference_t<parent>>);
      static_assert(sizeof...(ParamTypes) - 1 == std::meta::parameters_of(Info).size());
      return []<typename ObjType, typename... MemFuncParams>(
                ObjType&& object, MemFuncParams&&... rest) -> decltype(auto) {
         static constexpr auto param_mapping = get_param_mapping<Info, std::remove_cvref_t<MemFuncParams>::name...>();
         // clang-format off
         return std::apply(
            &[:Info:],
            std::tuple_cat(
               std::forward_as_tuple(std::forward<ObjType>(object)),
               get_tuple_params<param_mapping>(std::forward_like<MemFuncParams>(rest.value)...)));
         // clang-format on
      }(std::forward<ParamTypes>(args)...);
   }
   else {
      // Normal function
      static_assert(sizeof...(ParamTypes) == std::meta::parameters_of(Info).size());
      static constexpr auto param_mapping = get_param_mapping<Info, std::remove_cvref_t<ParamTypes>::name...>();
      return std::apply([:Info:], get_tuple_params<param_mapping>(std::forward_like<ParamTypes>(args.value)...));
   }
}

void test(int a, int b, int see, std::unique_ptr<int> ptr)
{
   std::println("test with a = {}, b = {}, see = {}, *ptr = {}", a, b, see, *ptr);
}

struct s {
   void func(int x) { std::println("s::func with x = {}", x); }
   static void func2(int b, int c) { std::println("s::func2 with b = {}, c = {}", b, c); }
};

struct derived : s {};

int main()
{
   call_with_param_names<^^test>(param<"see">(3), param<"a">(1), param<"ptr">(std::make_unique<int>(3)), param<"b">(2));
   call_with_param_names<^^s::func2>(param<"c">(10), param<"b">(5));
   call_with_param_names<^^s::func>(s{}, param<"x">(40));
   call_with_param_names<^^s::func>(derived{}, param<"x">(20));
}

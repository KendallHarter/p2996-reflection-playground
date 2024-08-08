#include <experimental/meta>
#include <initializer_list>
#include <memory>
#include <print>

template<std::size_t N>
struct fixed_string {
   constexpr fixed_string() noexcept { std::ranges::fill(storage_, '\0'); }

   constexpr fixed_string(const char (&val)[N]) noexcept { std::ranges::copy(val, storage_); }

   template<std::size_t ToSize>
      requires(ToSize > N)
   constexpr operator fixed_string<ToSize>() const noexcept
   {
      fixed_string<ToSize> to_ret;
      std::ranges::copy(storage_, std::begin(to_ret.storage_));
      return to_ret;
   }

   friend bool operator==(const fixed_string&, const fixed_string&) = default;

   constexpr std::string_view view() const noexcept { return std::string_view{storage_}; }

   inline static constexpr std::size_t size = N;

   char storage_[N];
};

namespace __impl {
template<auto... vals>
struct replicator_type {
   template<typename F>
   constexpr void operator>>(F body) const
   {
      (body.template operator()<vals>(), ...);
   }
};

template<auto... vals>
replicator_type<vals...> replicator = {};
} // namespace __impl

template<typename R>
consteval auto expand(R range)
{
   std::vector<std::meta::info> args;
   for (auto r : range) {
      args.push_back(std::meta::reflect_value(r));
   }
   return substitute(^__impl::replicator, args);
}

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

template<std::meta::info Info, typename... ParamTypes>
   requires(std::meta::is_function(Info))
constexpr decltype(auto) call_with_param_names(ParamTypes&&... args)
{
   if constexpr (
      std::meta::is_function(Info) && std::meta::is_class_member(Info) && !std::meta::is_static_member(Info)) {
      // Member function
      static_assert(std::same_as<std::remove_cvref_t<ParamTypes...[0]>, [:std::meta::parent_of(Info):]>);
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

int main()
{
   call_with_param_names<^test>(param<"see">(3), param<"a">(1), param<"ptr">(std::make_unique<int>(3)), param<"b">(2));
   call_with_param_names<^s::func2>(param<"c">(10), param<"b">(5));
   call_with_param_names<^s::func>(s{}, param<"x">(40));
}

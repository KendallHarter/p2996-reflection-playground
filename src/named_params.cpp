#include <experimental/meta>
#include <initializer_list>
#include <print>
#include <memory>

template<std::size_t N>
struct fixed_string {
   constexpr fixed_string() noexcept
   {
      std::ranges::fill(storage_, '\0');
   }

   constexpr fixed_string(const char (&val)[N]) noexcept
   {
      std::ranges::copy(val, storage_);
   }

   template<std::size_t ToSize>
      requires (ToSize > N)
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
      constexpr void operator>>(F body) const {
         (body.template operator()<vals>(), ...);
      }
   };

   template<auto... vals>
   replicator_type<vals...> replicator = {};
}

template<typename R>
consteval auto expand(R range) {
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

template<typename... T>
struct TD;

template<std::meta::info Info, typename... ParamTypes>
constexpr decltype(auto) call_with_param_names(ParamTypes&&... args)
{
   static_assert(sizeof...(ParamTypes) == std::meta::parameters_of(Info).size());
   static constexpr auto max_size = std::ranges::max({std::remove_cvref_t<ParamTypes>::name.size...});
   static constexpr auto param_mapping = [&]() {
      std::array<std::size_t, sizeof...(ParamTypes)> to_ret;
      std::size_t loc = 0;
      [:expand(std::initializer_list<fixed_string<max_size>>{std::remove_cvref_t<ParamTypes>::name...}):] >> [&]<auto Name> {
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
   }();

   auto tuple_params = std::forward_as_tuple(std::forward_like<ParamTypes>(args.value)...);
   const auto get_tuple_params = [&]<std::size_t... I>(const std::index_sequence<I...>) {
      return std::forward_as_tuple(std::get<param_mapping[I]>(std::forward_like<ParamTypes...[param_mapping[I]]>(tuple_params))...);
   };

   return std::apply([:Info:], get_tuple_params(std::make_index_sequence<param_mapping.size()>{}));
}

void test(int a, int b, int see, std::unique_ptr<int> ptr)
{
   std::println("a = {}, b = {}, see = {}, *ptr = {}", a, b, see, *ptr);
}

int main()
{
   call_with_param_names<^test>(param<"see">(3), param<"a">(1), param<"ptr">(std::make_unique<int>(3)), param<"b">(2));
}

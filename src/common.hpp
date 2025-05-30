#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <experimental/meta>
#include <ranges>
#include <string_view>

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

namespace impl {
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

template<typename T, T... Values>
inline constexpr T fixed_array[sizeof...(Values)]{Values...};
} // namespace impl

template<std::ranges::input_range R>
consteval std::meta::info reflect_constant_array(R&& r)
{
   auto args = std::vector<std::meta::info>{^^std::ranges::range_value_t<R>};
   for (auto&& elem : r) {
      args.push_back(r);
   }
   return std::meta::substitute(^^impl::fixed_array, args);
}

template<std::ranges::input_range R>
consteval std::span<const std::ranges::range_value_t<R>> define_static_array(R&& r)
{
   using T = std::ranges::range_value_t<R>;

   auto array = reflect_constant_array(r);

   return {std::meta::extract<const T*>(array), std::meta::extent(std::meta::type_of(array))};
}

template<typename R>
consteval auto expand(R range)
{
   std::vector<std::meta::info> args;
   for (auto r : range) {
      args.push_back(std::meta::reflect_constant(r));
   }
   return std::meta::substitute(^^impl::replicator, args);
}

#endif // COMMON_HPP

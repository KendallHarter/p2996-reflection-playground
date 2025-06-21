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

   template<std::ranges::input_range Range>
      requires std::convertible_to<std::ranges::range_value_t<Range>, char>
   constexpr fixed_string(Range&& r) noexcept : fixed_string{}
   {
      std::ranges::copy(r, storage_);
   }

   template<std::size_t ToSize>
      requires(ToSize > N)
   constexpr operator fixed_string<ToSize>() const noexcept
   {
      fixed_string<ToSize> to_ret;
      std::ranges::copy(storage_, std::begin(to_ret.storage_));
      return to_ret;
   }

   friend constexpr bool operator==(const fixed_string& lhs, const fixed_string& rhs) = default;

   constexpr std::string_view view() const noexcept { return std::string_view{storage_}; }

   constexpr std::size_t size() const noexcept { return N; }

   char storage_[N];
};

template<fixed_string Str>
consteval auto shrink_fixed_string()
{
   static constexpr auto end = std::ranges::distance(std::begin(Str.storage_), std::ranges::find(Str.storage_, '\0'));
   return fixed_string<end + 1>{std::span<const char>(std::begin(Str.storage_), end + 1)};
}

template<fixed_string... Strs>
consteval auto make_uniform_fixed_strings()
{
   static constexpr auto max_size = std::ranges::max({Strs.size()...});
   return std::array<fixed_string<max_size>, sizeof...(Strs)>{Strs...};
}

template<fixed_string... Strs>
consteval auto append_fixed_strings()
{
   // Subtract sizeof...(Strs) to remove the nulls, then append 1 for the final null
   static constexpr auto size = (Strs.size() + ...) - sizeof...(Strs) + 1;
   auto to_ret = fixed_string<size>{};
   auto write_loc = std::begin(to_ret.storage_);
   // if only std::concat was supported in Clang...
   for (const auto& str : make_uniform_fixed_strings<Strs...>()) {
      write_loc = std::ranges::copy(str.view(), write_loc).out;
   }
   return to_ret;
}

namespace impl {

template<typename T, T... Values>
inline constexpr T fixed_array[sizeof...(Values)]{Values...};

template<typename T, T... Vs>
constexpr T fixed_str[sizeof...(Vs) + 1]{Vs..., '\0'};

} // namespace impl

template<std::ranges::input_range R>
consteval std::meta::info reflect_constant_array(R&& r)
{
   auto args = std::vector<std::meta::info>{^^std::ranges::range_value_t<R>};
   for (auto&& elem : r) {
      args.push_back(std::meta::reflect_constant(elem));
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

consteval auto reflect_constant_string(std::string_view v) -> std::meta::info
{
   auto args = std::vector<std::meta::info>{^^char};
   for (auto&& elem : v) {
      args.push_back(std::meta::reflect_constant(elem));
   }
   return std::meta::substitute(^^impl::fixed_str, args);
}

template<typename T>
struct tdef {
   using type = T;
};

#endif // COMMON_HPP

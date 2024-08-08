#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <experimental/meta>
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
   return std::meta::substitute(^__impl::replicator, args);
}

#endif // COMMON_HPP

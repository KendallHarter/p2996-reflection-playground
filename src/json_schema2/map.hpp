#ifndef KHCT_MAP_HPP
#define KHCT_MAP_HPP

#include "common.hpp"

#include <algorithm>
#include <compare>
#include <numeric>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace khct {

template<typename Key, typename Value, std::size_t Size, typename Comp>
   requires(std::is_empty_v<Comp>)
struct map {
   consteval map() noexcept : values_{} {}

   consteval map(const std::pair<Key, Value> (&init)[Size]) noexcept
   {
      std::ranges::copy(init, std::begin(values_));
      std::ranges::sort(values_, Comp{});
   }

   constexpr std::optional<Value> operator[](const Key& k) const noexcept
   {
      const auto loc = std::lower_bound(
         std::begin(values_), std::end(values_), k, [](const auto& a, const auto& b) { return Comp{}(a.first, b); });
      if (loc == std::end(values_)) {
         return std::nullopt;
      }
      return loc->second;
   }

   constexpr auto begin() const noexcept { return values_; }
   constexpr auto begin() noexcept { return values_; }
   constexpr auto end() const noexcept { return &values_[Size]; }
   constexpr auto end() noexcept { return &values_[Size]; }
   constexpr auto size() noexcept { return Size; }

   std::pair<Key, Value> values_[Size];
};

template<typename Key, typename Value, std::size_t Size1, std::size_t Size2, typename Comp>
consteval auto merge(const map<Key, Value, Size1, Comp>& lhs, const map<Key, Value, Size2, Comp>& rhs)
   -> map<Key, Value, Size1 + Size2, Comp>
{
   map<Key, Value, Size1 + Size2, Comp> to_ret;
   std::ranges::copy(lhs, to_ret.begin());
   std::ranges::copy(rhs, to_ret.begin() + Size1);
   std::ranges::sort(to_ret, Comp{});
   return to_ret;
}

template<typename Key, typename Value, std::size_t Size, typename Comp = std::less<void>>
   requires(std::is_empty_v<Comp>)
consteval auto make_map(const std::pair<Key, Value> (&init)[Size], Comp = std::less<void>{}) noexcept
   -> map<Key, Value, Size, Comp>
{
   return map<Key, Value, Size, Comp>{init};
}

template<
   typename Key,
   typename Comp,
   std::size_t Size,
   std::array<Key, Size> Keys,
   std::array<std::size_t, Size> Mapping,
   auto... Values>
   requires(sizeof...(Values) == Size && std::is_empty_v<Comp>)
struct multi_type_map {

   template<auto KeyToFind>
      requires weakly_equality_comparible_with<decltype(KeyToFind), Key> && requires(decltype(KeyToFind) k, Key k2) {
         { k < k2 } -> std::convertible_to<bool>;
      }
   consteval auto get_key() const noexcept
   {
      constexpr auto loc = std::lower_bound(std::begin(Keys), std::end(Keys), KeyToFind, Comp{});
      if constexpr (loc == std::end(Keys) || *loc != KeyToFind) {
         return nil;
      }
      else {
         constexpr auto index = std::distance(std::begin(Keys), loc);
         return std::get<Mapping[index]>(std::tuple{Values...});
      }
   }

   template<std::size_t I>
   consteval auto get() const noexcept
   {
      // Do mapping in this weird way to keep everything in order
      return pair{Keys[Mapping[I]], std::get<I>(std::tuple{Values...})};
   }

   constexpr auto size() const noexcept { return Size; }

   friend auto operator<=>(const multi_type_map&, const multi_type_map&) noexcept = default;
};

template<pair... Pairs, typename Comp = std::less<void>>
   requires(has_common_type<decltype(Pairs.first)...> && std::is_empty_v<Comp>)
consteval auto make_multi_type_map(Comp = std::less<void>{})
{
   using key_type = std::common_type_t<decltype(Pairs.first)...>;
   constexpr pair sorted_keys_and_indexes = []() {
      std::array<std::size_t, sizeof...(Pairs)> indexes;
      std::iota(indexes.begin(), indexes.end(), 0);
      std::array<key_type, sizeof...(Pairs)> keys{Pairs.first...};
      // need to sort the indexes before the keys
      std::ranges::sort(indexes, [&](std::size_t a, std::size_t b) { return Comp{}(keys[a], keys[b]); });
      std::ranges::sort(keys, Comp{});
      return pair{keys, indexes};
   }();
   constexpr auto keys = sorted_keys_and_indexes.first;
   constexpr auto indexes = sorted_keys_and_indexes.second;
   return multi_type_map<key_type, Comp, sizeof...(Pairs), keys, indexes, Pairs.second...>{};
}

// namespace detail {
// template<typename KeyType, KeyType KeyValue>
// struct get_struct {
//    template<
//       typename Comp,
//       std::size_t Size,
//       std::array<KeyType, Size> Keys,
//       std::array<std::size_t, Size> Mapping,
//       auto... Values>
//    consteval auto operator()(const multi_type_map<KeyType, Comp, Size, Keys, Mapping, Values...>& map) const noexcept
//    {
//       return map.template get<KeyValue>();
//    }

//    template<
//       typename Comp,
//       std::size_t Size,
//       std::array<KeyType, Size> Keys,
//       std::array<std::size_t, Size> Mapping,
//       auto... Values>
//    friend consteval auto operator|(const multi_type_map<KeyType, Comp, Size, Keys, Mapping, Values...>& map,
//    get_struct)
//    {
//       return map.template get<KeyValue>();
//    }
// };
// } // namespace detail

// template<auto KeyValue>
// constexpr inline auto get = detail::get_struct<decltype(KeyValue), KeyValue>{};

} // namespace khct

template<
   typename Key,
   typename Comp,
   std::size_t Size,
   std::array<Key, Size> Keys,
   std::array<std::size_t, Size> Mapping,
   auto... Values>
struct std::tuple_size<khct::multi_type_map<Key, Comp, Size, Keys, Mapping, Values...>>
   : std::integral_constant<std::size_t, Size> {};

template<
   std::size_t I,
   typename Key,
   typename Comp,
   std::size_t Size,
   std::array<Key, Size> Keys,
   std::array<std::size_t, Size> Mapping,
   auto... Values>
struct std::tuple_element<I, khct::multi_type_map<Key, Comp, Size, Keys, Mapping, Values...>> {
   using type
      = decltype(std::declval<khct::multi_type_map<Key, Comp, Size, Keys, Mapping, Values...>>().template get<I>());
};

#endif // KHCT_MAP_HPP

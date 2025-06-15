#ifndef KHCT_COMMON_HPP
#define KHCT_COMMON_HPP

#include <compare>
#include <concepts>
#include <type_traits>
#include <utility>

namespace khct {

struct nil_t {
   friend constexpr bool operator==(const nil_t&, const nil_t&) noexcept { return true; }

   // No other types compare true to nil aside from nil
   template<typename T>
      requires(!std::same_as<std::remove_cvref_t<T>, nil_t>)
   friend constexpr bool operator==(const nil_t&, T other) noexcept
   {
      return false;
   }

   template<typename T>
      requires(!std::same_as<std::remove_cvref_t<T>, nil_t>)
   friend constexpr bool operator==(T other, const nil_t&) noexcept
   {
      return false;
   }
};
inline constexpr auto nil{nil_t{}};

/// @brief A simple pair to use as template parameters because std::pair can't be
/// @tparam T1 The first type
/// @tparam T2 The second type
template<typename T1, typename T2>
struct pair {
   [[no_unique_address]] T1 first;
   [[no_unique_address]] T2 second;
   friend constexpr auto operator<=>(const pair&, const pair&) noexcept = default;

   template<std::size_t I>
   constexpr const auto& get() const noexcept
   {
      if constexpr (I == 0) {
         return first;
      }
      else if constexpr (I == 1) {
         return second;
      }
      throw "invalid index";
   }
};

// clang-format off
template<typename T, typename U>
concept weakly_equality_comparible_with = requires (const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u) {
   { t == u } -> std::convertible_to<bool>;
   { t != u } -> std::convertible_to<bool>;
   { u == t } -> std::convertible_to<bool>;
   { u != t } -> std::convertible_to<bool>;
};
// clang-format on

namespace detail {

template<std::size_t I, auto First, auto... Rest>
consteval auto get_at_index() noexcept
{
   if constexpr (I == 0) {
      return First;
   }
   else {
      return get_at_index<I - 1, Rest...>();
   }
}

template<typename T>
struct type_holder {
   using type = T;
};

template<typename T, std::size_t I>
struct tuple_base {
   // TODO: Move only types?
   // This is needed for some reason???
   constexpr tuple_base(const T& val) : value{val} {}

   [[no_unique_address]] T value;
   friend constexpr bool operator==(const tuple_base&, const tuple_base&) noexcept = default;
   friend constexpr auto operator<=>(const tuple_base&, const tuple_base&) noexcept = default;
};

template<pair... TypesAndIndexes>
struct tuple_impl : tuple_base<typename decltype(TypesAndIndexes.first)::type, TypesAndIndexes.second>... {
   template<std::size_t I>
   constexpr const auto& get() const& noexcept
   {
      constexpr auto use_pair = get_at_index<I, TypesAndIndexes...>();
      return static_cast<const tuple_base<typename decltype(use_pair.first)::type, use_pair.second>*>(this)->value;
   }

   template<std::size_t I>
   constexpr auto& get() & noexcept
   {
      constexpr auto use_pair = get_at_index<I, TypesAndIndexes...>();
      return static_cast<tuple_base<typename decltype(use_pair.first)::type, use_pair.second>*>(this)->value;
   }

   template<std::size_t I>
   constexpr auto get() && noexcept
   {
      return get<I>();
   }

   friend constexpr bool operator==(const tuple_impl&, const tuple_impl&) noexcept = default;
   friend constexpr auto operator<=>(const tuple_impl&, const tuple_impl&) noexcept = default;
};

template<typename... Ts, std::size_t... Is>
consteval auto make_tuple(std::index_sequence<Is...>) noexcept
{
   return static_cast<tuple_impl<pair{type_holder<Ts>{}, Is}...>*>(nullptr);
}

} // namespace detail

template<typename... Ts>
struct tuple : std::remove_reference_t<decltype(*detail::make_tuple<Ts...>(std::index_sequence_for<Ts...>{}))> {
   using base = std::remove_reference_t<decltype(*detail::make_tuple<Ts...>(std::index_sequence_for<Ts...>{}))>;

   // TODO: Move only types?
   constexpr tuple(const Ts&... values) noexcept : base{values...} {}

   friend constexpr bool operator==(const tuple&, const tuple&) noexcept = default;
   friend constexpr auto operator<=>(const tuple&, const tuple&) noexcept = default;

   inline static constexpr auto size = sizeof...(Ts);
};

// std::equality_comparable_with is too strong here as it requires there to be a common reference type
// between the two types; we don't want this
template<typename... Ts1, typename... Ts2>
   requires(sizeof...(Ts1) == sizeof...(Ts2) && (weakly_equality_comparible_with<Ts1, Ts2> && ...))
constexpr bool operator==(const tuple<Ts1...>& lhs, const tuple<Ts2...>& rhs) noexcept
{
   return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
      return ((lhs.template get<Is>() == rhs.template get<Is>()) && ...);
   }(std::index_sequence_for<Ts1...>{});
}

template<typename... Ts1, typename... Ts2>
constexpr tuple<Ts1..., Ts2...> tuple_cat(const tuple<Ts1...>& t1, const tuple<Ts2...>& t2) noexcept
{
   return [&]<std::size_t... Is1, std::size_t... Is2>(std::index_sequence<Is1...>, std::index_sequence<Is2...>) {
      return tuple<Ts1..., Ts2...>{t1.template get<Is1>()..., t2.template get<Is2>()...};
   }(std::index_sequence_for<Ts1...>{}, std::index_sequence_for<Ts2...>{});
}

template<std::size_t I, typename... Ts>
constexpr auto get(const tuple<Ts...> t) noexcept
{
   return t.template get<I>();
}

template<typename... Types>
concept has_common_type = requires() { typename std::common_type<Types...>::type; };

namespace detail {

template<typename T, typename...>
struct head_struct {
   using type = T;
};

} // namespace detail

template<typename... Ts>
using head = detail::head_struct<Ts...>::type;

} // namespace khct

template<typename T1, typename T2>
struct std::tuple_size<khct::pair<T1, T2>> : std::integral_constant<std::size_t, 2> {};

template<typename... Ts>
struct std::tuple_size<khct::tuple<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

// Dumb workaround for not being able to reflect type aliases
template<typename T>
struct tdef {
   typedef T type;
};

#endif // KHCT_COMMON_HPP

#ifndef KHCT_STRING_HPP
#define KHCT_STRING_HPP

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <ranges>
#include <string_view>
#include <tuple>

namespace khct {

// It would be nice to be able to make these functions generic across any statically sized structs,
// but it doesn't really seem possible and would be extremely complex if it were
template<std::size_t RawArraySize>
struct string {
   consteval string() : value_{} {}

   consteval string(const char (&c)[RawArraySize]) : value_{} { std::ranges::copy(c, c + RawArraySize, value_); }

   // template<typename Range>
   //    requires std::convertible_to<std::ranges::range_value_t<Range>, char>
   // constexpr string(Range&& r) noexcept : value_{}
   // {
   //    std::ranges::copy(r, value_);
   // }

   // Need + 1 size for the null character
   template<std::size_t Start, std::size_t End>
   consteval string<End - Start + 1> splice() const noexcept
   {
      static_assert(End >= Start);
      static_assert(End <= RawArraySize);
      string<End - Start + 1> to_ret{};
      std::copy(value_ + Start, value_ + End, to_ret.begin());
      return to_ret;
   }

   template<std::size_t Padsize, char Filler = ' '>
   consteval string<Padsize + RawArraySize> pad_left() const noexcept
   {
      string<Padsize + RawArraySize> to_ret{};
      std::fill(to_ret.value_, to_ret.value_ + Padsize, Filler);
      std::ranges::copy(*this, to_ret.value_ + Padsize);
      return to_ret;
   }

   template<std::size_t Padsize, char Filler = ' '>
   consteval string<Padsize + RawArraySize> pad_right() const noexcept
   {
      string<Padsize + RawArraySize> to_ret{};
      std::ranges::copy(*this, to_ret.begin());
      std::fill(to_ret.begin() + RawArraySize - 1, to_ret.end() - 1, Filler);
      return to_ret;
   }

   template<std::size_t NewSize>
      requires(NewSize > RawArraySize)
   consteval operator string<NewSize>() const noexcept
   {
      return pad_right<NewSize - RawArraySize, '\0'>();
   }

   consteval operator std::string_view() const noexcept { return {begin(), std::ranges::find(begin(), end(), '\0')}; }

   consteval std::string_view view() const noexcept { return *this; }

   constexpr auto begin() const noexcept { return &value_[0]; }
   constexpr auto begin() noexcept { return &value_[0]; }
   constexpr auto end() const noexcept { return &value_[RawArraySize]; }
   constexpr auto end() noexcept { return &value_[RawArraySize]; }
   constexpr auto size() const noexcept { return RawArraySize - 1; }
   constexpr auto empty() const noexcept { return size() == 0; }

   constexpr char operator[](std::size_t index) const noexcept { return value_[index]; }

   char value_[RawArraySize];

   friend constexpr bool operator==(const string& lhs, const string& rhs) noexcept { return lhs.view() == rhs.view(); }

   template<std::size_t N>
   friend constexpr auto operator<=>(const string& lhs, const string<N>& rhs) noexcept
   {
      return lhs.view() <=> rhs.view();
   }
};

// I couldn't define this as a inline friend for some reason, unfortunately
template<std::size_t SizeL, std::size_t SizeR>
consteval auto operator+(const string<SizeL>& lhs, const string<SizeR>& rhs) noexcept -> string<SizeL + SizeR - 1>
{
   string<SizeL + SizeR - 1> to_ret;
   std::ranges::copy(lhs, to_ret.begin());
   std::ranges::copy(rhs, to_ret.begin() + SizeL - 1);
   return to_ret;
}

namespace detail {

template<string Str, char SplitChar, std::size_t MaxSplits>
consteval auto split_helper() noexcept
{
   constexpr auto split_loc = std::ranges::find(Str, SplitChar);
   if constexpr (split_loc == std::end(Str)) {
      return std::make_tuple(Str);
   }
   else {
      constexpr auto split_index = std::distance(std::begin(Str), split_loc);
      constexpr auto split = Str.template splice<0, split_index>();
      constexpr auto rest = Str.template splice<split_index + 1, Str.size()>();
      if constexpr (MaxSplits == 1) {
         return std::make_tuple(split, rest);
      }
      else {
         constexpr auto next_split_num = MaxSplits == 0 ? 0 : MaxSplits - 1;
         return std::tuple_cat(std::make_tuple(split), split_helper<rest, SplitChar, next_split_num>());
      }
   }
}

} // namespace detail

template<string Str, char SplitChar, std::size_t MaxSplits = 0>
consteval auto split() noexcept
{
   return detail::split_helper<Str, SplitChar, MaxSplits>();
}

template<string Str>
consteval auto strip_leading_whitespace() noexcept
{
   // TODO: Maybe create a one_of class so this is cleaner (e.g., c == one_of{' ', '\f', ...})
   constexpr auto is_ws
      = [](char c) { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; };
   constexpr auto non_ws_loc = std::ranges::find_if_not(Str, is_ws);
   if constexpr (non_ws_loc == std::end(Str)) {
      return string{""};
   }
   else {
      constexpr auto splice_loc = std::distance(std::begin(Str), non_ws_loc);
      return Str.template splice<splice_loc, Str.size()>();
   }
}

namespace detail {

template<std::size_t Start, std::size_t End>
struct splice_struct {
   template<std::size_t Size>
   consteval auto operator()(const string<Size>& str) const noexcept
   {
      return str.template splice<Start, End>();
   }

   template<std::size_t Size>
   friend consteval auto operator|(const string<Size>& str, splice_struct) noexcept
   {
      return str.template splice<Start, End>();
   }
};

template<std::size_t Padsize, char Filler = ' '>
struct pad_left_struct {
   template<std::size_t Size>
   consteval auto operator()(const string<Size>& str) const noexcept
   {
      return str.template pad_left<Padsize, Filler>();
   }

   template<std::size_t Size>
   friend consteval auto operator|(const string<Size>& str, pad_left_struct) noexcept
   {
      return str.template pad_left<Padsize, Filler>();
   }
};

template<std::size_t Padsize, char Filler = ' '>
struct pad_right_struct {
   template<std::size_t Size>
   consteval auto operator()(const string<Size>& str) const noexcept
   {
      return str.template pad_right<Padsize, Filler>();
   }

   template<std::size_t Size>
   friend consteval auto operator|(const string<Size>& str, pad_right_struct) noexcept
   {
      return str.template pad_right<Padsize, Filler>();
   }
};

} // namespace detail

template<std::size_t Start, std::size_t End>
inline constexpr auto splice = detail::splice_struct<Start, End>{};

template<std::size_t Padsize, char Filler = ' '>
inline constexpr auto pad_left = detail::pad_left_struct<Padsize, Filler>{};

template<std::size_t Padsize, char Filler = ' '>
inline constexpr auto pad_right = detail::pad_right_struct<Padsize, Filler>{};

} // namespace khct

#endif // KHCT_STRING_HPP

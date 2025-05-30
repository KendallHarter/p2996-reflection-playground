#ifndef JSON_PARSE_HPP
#define JSON_PARSE_HPP

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

// map isn't constexpr yet, sadly, so use std::vector<std::pair> instead
// struct json_value
//    : std::variant<
//         std::int64_t,
//         std::string_view,
//         std::map<std::string_view, json_value>,
//         std::vector<json_value>> {
// private:
//    using base = std::variant<
//       std::int64_t,
//       std::string_view,
//       std::map<std::string_view, json_value>,
//       std::vector<json_value>>;
//
// public:
//    using base::base;
// };

struct json_value
   : std::variant<
        std::int64_t,
        std::string_view,
        std::vector<std::pair<std::string_view, json_value>>,
        std::vector<json_value>> {
private:
   using base = std::variant<
      std::int64_t,
      std::string_view,
      std::vector<std::pair<std::string_view, json_value>>,
      std::vector<json_value>>;

public:
   using base::base;
};

using json_array = std::vector<json_value>;
using json_map = std::vector<std::pair<std::string_view, json_value>>;

template<typename Range>
consteval const std::ranges::range_value_t<Range>::second_type& get_by_key(Range&& vals, const std::string_view key)
{
   for (auto&& [comp_key, value] : vals) {
      if (comp_key == key) {
         return value;
      }
   }
   throw std::runtime_error{"no matching key found"};
}

template<class T, class U>
concept weaky_comparable_with = requires(const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u) {
   { t == u } -> std::convertible_to<bool>;
   { t != u } -> std::convertible_to<bool>;
   { u == t } -> std::convertible_to<bool>;
   { u != t } -> std::convertible_to<bool>;
};

template<auto... Values>
struct one_of_struct {
   friend constexpr bool operator==(const auto& val, one_of_struct)
      requires(weaky_comparable_with<decltype(val), decltype(Values)> && ...)
   {
      return ((val == Values) || ...);
   }
};

template<auto... Values>
constexpr auto one_of = one_of_struct<Values...>{};

constexpr std::string_view strip_leading_whitespace(const std::string_view v)
{
   std::size_t index = 0;
   while (index < v.size() && v[index] == one_of<' ', '\f', '\n', '\r', '\t', '\v'>) {
      index += 1;
   }
   return v.substr(index);
}

// Pre: leading whitespace has been removed
// First is the key, second is rest
consteval std::pair<std::string_view, std::string_view> parse_json_str(const std::string_view v)
{
   std::size_t rest = 1;
   while (true) {
      const auto next = v.find('"', rest);
      if (next == std::string_view::npos) {
         throw std::runtime_error{"unterminated string"};
      }
      if (v[next - 1] != '\\') {
         rest = next;
         break;
      }
      rest = next;
   }
   return {v.substr(1, rest - 1), v.substr(rest + 1)};
}

// Pre: leading whitespace has been removed
consteval std::pair<json_value, std::string_view> parse_json_value(const std::string_view v)
{
   if (v.front() == '-' || (v.front() >= '0' && v.front() <= '9')) {
      // number
      std::int64_t value;
      const auto [rest, ec] = std::from_chars(v.data(), v.data() + v.size(), value);
      if (ec != std::errc{}) {
         throw std::runtime_error{"number parse error"};
      }
      return {value, {rest, v.data() + v.size()}};
   }
   else if (v.front() == '"') {
      // string
      return parse_json_str(v);
   }
   else if (v.front() == '{') {
      // object
      const auto next = strip_leading_whitespace(v.substr(1));
      if (next.front() != '"') {
         throw std::runtime_error{"expected string for JSON key"};
      }
      auto next_key = next;
      // std::map<std::string_view, json_value> vals;
      std::vector<std::pair<std::string_view, json_value>> vals;
      while (true) {
         const auto [key, rest] = parse_json_str(next_key);
         const auto colon_next = strip_leading_whitespace(rest);
         if (colon_next.front() != ':') {
            throw std::runtime_error{"expected colon after JSON key"};
         }
         const auto [value, end_value_str] = parse_json_value(strip_leading_whitespace(colon_next.substr(1)));
         // vals.emplace(key, value);
         vals.emplace_back(key, value);
         const auto end = strip_leading_whitespace(end_value_str);
         next_key = end;
         if (end.front() == '}') {
            return {vals, end.substr(1)};
         }
         else if (end.front() == ',') {
            next_key = strip_leading_whitespace(end.substr(1));
         }
         else {
            throw std::runtime_error{"unexpected token after dict value"};
         }
      }
   }
   else if (v.front() == '[') {
      // array
      auto next_val = strip_leading_whitespace(v.substr(1));
      std::vector<json_value> vals;
      while (true) {
         const auto [value, end_val] = parse_json_value(next_val);
         vals.push_back(value);
         const auto end = strip_leading_whitespace(end_val);
         if (end.front() == ']') {
            return {vals, end.substr(1)};
         }
         else if (end.front() == ',') {
            next_val = strip_leading_whitespace(end.substr(1));
         }
         else {
            throw std::runtime_error{"unexpected token after array value"};
         }
      }
   }
   throw std::runtime_error{"unexpected token"};
}

consteval json_value parse_json(const std::string_view v)
{
   const auto [value, rest] = parse_json_value(strip_leading_whitespace(v));
   const auto no_ws_end = strip_leading_whitespace(rest);
   if (!no_ws_end.empty()) {
      throw std::runtime_error{"unexpected data after end of JSON object"};
   }
   return value;
}

#endif // JSON_PARSE_HPP

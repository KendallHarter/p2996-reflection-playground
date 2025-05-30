#include "common.hpp"
#include "runtime_setter_stuff.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <experimental/meta>
#include <map>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>
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

static_assert(parse_json("123") == json_value{123});
static_assert(parse_json(R"( "123" )") == json_value{"123"});
static_assert(parse_json("[1, 2, 3]") == json_value{std::vector<json_value>{1, 2, 3}});
static_assert(
   parse_json("{\"a\": 123}") == json_value{std::vector<std::pair<std::string_view, json_value>>{{"a", 123}}});

constexpr auto type_mapping = std::to_array<std::pair<std::string_view, std::meta::info>>({
   {"i8", std::meta::underlying_entity_of(^^std::int8_t)},
   {"i16", std::meta::underlying_entity_of(^^std::int16_t)},
   {"i32", std::meta::underlying_entity_of(^^std::int32_t)},
   {"i64", std::meta::underlying_entity_of(^^std::int64_t)},
   {"u8", std::meta::underlying_entity_of(^^std::uint8_t)},
   {"u16", std::meta::underlying_entity_of(^^std::uint16_t)},
   {"u32", std::meta::underlying_entity_of(^^std::uint32_t)},
   {"u64", std::meta::underlying_entity_of(^^std::uint64_t)},
});

consteval std::meta::info make_struct_from_json(std::meta::info to_complete, const std::string_view json_str)
{
   const auto json = parse_json(json_str);
   const auto& format_info = get_by_key(std::get<json_map>(json), "format");
   std::vector<std::meta::info> members;
   for (const auto& [name, type_raw] : std::get<json_map>(format_info)) {
      const auto& type = std::get<std::string_view>(type_raw);
      members.push_back({data_member_spec(get_by_key(type_mapping, type), {.name = name})});
   }
   return std::meta::define_aggregate(to_complete, members);
}

template<typename T>
consteval auto make_data_from_json(const std::string_view json_str)
{
   const auto json = parse_json(json_str);
   const auto& data_info = get_by_key(std::get<json_map>(json), "data");
   std::vector<T> to_ret;
   for (const auto& data : std::get<std::vector<json_value>>(data_info)) {
      T to_add;
      for (const auto& [name, value] : std::get<json_map>(data)) {
         // TODO: more complicated stuff
         set_by_name(to_add, name, std::get<std::int64_t>(value));
      }
      to_ret.push_back(to_add);
   }
   return define_static_array(to_ret);
}

constexpr const char struct_info[]{
   R"(
   {
      "format": {
         "x": "i32",
         "y": "i32"
      },
      "data": [
         {
            "x": 1,
            "y": 1
         },
         {
            "x": 2,
            "y": 2
         }
      ]
   }
   )"};

struct point;
consteval { make_struct_from_json(^^point, struct_info); }
constexpr auto data = make_data_from_json<point>(struct_info);

static_assert(data.size() == 2 && data[0].x == 1 && data[0].y == 1 && data[1].x == 2 && data[1].y == 2);

int main() {}

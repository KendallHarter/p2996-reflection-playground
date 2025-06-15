#include "common.hpp"
#include "json_parse.hpp"
#include "runtime_setter_stuff.hpp"

static_assert(parse_json("123") == json_value{123});
static_assert(parse_json(R"( "123" )") == json_value{"123"});
static_assert(parse_json("[1, 2, 3]") == json_value{std::vector<json_value>{1, 2, 3}});
static_assert(
   parse_json("{\"a\": 123}") == json_value{std::vector<std::pair<std::string_view, json_value>>{{"a", 123}}});

constexpr auto type_mapping = std::to_array<std::pair<std::string_view, std::meta::info>>({
   {"i8", ^^tdef<std::int8_t>::type},
   {"i16", ^^tdef<std::int16_t>::type},
   {"i32", ^^tdef<std::int32_t>::type},
   {"i64", ^^tdef<std::int64_t>::type},
   {"u8", ^^tdef<std::uint8_t>::type},
   {"u16", ^^tdef<std::uint16_t>::type},
   {"u32", ^^tdef<std::uint32_t>::type},
   {"u64", ^^tdef<std::uint64_t>::type},
});

consteval std::meta::info make_struct_from_json(std::meta::info to_complete, const std::string_view json_str)
{
   const auto json = parse_json(json_str);
   const auto& format_info = get_by_key(std::get<json_map>(json), "format");
   std::vector<std::meta::info> members;
   for (const auto& [name, type_raw] : std::get<json_map>(format_info)) {
      const auto& type = std::get<std::string_view>(type_raw);
      members.push_back(std::meta::data_member_spec(get_by_key(type_mapping, type), {.name = name}));
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
   return ::define_static_array(to_ret);
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

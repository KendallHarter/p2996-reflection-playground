#include "common.hpp"
#include "json_parse.hpp"
#include "my_tuple.hpp"

#include <cassert>
#include <experimental/meta>

constexpr auto type_mapping = std::to_array<std::pair<std::string_view, std::meta::info>>(
   {{"null", ^^tdef<std::nullptr_t>::type},
    {"boolean", ^^bool},
    {"number", ^^double},
    {"integer", ^^tdef<std::int64_t>::type},
    {"string", ^^tdef<std::string>::type}});

constexpr char basic_array_schema[]{R"(
{
   "$id": "https://example.com/arrays.schema.json",
   "$schema": "https://json-schema.org/draft/2020-12/schema",
   "description": "Arrays of strings and objects",
   "title": "Arrays",
   "type": "object",
   "properties": {
      "fruits": {
         "type": "array",
         "items": {
            "type": "string"
         }
      },
      "vegetables": {
         "type": "array",
         "items": { "$ref": "#/$defs/veggie" }
      }
   },
   "$defs": {
      "veggie": {
         "type": "object",
         "required": [ "veggieName", "veggieLike" ],
         "properties": {
            "veggieName": {
               "type": "string",
               "description": "The name of the vegetable."
            },
            "veggieLike": {
               "type": "boolean",
               "description": "Do I like this vegetable?"
            }
         }
      }
   }
})"};

template<tuple Vals>
consteval auto tuple_names_to_array()
{
   return [&]<std::size_t... I>(std::index_sequence<I...>) {
      const auto names = make_uniform_fixed_strings<Vals.template get<I>().first...>();
      const auto values = std::array{Vals.template get<I>().second...};
      return std::array{pair{names[I], values[I]}...};
   }(std::make_index_sequence<std::tuple_size_v<decltype(Vals)>>{});
}

template<fixed_string name>
struct json_schema_types;

consteval auto get_longest_def_name(const std::string_view schema)
{
   const auto parsed_schema = parse_json(schema);
   const auto defs = std::get<json_map>(get_by_key(std::get<json_map>(parsed_schema), "$defs"));
   return std::ranges::max(defs | std::views::elements<0>, {}, [](const auto& str) { return str.size(); }).size();
}

template<std::size_t LongestLength>
consteval auto get_def_names(const std::string_view schema)
{
   const auto parsed_schema = parse_json(schema);
   const auto defs = std::get<json_map>(get_by_key(std::get<json_map>(parsed_schema), "$defs"));
   std::vector<fixed_string<LongestLength + 1>> names;
   for (const auto [name, value] : defs) {
      names.emplace_back(name);
   }
   return ::define_static_array(names);
}

consteval std::vector<std::meta::info> parse_schema_type(json_map definition);

consteval std::meta::info parse_single_schema_type(
   std::string_view prop_name, json_map prop_desc, const std::vector<std::string_view>& required_items)
{
   const auto type = std::get<std::string_view>(get_by_key(prop_desc, "type"));
   if (std::ranges::find(required_items, prop_name) == required_items.end()) {
      return std::meta::data_member_spec(
         std::meta::substitute(^^std::optional, {get_by_key(type_mapping, type)}), {.name = prop_name});
   }
   else {
      return std::meta::data_member_spec(get_by_key(type_mapping, type), {.name = prop_name});
   }
}

consteval std::vector<std::meta::info> parse_schema_type(json_map definition)
{
   std::vector<std::meta::info> members;
   // neither std::views::transform nor std::ranges::transform were working...
   // const auto required_items
   //    = std::get<json_array>(get_by_key(cur_def, "required"))
   //    | std::views::transform([](const auto& x) { return std::get<std::string_view>(x); })
   //    | std::ranges::to<std::vector>();
   const std::vector required_items = [&]() {
      const auto required = std::get<json_array>(get_by_key(definition, "required"));
      std::vector<std::string_view> to_ret;
      for (const auto& member : required) {
         to_ret.push_back(std::get<std::string_view>(member));
      }
      return to_ret;
   }();
   const auto properties = std::get<json_map>(get_by_key(definition, "properties"));
   for (const auto& [prop_name, prop_desc] : properties) {
      members.push_back(parse_single_schema_type(prop_name, std::get<json_map>(prop_desc), required_items));
   }
   return members;
}

template<fixed_string StructName, fixed_string DefPrefix, fixed_string JsonSchema>
consteval void define_schema_types()
{
   // $defs should be optional, rather than a requirement
   constexpr auto longest_def_name = get_longest_def_name(JsonSchema.view());
   constexpr auto def_names = get_def_names<longest_def_name>(JsonSchema.view());
   // verify that all defs are objects (for now)
   const auto parsed_schema = std::get<json_map>(parse_json(JsonSchema.view()));
   const auto defs = std::get<json_map>(get_by_key(parsed_schema, "$defs"));
   for (const auto& [name, value] : defs) {
      const auto def_info = std::get<json_map>(value);
      const auto type = std::get<std::string_view>(get_by_key(def_info, "type"));
      assert(type == "object");
   }
   // Define each of the def types
   template for (constexpr auto name : def_names)
   {
      constexpr auto full_name = append_fixed_strings<DefPrefix, name>();
      constexpr auto to_define = std::meta::substitute(^^json_schema_types, {std::meta::reflect_constant(full_name)});
      const auto cur_def = std::get<json_map>(get_by_key(defs, name.view()));
      std::meta::define_aggregate(to_define, parse_schema_type(cur_def));
   }
   // // Then the main type
   // constexpr auto main_type = std::meta::substitute(^^json_schema_types, {std::meta::reflect_constant(StructName)});
   // std::meta::define_aggregate(main_type, parse_schema_type(parsed_schema));
}

consteval { define_schema_types<"fruits_and_veggies", "test_", basic_array_schema>(); }

constexpr auto x = json_schema_types<"test_veggie">{.veggieName = "hi", .veggieLike = false};

int main() {}

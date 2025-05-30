#include "json.hpp"

#include <experimental/meta>
#include <stdexcept>

constexpr char basic_nested_schema[]{R"(
{
   "$schema": "https://json-schema.org/draft/2020-12/schema",
   "type": "object",
   "properties": {
      "pain": {
         "type": "object",
         "properties": {
            "sadness": {
               "type": "number"
            }
         }
      }
   }
})"};

template<std::size_t N>
using cstr = khct::string<N>;

template<khct::string name>
struct json_schema_types;

constexpr auto type_mapping = std::to_array<std::pair<std::string_view, std::meta::info>>(
   {{"null", std::meta::underlying_entity_of(^^std::nullptr_t)},
    {"boolean", ^^bool},
    {"number", ^^double},
    {"integer", std::meta::underlying_entity_of(^^std::int64_t)},
    {"string", std::meta::underlying_entity_of(^^std::string)}});

struct obj_result {
   std::meta::info obj_struct;
};

template<cstr StructName, auto Def, bool Required>
consteval std::meta::info handle_object();

template<cstr StructName, auto Def, bool Required>
consteval std::meta::info handle_field();

template<cstr Structname, auto Def, bool Required>
consteval std::meta::info handle_array();

template<cstr StructName, auto Def, bool Required>
consteval std::meta::info handle_object()
{
   static_assert(Required);
   std::vector<std::meta::info> fields;
   template for (constexpr auto name_and_props : Def.template get_key<cstr{"properties"}>())
   {
      constexpr auto name = name_and_props.first;
      constexpr auto props = name_and_props.second;
      constexpr auto type = props.template get_key<cstr{"type"}>();
      if constexpr (type.view() == "object") {
         auto obj = handle_object<StructName + cstr{"::"} + name, props, true>();
         fields.push_back(std::meta::data_member_spec(obj, {.name = name, .no_unique_address = true}));
      }
      else if constexpr (type.view() == "array") {
         // TODO
      }
      else {
         fields.push_back(
            std::meta::data_member_spec(
               handle_field<StructName, props, true>(), {.name = name, .no_unique_address = true}));
      }
   }
   constexpr auto to_define = std::meta::substitute(^^json_schema_types, {std::meta::reflect_constant(StructName)});
   return std::meta::define_aggregate(to_define, fields);
}

template<cstr StructName, auto Def, bool Required>
consteval std::meta::info handle_field()
{
   static_assert(Required);
   constexpr auto type = Def.template get_key<cstr{"type"}>();
   constexpr auto iter = std::ranges::find(type_mapping, type, [](const auto& t) { return t.first; });
   static_assert(iter != type_mapping.end());
   return iter->second;
}

template<cstr Structname, auto Def, bool Required>
consteval std::meta::info handle_array()
{
   static_assert(Required);
}

template<cstr StructName, cstr DefPrefix, cstr JsonSchema>
consteval void define_schema_types()
{
   constexpr auto json = khct::parse_json<JsonSchema>();
   constexpr auto type = json.template get_key<cstr{"type"}>();
   if constexpr (type == "object") {
      handle_object<StructName, json, true>();
   }
   else if constexpr (type == "array") {
      // TODO
   }
   else {
      // TODO
   }
}

consteval { define_schema_types<"root", "test_", basic_nested_schema>(); }

using root = json_schema_types<"root">;

constexpr auto heck = root{.pain = {.sadness = 10.0}};

int main() {}

/*
Wow we're doing psuedo-code!!!

Basic type:
   - This will either resolve to an optional or a base type
   - Easy
   - Pass is_required from above; should be false by default
   - Creates a new data member but not a new data type

Handling a type of object:
   - This will always create a new type
   - Creates both a new data member and a new data type

Handling a type of array:
   - This also results in a new type being created?
   - At least a substitution into std::vector
   - Creates both a new data member and potentially a new data type?

Let's do some basic (but painful) sets:
schema:
   {
      "type": "object",
      "properties":
      {
         "pain":
         {
            "type": "object",
            "properties":
            {
               "sadness":
               {
                  "type": "number"
               }
            }
         }
      }
   }

value:
   {
      "pain": {
         "sadness": 20
      }
   }

so we can have a "test::base_obj::pain" struct
and a "test::base_obj" struct?

that look like
test::base_obj::pain { std::optional<double> sadness; };
test::base_obj { std::optional<test::base_obj::pain>; };

process:
   - Register the required fields {}
   - look at the properties
      - for each property
         -

*/

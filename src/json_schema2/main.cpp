#include "json.hpp"

#include <experimental/meta>
#include <stdexcept>
#include <unordered_map>

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
         },
         "required": ["sadness"],
         "additionalProperties": false
      }
   },
   "required": ["pain"],
   "additionalProperties": false
})"};

constexpr char basic_array_schema[](R"(
{
   "$schema": "https://json-schema.org/draft/2020-12/schema",
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
               "type": "string"
            },
            "veggieLike": {
               "type": "boolean"
            }
         }
      }
   }
})");

template<std::size_t N>
using strc = khct::string<N>;

struct additional_value
   : std::variant<
        std::int64_t,
        std::string,
        bool,
        std::nullptr_t,
        double,
        std::unordered_map<std::string, additional_value>,
        std::vector<additional_value>> {
private:
   using base = std::variant<
      std::int64_t,
      std::string,
      bool,
      std::nullptr_t,
      double,
      std::unordered_map<std::string, additional_value>,
      std::vector<additional_value>>;

public:
   using base::base;
};

using additional_properties = std::unordered_map<std::string, additional_value>;

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

template<strc StructName, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_object();

template<strc StructName, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_field();

template<strc Structname, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_array();

template<strc StructName, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_object()
{
   static_assert(Required);
   std::vector<std::meta::info> fields;
   template for (constexpr auto name_and_props : Def.template get_key<strc{"properties"}>())
   {
      constexpr auto name = name_and_props.first;
      static_assert(
         name.view() != "additional_properties", "additional_properties field name is reserved for library use");
      constexpr auto props = name_and_props.second;
      constexpr auto type = props.template get_key<strc{"type"}>();
      if constexpr (type.view() == "object") {
         const auto obj = handle_object<StructName + strc{"::"} + name, DefPrefix, props, true>();
         fields.push_back(std::meta::data_member_spec(obj, {.name = name, .no_unique_address = true}));
      }
      else if constexpr (type.view() == "array") {
         fields.push_back(
            std::meta::data_member_spec(
               handle_array<StructName, DefPrefix, props, true>(), {.name = name, .no_unique_address = true}));
      }
      else {
         fields.push_back(
            std::meta::data_member_spec(
               handle_field<StructName, DefPrefix, props, true>(), {.name = name, .no_unique_address = true}));
      }
   }
   constexpr auto add_prop = Def.template get_key<strc{"additionalProperties"}>();
   if constexpr (add_prop == khct::nil || add_prop == khct::true_) {
      fields.push_back(std::meta::data_member_spec(^^additional_properties, {.name = "additional_properties"}));
   }
   constexpr auto to_define = std::meta::substitute(^^json_schema_types, {std::meta::reflect_constant(StructName)});
   return std::meta::define_aggregate(to_define, fields);
}

template<strc StructName, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_field()
{
   constexpr auto type = Def.template get_key<strc{"type"}>();
   constexpr auto iter = std::ranges::find(type_mapping, type, [](const auto& t) { return t.first; });
   static_assert(iter != type_mapping.end());
   if constexpr (Required) {
      return iter->second;
   }
   else {
      return std::meta::substitute(^^std::optional, {iter->second});
   }
}

template<strc StructName, strc DefPrefix, auto Def, bool Required>
consteval std::meta::info handle_array()
{
   static_assert(Required);
   constexpr auto items = Def.template get_key<strc{"items"}>();
   // first try for a simple type
   constexpr auto type = items.template get_key<strc{"type"}>();
   if constexpr (type != khct::nil) {
      const auto to_add = handle_field<StructName, DefPrefix, items, true>();
      const auto as_vec = std::meta::substitute(^^std::vector, {to_add});
      return as_vec;
   }
   else {
      // Otherwise this is a def_type
      static constexpr auto ref = items.template get_key<strc{"$ref"}>();
      constexpr std::string_view def_start = "#/$defs/";
      static_assert(ref.view().starts_with(def_start));
      constexpr auto name = ref.template splice<def_start.size(), ref.size()>();
      const auto value_type = ^^json_schema_types<DefPrefix + name>;
      return std::meta::substitute(^^std::vector, {value_type});
   }
}

template<strc StructName, strc DefPrefix, strc JsonSchema>
consteval void define_schema_types()
{
   constexpr auto json = khct::parse_json<JsonSchema>();
   constexpr auto defs = json.template get_key<strc{"$defs"}>();
   // Define any defs first
   if constexpr (defs != khct::nil) {
      template for (constexpr auto name_and_info : defs)
      {
         constexpr auto name = name_and_info.first;
         constexpr auto info = name_and_info.second;
         constexpr auto type = info.template get_key<strc{"type"}>();
         static_assert(type == "object", "Non-object defs not yet supported");
         if constexpr (type == "object") {
            handle_object<DefPrefix + name, DefPrefix, info, true>();
         }
         else if constexpr (type == "array") {
            handle_array<DefPrefix + name, DefPrefix, info, true>();
         }
         else {
            handle_value<DefPrefix + name, DefPrefix, info, true>();
         }
      }
   }
   // Then define the main thing
   constexpr auto type = json.template get_key<strc{"type"}>();
   if constexpr (type == "object") {
      handle_object<StructName, DefPrefix, json, true>();
   }
   else if constexpr (type == "array") {
      // TODO
      static_assert(false, "not supported yet");
   }
   else {
      // TODO
      static_assert(false, "not supported yet");
   }
}

consteval
{
   define_schema_types<"root", "", basic_nested_schema>();
   define_schema_types<"veggies_and_fruits", "test_", basic_array_schema>();
}

using root = json_schema_types<"root">;
using veggies_and_fruits = json_schema_types<"veggies_and_fruits">;

constexpr auto heck = root{.pain = {.sadness = 10.0}};

const auto heck2 = veggies_and_fruits{
   .fruits = {"apple", "orange", "test"},
   .vegetables = {{
      .veggieName = "banana",
      .veggieLike = true,
      .additional_properties = {{"extra", "prop"}}
   }
   }};

// ----------------------------------
// - Testing section
// ----------------------------------

// template <typename T, T... Vs>
// constexpr T fixed_str[sizeof...(Vs) + 1]{Vs..., '\0'};

// consteval auto reflect_constant_string(std::string_view v) -> std::meta::info {
//    auto args = std::vector<std::meta::info>{^^char};
//    for (auto&& elem : v) {
//       args.push_back(std::meta::reflect_constant(elem));
//    }
//    return std::meta::substitute(^^fixed_str, args);
// }

// template<khct::string name>
// struct test_types;

// consteval void tester(std::string_view v)
// {
//    const auto const_str = reflect_constant_string(v);
//    const auto const_type = std::meta::substitute(^^test_types, {const_str});
//    // const auto type = std::meta::substitute(^^test_types, {});
//    // std::meta::define_aggregate(type, {^^int, {.name = v}});
// }

// consteval
// {
//    tester("hi");
// }

int main() {}

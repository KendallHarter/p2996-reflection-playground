#include "common.hpp"
#include "json_parse.hpp"

#include <array>
#include <cassert>
#include <string_view>
#include <unordered_map>

constexpr auto type_mapping = std::to_array<std::pair<std::string_view, std::meta::info>>(
   {{"null", ^^tdef<std::nullptr_t>::type},
    {"boolean", ^^bool},
    {"number", ^^double},
    {"integer", ^^tdef<std::int64_t>::type},
    {"string", ^^tdef<std::string>::type}});

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

consteval std::meta::info handle_object(std::meta::info defs_struct, std::string_view struct_name, json_map def);
consteval std::meta::info handle_field(std::meta::info defs_struct, std::string_view struct_name, json_map def);
consteval std::meta::info handle_array(std::meta::info defs_struct, std::string_view struct_name, json_map def);

consteval std::meta::info handle_object(std::meta::info defs_struct, std::string_view struct_name, json_map def)
{
   std::vector<std::meta::info> fields;
   const auto required_fields = [&]() -> std::vector<std::string> {
      if (const auto req_ptr = get_by_key_opt(def, "required")) {
         return std::get<json_array>(*req_ptr)
              | std::views::transform([](const auto& s) { return std::string(std::get<std::string_view>(s)); })
              | std::ranges::to<std::vector>();
      }
      return {};
   }();
   for (const auto& [name, props_raw] : std::get<json_map>(get_by_key(def, "properties"))) {
      const auto& props = std::get<json_map>(props_raw);
      const auto type = std::get<std::string_view>(get_by_key(props, "type"));
      // This wasn't compiling when within the lambda...?
      const auto scoped_name = struct_name + std::string("::") + name;
      const std::meta::info type_info = [&]() {
         if (type == "object") {
            return handle_object(defs_struct, scoped_name, props);
         }
         else if (type == "array") {
            return handle_array(defs_struct, struct_name, props);
         }
         else {
            return handle_field(defs_struct, struct_name, props);
         }
      }();
      if (std::ranges::find(required_fields, name) == required_fields.end()) {
         const auto opt_field = std::meta::substitute(^^std::optional, {type_info});
         fields.push_back(std::meta::data_member_spec(opt_field, {.name = name, .no_unique_address = true}));
      }
      else {
         fields.push_back(std::meta::data_member_spec(type_info, {.name = name, .no_unique_address = true}));
      }
   }
   const auto add_prop = get_by_key_opt(def, "additionalProperties");
   if (!add_prop || std::get<bool>(*add_prop)) {
      fields.push_back(std::meta::data_member_spec(^^additional_properties, {.name = "additional_properties"}));
   }
   const auto static_name = reflect_constant_string(struct_name);
   const auto to_define = std::meta::substitute(defs_struct, {static_name});
   return std::meta::define_aggregate(to_define, fields);
}

consteval std::meta::info handle_field(std::meta::info defs_struct, std::string_view struct_name, json_map def)
{
   const auto type = std::get<std::string_view>(get_by_key(def, "type"));
   const auto iter = std::ranges::find(type_mapping, type, [](const auto& t) { return t.first; });
   assert(iter != type_mapping.end());
   return iter->second;
}

consteval std::meta::info handle_array(std::meta::info defs_struct, std::string_view struct_name, json_map def)
{
   const auto items = std::get<json_map>(get_by_key(def, "items"));
   // See if it's just a simple type first
   if (const auto type = get_by_key_opt(items, "type")) {
      const auto to_add = handle_field(defs_struct, struct_name, items);
      const auto as_vec = std::meta::substitute(^^std::vector, {to_add});
      return as_vec;
   }
   else {
      // Should be a def type
      const auto ref = std::get<std::string_view>(get_by_key(items, "$ref"));
      constexpr std::string_view def_start = "#/$defs/";
      assert(ref.starts_with(def_start));
      const auto name = ref.substr(def_start.size());
      const auto value_type = std::meta::substitute(defs_struct, {reflect_constant_string(name)});
      return std::meta::substitute(^^std::vector, {value_type});
   }
}

consteval void
   define_schema_types(std::meta::info defs_struct, std::string_view struct_name, std::string_view json_schema)
{
   const auto json = std::get<json_map>(parse_json(json_schema));
   if (const auto defs_raw = get_by_key_opt(json, "$defs")) {
      const auto& defs = std::get<json_map>(*defs_raw);
      for (const auto& [name, info_raw] : defs) {
         const auto& info = std::get<json_map>(info_raw);
         handle_object(defs_struct, name, info);
      }
   }
   const auto type = std::get<std::string_view>(get_by_key(json, "type"));
   handle_object(defs_struct, struct_name, json);
}

template<fixed_string>
struct my_defs;

template<fixed_string>
struct v_and_f_structs;

consteval
{
   define_schema_types(^^my_defs, "root", basic_nested_schema);
   define_schema_types(^^v_and_f_structs, "veggies_and_fruits", basic_array_schema);
}

using root = my_defs<"root">;
using veggies_and_fruits = v_and_f_structs<"veggies_and_fruits">;
using veggie = v_and_f_structs<"veggie">;

constexpr auto heck = root{.pain = {.sadness = 1.0}};

const auto heck2 = veggies_and_fruits{
   .fruits = std::vector<std::string>{"apple", "orange", "test"},
   .vegetables
   = std::vector<veggie>{{.veggieName = "banana", .veggieLike = true, .additional_properties = {{"extra", "prop"}}}}};

int main() {}

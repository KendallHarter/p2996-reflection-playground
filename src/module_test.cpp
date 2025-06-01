module;

#include <array>
#include <cassert>
#include <experimental/meta>
#include <ranges>
#include <string>

export module module_test;

export consteval std::string_view get_fully_qualified_type(std::meta::info info);

consteval std::string get_fully_qualified_name_string(std::meta::info info)
{
   const std::string base_name = [&]() -> std::string {
      if (!std::meta::has_identifier(info)) {
         if (std::meta::is_namespace(info)) {
            return "<anonymous namespace>";
         }
         else if (std::meta::is_aggregate_type(info)) {
            return "<anonymous>";
         }
      }
      const std::string_view name_view = std::meta::identifier_of(info);
      std::string name(name_view.data(), name_view.data() + name_view.size());
      return name;
   }();
   const auto parent = std::meta::parent_of(info);
   if (parent != ^^::) {
      return get_fully_qualified_name_string(parent) + "::" + base_name;
   }
   else {
      // name = "::" + name;
      return base_name;
   }
}

export consteval std::string_view get_fully_qualified_name(std::meta::info info)
{
   assert(std::meta::has_identifier(info));
   return std::define_static_string(get_fully_qualified_name_string(info));
}

constexpr auto basic_type_mapping = std::to_array<std::pair<std::meta::info, std::string_view>>(
   {{^^signed char, "signed char"},
    {^^char, "char"},
    {^^unsigned char, "unsigned char"},
    {^^char8_t, "char8_t"},
    {^^wchar_t, "wchar_t"},
    {^^short, "short"},
    {^^unsigned short, "unsigned short"},
    {^^int, "int"},
    {^^unsigned int, "unsigned int"},
    {^^long, "long"},
    {^^unsigned long, "unsigned long"},
    {^^long long, "long long"},
    {^^unsigned long long, "unsigned long long"},
    {^^float, "float"},
    {^^double, "double"},
    {^^long double, "long double"},
    {^^void, "void"}});

consteval std::string get_fully_qualified_type_str(std::meta::info info)
{
   if (std::meta::is_type_alias(info)) {
      return get_fully_qualified_type_str(std::meta::underlying_entity_of(info));
   }
   std::string build_up = "";
   if (std::meta::is_volatile_type(info)) {
      build_up += "volatile";
   }
   if (std::meta::is_const_type(info)) {
      if (!build_up.empty()) {
         build_up += " ";
      }
      build_up += "const";
   }
   const auto no_cv_type = std::meta::remove_cv(info);
   const auto iter = std::ranges::find(basic_type_mapping, no_cv_type, [](auto p) { return p.first; });
   if (iter == basic_type_mapping.end()) {
      if (info == std::meta::underlying_entity_of(^^std::meta::info)) {
         if (!build_up.empty()) {
            build_up = " " + build_up;
         }
         return "std::meta::info" + build_up;
      }
      if (info == std::meta::underlying_entity_of(^^std::nullptr_t)) {
         if (!build_up.empty()) {
            build_up = " " + build_up;
         }
         return "std::nullptr_t" + build_up;
      }
      if (std::meta::is_lvalue_reference_type(info)) {
         if (!build_up.empty()) {
            build_up = " " + build_up;
         }
         return get_fully_qualified_type_str(std::meta::remove_reference(info)) + "&" + build_up;
      }
      if (std::meta::is_pointer_type(info)) {
         if (!build_up.empty()) {
            build_up = " " + build_up;
         }
         return get_fully_qualified_type_str(std::meta::remove_pointer(info)) + "*" + build_up;
      }
      if (std::meta::is_class_type(info)) {
         if (!build_up.empty()) {
            build_up += " ";
         }
         if (std::meta::has_template_arguments(info)) {
            build_up = build_up + get_fully_qualified_name_string(std::meta::template_of(info)) + "<";
            bool first_temp = true;
            for (const auto& t_info : std::meta::template_arguments_of(info)) {
               if (!first_temp) {
                  build_up += ", ";
               }
               first_temp = false;
               if (std::meta::is_type(t_info)) {
                  build_up += get_fully_qualified_type_str(t_info);
               }
               else {
                  build_up += get_fully_qualified_name_string(t_info);
               }
            }
            return build_up + ">";
         }
         else {
            return build_up + get_fully_qualified_name_string(info);
         }
      }
      if (std::meta::is_function_type(info)) {
         throw "functions are too hard to do";
      }
      if (std::meta::is_array_type(info)) {
         throw "arrays are too hard to do";
      }
   }
   else {
      if (!build_up.empty()) {
         build_up += " ";
      }
      return build_up + std::string(iter->second.data(), iter->second.data() + iter->second.size());
   }
   throw "unsupported type";
}

export consteval std::string_view get_fully_qualified_type(std::meta::info info)
{
   assert(std::meta::is_type(info) || std::meta::is_type_alias(info));
   return std::define_static_string(get_fully_qualified_type_str(info));
}

#include "common.hpp"

#include <cassert>
#include <cctype>
#include <experimental/meta>
#include <iostream>
#include <print>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

template<typename SetIn, typename T>
consteval bool can_set_with_type()
{
   static constexpr auto members
      = define_static_array(std::meta::nonstatic_data_members_of(^^SetIn, std::meta::access_context::unchecked()));
   template for (constexpr auto mem : members)
   {
      if (std::meta::is_assignable_type(std::meta::add_lvalue_reference(std::meta::type_of(mem)), ^^T)) {
         return true;
      }
   }
   return false;
}

template<typename SetIn, typename T>
constexpr bool set_by_name(SetIn& set_in, std::string_view name, T&& set_value) noexcept
{
   static_assert(can_set_with_type<SetIn, T>(), "No members can be assigned to T's type.");
   static constexpr auto members
      = define_static_array(std::meta::nonstatic_data_members_of(^^SetIn, std::meta::access_context::unchecked()));
   template for (constexpr auto mem : members)
   {
      if (std::meta::identifier_of(mem) == name) {
         if constexpr (std::meta::is_assignable_type(std::meta::add_lvalue_reference(std::meta::type_of(mem)), ^^T)) {
            set_in.[:mem:] = std::forward<T>(set_value);
            return true;
         }
      }
   }
   return false;
}

template<typename T>
consteval std::meta::info get_variant_of_unique_types() noexcept
{
   const auto members
      = std::meta::nonstatic_data_members_of(std::meta::remove_reference(^^T), std::meta::access_context::unchecked());
   std::vector<std::meta::info> unique_members;
   for (const auto& mem : members) {
      const auto type = std::meta::type_of(mem);
      if (std::ranges::find(unique_members, type) == unique_members.end()) {
         unique_members.push_back(type);
      }
   }
   return std::meta::substitute(^^std::variant, unique_members);
}

template<typename T>
struct to_ptr_variant_impl;

template<typename... T>
struct to_ptr_variant_impl<std::variant<T...>> {
   using type = std::variant<std::add_pointer_t<T>...>;
};

template<typename T>
using to_ptr_variant = to_ptr_variant_impl<T>::type;

template<typename T>
struct to_const_ptr_variant_impl;

template<typename... T>
struct to_const_ptr_variant_impl<std::variant<T...>> {
   using type = std::variant<std::add_pointer_t<std::add_const_t<T>>...>;
};

template<typename T>
using to_const_ptr_variant = to_const_ptr_variant_impl<T>::type;

template<typename SetIn, typename Var>
constexpr bool assign_variant_by_name_impl(SetIn& set_in, std::string_view name, Var&& var) noexcept
{
   return std::visit(
      [&]<typename T>(T&& value) -> bool { return set_by_name(set_in, name, std::forward<T>(value)); },
      std::forward<Var>(var));
}

template<typename T>
constexpr bool
   assign_variant_by_name(T& set_in, std::string_view name, const[:get_variant_of_unique_types<T>():] & var) noexcept
{
   return assign_variant_by_name_impl(set_in, name, var);
}

template<typename T>
constexpr bool
   assign_variant_by_name(T& set_in, std::string_view name, [:get_variant_of_unique_types<T>():] && var) noexcept
{
   return assign_variant_by_name_impl(set_in, name, std::move(var));
}

template<typename T>
constexpr auto get_by_name(T& get_from, std::string_view name) noexcept
   // clang-format off
   -> std::optional<to_ptr_variant<[:get_variant_of_unique_types<T>():]>> {
      // clang-format on
      static constexpr auto nsdm
         = define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
      template for (constexpr auto mem : nsdm)
      {
         if (std::meta::identifier_of(mem) == name) {
            return &get_from.[:mem:];
         }
      }
      return std::nullopt;
   }

// Is there a way to not just replicate this?
template<typename T>
constexpr auto get_by_name(const T& get_from, std::string_view name) noexcept
   // clang-format off
   -> std::optional<to_const_ptr_variant<[:get_variant_of_unique_types<T>():]>> {
      // clang-format on
      using ret_type = std::optional<to_const_ptr_variant<[:get_variant_of_unique_types<T>():]>>;
      static constexpr auto nsdm
         = define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
      template for (constexpr auto mem : nsdm)
      {
         if (std::meta::identifier_of(mem) == name) {
            return &get_from.[:mem:];
         }
      }
      return std::nullopt;
   }

std::vector<std::string_view> split_by_whitespace(std::string_view v)
{
   std::vector<std::string_view> to_ret;
   auto previous = std::ranges::find_if_not(v, [](int c) { return std::isspace(c); });
   while (previous != v.end()) {
      const auto start = std::ranges::find_if_not(previous, v.end(), [](int c) { return std::isspace(c); });
      if (start == v.end()) {
         break;
      }
      const auto end = std::ranges::find_if(start + 1, v.end(), [](int c) { return std::isspace(c); });
      to_ret.push_back({start, end});
      previous = end;
   }
   return to_ret;
}

template<typename T>
void print_named_value(std::string_view name, const T& value, std::string_view equals = "==") noexcept
{
   if (!name.empty()) {
      std::print("{}: {} ", name, std::meta::display_string_of(^^T));
   }

   if constexpr (std::same_as<const char*, T>) {
      if (value) {
         std::print("{} {:?}", equals, value);
      }
      else {
         std::print("{} nullptr", equals);
      }
   }
   else if constexpr (std::is_pointer_v<T>) {
      if (value) {
         std::print("{} 0x{:X}", equals, reinterpret_cast<std::uintptr_t>(value));
         print_named_value("", *value, " ->");
      }
      else {
         std::print("{} nullptr", equals);
      }
   }
   else if constexpr (std::same_as<std::string, T>) {
      std::print("{} {:?}", equals, value);
   }
   else if constexpr (std::is_default_constructible_v<std::formatter<T>>) {
      std::print("{} {}", equals, value);
   }
   else if constexpr (requires(std::ostream& out) { out << value; }) {
      std::stringstream sstr;
      sstr << value;
      std::print("{} {}", equals, sstr.str());
   }
   else {
      std::print("{} <non printable>", equals);
   }
}

template<typename T>
   requires std::is_standard_layout_v<T>
consteval std::meta::info make_min_size() noexcept
{
   struct min_size;

   const auto mems = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked());
   for (const auto& mem : mems) {
      assert(!std::meta::is_bit_field(mem) && "Bitfield members are not supported");
   }

   std::vector<std::pair<std::meta::info, std::size_t>> members_and_align;
   std::ranges::transform(mems, std::back_inserter(members_and_align), [](const std::meta::info i) {
      return std::make_pair(i, std::meta::alignment_of(i));
   });

   std::ranges::sort(members_and_align, {}, [](const auto& p) { return p.second; });

   std::vector<std::meta::info> opts;
   std::ranges::transform(members_and_align, std::back_inserter(opts), [](const auto& info) {
      const auto& [i, dummy] = info;
      return std::meta::data_member_spec(
         std::meta::type_of(i),
         {.name = std::meta::identifier_of(i),
          .alignment = std::meta::alignment_of(i),
          .width = std::nullopt,
          .no_unique_address = true});
   });

   return std::meta::define_aggregate(^^min_size, opts);
}

template<typename T>
   requires std::is_standard_layout_v<T>
using min_sized = [:make_min_size<T>():];

struct values {
   int x;
   int y;
   std::string z;
   char c;
   const char* walrus;
   const char* walrus2 = "walrus";
   int* pointer = &x;
   int** pointer2 = &pointer;
};

constexpr std::size_t constexpr_strlen(const char* c) noexcept
{
   if consteval {
      std::size_t size = 0;
      while (c[size] != '\0') {
         size += 1;
      }
      return size;
   }
   else {
      return std::strlen(c);
   }
}

int main()
{
   using command_ptr = void (*)(values&, const std::vector<std::string_view>&);
   static constexpr auto valid_commands = std::to_array<std::pair<const char*, command_ptr>>(
      {{"set",
        [](values& vals, const std::vector<std::string_view>& args) {
           if (args.size() < 2) {
              std::println("Error: Missing field name");
              return;
           }
           if (args.size() < 3) {
              std::println("Error: Missing value in set command");
              return;
           }
           const auto field_name = args[1];
           const auto loc_opt = get_by_name(vals, field_name);
           if (!loc_opt) {
              std::println("Error: No field named {}", field_name);
              return;
           }
           const auto value_to_set = args[2];
           std::visit(
              [&](const auto& value) {
                 if constexpr (requires { std::declval<std::stringstream>() >> *value; }) {
                    std::stringstream sstr{value_to_set};
                    auto old_value = std::move(*value);
                    sstr >> *value;
                    if (!sstr) {
                       std::println("Error: Could not parse value");
                       *value = std::move(old_value);
                    }
                 }
                 else {
                    std::println("Error: Field cannot be set");
                 }
              },
              *loc_opt);
        }},
       {"view",
        [](values& vals, const std::vector<std::string_view>& args) {
           if (args.size() < 2) {
              std::println("Error: Missing field name");
              return;
           }
           const auto field_name = args[1];
           const auto loc_opt = get_by_name(vals, field_name);
           if (!loc_opt) {
              std::println("Error: No field named {}", field_name);
              return;
           }
           std::visit([&](const auto& value) { print_named_value(field_name, *value); }, *loc_opt);
           std::println("");
        }},
       {"view_all", [](values& vals, const std::vector<std::string_view>& args) {
           static constexpr auto nsdm = define_static_array(
              std::meta::nonstatic_data_members_of(^^values, std::meta::access_context::unchecked()));
           template for (constexpr auto mem : nsdm)
           {
              std::print("   ");
              print_named_value(std::meta::identifier_of(mem), vals.[:mem:]);
              std::println("");
           };
        }}});
   static constexpr auto total_len
      = valid_commands.size()
      + std::ranges::fold_left(
           std::ranges::views::transform(valid_commands | std::ranges::views::elements<0>, constexpr_strlen),
           0,
           std::plus<>{});
   static constexpr std::array<char, total_len> arguments_string = []() {
      std::array<char, total_len> to_ret;
      auto copy_loc = to_ret.begin();
      for (const auto& [cmd, func] : valid_commands) {
         copy_loc = std::ranges::copy(cmd, cmd + constexpr_strlen(cmd), copy_loc).out;
         *copy_loc = '|';
         ++copy_loc;
      }
      to_ret.back() = '\0';
      return to_ret;
   }();
   static_assert(arguments_string.size() - 1 == constexpr_strlen(arguments_string.data()));

   values vals{};
   std::string line;
   while (true) {
      std::print("> ");
      if (!std::getline(std::cin, line)) {
         break;
      }
      const auto split = split_by_whitespace(line);
      if (split.size() == 0) {
         std::println("Usage: {} [field_name] [value]", arguments_string.data());
         continue;
      }
      const auto command = split[0];
      const auto command_loc = std::ranges::find(valid_commands, command, [](const auto& v) { return v.first; });
      if (command_loc == valid_commands.end()) {
         std::println("Error: Invalid command");
      }
      else {
         command_loc->second(vals, split);
      }
   }
   std::println("");
}

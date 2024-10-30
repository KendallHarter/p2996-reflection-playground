#include "common.hpp"

#include <concepts>
#include <experimental/meta>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

// template<typename T>
// struct command_ptr_impl {
//    using type = void (*)(T&, const std::vector<std::string_view>&);
// };

// template<>
// struct command_ptr_impl<void> {
//    using type = void (*)(const std::vector<std::string_view>&);
// };

// template<typename T>
// using command_ptr = command_ptr_impl<T>::type;

// template<typename T, command_ptr<T> P>
// struct command_base {
//    inline static constexpr auto func = P;
// };

template<std::meta::info Info>
constexpr auto get_type_alias_names() noexcept
{
   using func_ptr = void (*)(const std::vector<std::string_view>&);
   std::vector<std::pair<std::string_view, func_ptr>> to_ret;
   static constexpr auto func = []<std::meta::info Mem>(const std::vector<std::string_view>& args) static {
      static constexpr auto func_type = ^^[:Mem:] ::func;
      static constexpr auto func_value = std::meta::value_of(func_type);
      static constexpr auto func_refl = ^^decltype([:func_value:])::operator();
      static constexpr auto num_params = std::meta::parameters_of(func_refl).size();
      if (num_params != args.size() - 1) {
         std::println(
            "Error: Got {} parameter{}, expected {}.", args.size() - 1, (args.size() - 1) == 1 ? "" : "s", num_params);
         return;
      }
      std::array<std::string_view, num_params> to_call_with;
      std::ranges::copy(args.begin() + 1, args.end(), to_call_with.begin());
      std::apply([:Mem:] ::func, to_call_with);
   };
   [:expand(std::meta::members_of(Info) | std::views::filter(std::meta::is_type_alias)):]
      >> [&]<auto Mem> { to_ret.emplace_back(std::meta::identifier_of(Mem), &func.template operator()<Mem>); };
   return to_ret;
}

constexpr std::vector<std::string_view> split_by_whitespace(std::string_view v)
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

template<typename T = void>
struct commands {
   // template<command_ptr<T> P>
   // using command = command_base<T, P>;

   // template<typename Self>
   // void execute(this const Self& self, T& values, std::string_view line) noexcept
   // {
   //    ;
   // }
};

template<typename... U>
   requires(std::same_as<U, std::string_view> && ...)
using ptr_fun = void(*)(const U&...);

template<auto P>
struct command_ptr {
   inline static constexpr auto func = P;
};

template<>
struct commands<void> {

   template<auto P>
   using command = command_ptr<P>;

   template<typename Self>
   void execute(this const Self& self, std::string_view line) noexcept
   {
      const auto args = split_by_whitespace(line);
      const auto command_names = get_type_alias_names<^^Self>();
      if (args.size() == 0) {
         std::println("Error: No command given.");
         return;
      }
      const auto loc = std::ranges::find(command_names, args[0], [](const auto& p) { return p.first; });
      if (loc == command_names.end()) {
         if (args[0] == "help") {
            std::println("   Commands and arguments:");
            [:expand(std::meta::members_of(^^Self) | std::views::filter(std::meta::is_type_alias)):]
               >> [&]<auto Alias> {
                    static constexpr auto func_type = ^^[:Alias:] ::func;
                    static constexpr auto func_value = std::meta::value_of(func_type);
                    static constexpr auto func_refl = ^^decltype([:func_value:])::operator();
                    std::print("      {}", std::meta::identifier_of(Alias));
                    [:expand(std::meta::parameters_of(func_refl)):]
                       >> [&]<auto Param> { std::print(" {}", std::meta::identifier_of(Param)); };
                    std::print("\n");
                 };
         }
         else {
            std::println("Error: No command {}.", args[0]);
         }
      }
      else {
         loc->second(args);
      }
   }
};

struct test_commands : commands<> {
   using goodbye = command<[]() { std::println("goodbye!"); }>;

   using greet = command<[](std::string_view name) { std::println("Hello {}.", name); }>;
};

int main()
{
   std::string line;
   while (true) {
      std::print("> ");
      if (!std::getline(std::cin, line)) {
         break;
      }
      test_commands{}.execute(line);
   }
   std::print("\n");
}

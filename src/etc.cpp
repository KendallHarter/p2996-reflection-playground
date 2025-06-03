#include "common.hpp"

#include <experimental/meta>
#include <print>
#include <ranges>
#include <string>
#include <tuple>

template<typename T>
struct debug_print_struct {
   const T& value;
};

struct inner_hoot {
   int y = 10;
};

struct hoot {
   int x;
   inner_hoot hoot2;
   std::tuple<int, int, int> wow{1, 2, 3};
   std::vector<int> doot{1, 2, 3, 4, 5};
};

template<typename T>
constexpr auto as_debug(const T& val) noexcept -> debug_print_struct<T>
{
   return {val};
}

template<typename T>
concept is_debug_print_struct = requires(T val) { []<typename T2>(debug_print_struct<T2>*) {}(std::addressof(val)); };

template<typename T, typename CharT>
   requires(!is_debug_print_struct<T> && !std::formattable<T, CharT>)
struct std::formatter<debug_print_struct<T>, CharT> {
   template<typename ParseContext>
   static constexpr auto parse(ParseContext& ctx) noexcept -> ParseContext::iterator
   {
      return ctx.begin();
   }

   template<typename FmtContext>
   static auto format(const debug_print_struct<T>& value, FmtContext& ctx) -> FmtContext::iterator
   {
      auto out = [&]() {
         if constexpr (std::meta::has_identifier(^^T)) {
            return std::format_to(ctx.out(), "{}{{", std::meta::identifier_of(^^T));
         }
         else {
            return std::format_to(ctx.out(), "<unnamed>{{");
         }
      }();
      auto output_delim = [&, first = true] mutable {
         if (!first) {
            *out = ',';
            ++out;
            *out = ' ';
            ++out;
         }
         first = false;
      };

      static constexpr auto bases
         = ::define_static_array(std::meta::bases_of(^^T, std::meta::access_context::current()));
      template for (constexpr auto base : bases)
      {
         output_delim();
         // clang-format off
         out = std::format_to(out, "{}", as_debug(static_cast<const [:std::meta::type_of(base):]&>(value.value)));
         // clang-format on
      }

      static constexpr auto mems
         = ::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
      template for (constexpr auto mem : mems)
      {
         output_delim();
         std::format_to(ctx.out(), ".{} = ", std::meta::identifier_of(mem));
         if constexpr (std::meta::is_class_type(std::meta::type_of(mem))) {
            out = std::format_to(ctx.out(), "{}", as_debug(value.value.[:mem:]));
         }
         else {
            std::formatter<[:std::meta::remove_cvref(std::meta::type_of(mem)):]> member_formatter;
            if constexpr (requires() { member_formatter.set_debug_format(); }) {
               member_formatter.set_debug_format();
            }
            ctx.advance_to(out);
            out = member_formatter.format(value.value.[:mem:], ctx);
         }
      }
      *out = '}';
      ++out;
      return out;
   }
};

template<typename T, typename CharT>
   requires(!is_debug_print_struct<T> && std::formattable<T, CharT>)
struct std::formatter<debug_print_struct<T>, CharT> : private std::formatter<T> {
   template<typename ParseContext>
   constexpr auto parse(ParseContext& ctx) noexcept -> ParseContext::iterator
   {
      return static_cast<std::formatter<T>&>(*this).parse(ctx);
   }

   template<typename FmtContext>
   auto format(const debug_print_struct<T>& value, FmtContext& ctx) const -> FmtContext::iterator
   {
      return static_cast<const std::formatter<T>&>(*this).format(value.value, ctx);
   }
};

int main()
{
   const auto test = hoot{.x = 20};
   std::print("{}\n", as_debug(hoot{.x = 20}));
   std::print("{}\n", as_debug(std::tuple{1, 2, 3, 4, 5}));
}

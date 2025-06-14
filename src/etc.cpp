#include "common.hpp"

#include <cassert>
#include <experimental/meta>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>

template<fixed_string Name, typename T>
struct named_pair_struct {
   inline static constexpr auto name = Name;
   using type = T;
};

template<fixed_string Name, typename T>
inline constexpr auto named_pair = named_pair_struct<Name, T>{};

// The requires expression is a limitation of the compiler - in C++26 trivial unions are a thing
template<named_pair_struct... Members>
   requires(std::is_trivially_destructible_v<typename decltype(Members)::type> && ...)
struct named_variant {
private:
   union storage_union;
   consteval
   {
      std::meta::define_aggregate(
         ^^storage_union, {std::meta::data_member_spec(^^typename decltype(Members)::type)...});
   }

   int index_;
   storage_union storage_;

   static consteval auto get_nth_member(std::size_t i) -> std::meta::info
   {
      const auto ctx = std::meta::access_context::current();
      return std::meta::nonstatic_data_members_of(^^storage_union, ctx)[i];
   }

   // clang-format off
   template<std::size_t I, typename T>
   constexpr named_variant(std::integral_constant<std::size_t, I>, T&& value)
      requires std::convertible_to<decltype(value), typename [:std::meta::type_of(get_nth_member(I)):]>
   {
      storage_.[:get_nth_member(I):] = std::forward<T>(value);
      index_ = I;
   }

   constexpr void delete_cur_index()
   {
      template for (constexpr auto i : ::define_static_array(std::views::iota(0zu, sizeof...(Members))))
      {
         if (i == index_) {
            std::destroy_at(&storage_.[:get_nth_member(i):]);
            return;
         }
      }
   }

   template<fixed_string Name>
   static consteval auto get_index_by_name() -> std::size_t
   {
      template for (constexpr auto i : ::define_static_array(std::views::iota(0zu, sizeof...(Members)))) {
         if (Name == Members...[i].name) {
            return static_cast<std::size_t>(i);
         }
      }
      throw std::runtime_error{"Name not found"};
   }

public:
   template<fixed_string Name, typename T>
   constexpr static auto create(T&& val) -> named_variant
   {
      constexpr auto index = get_index_by_name<Name>();
      return {std::integral_constant<std::size_t, index>{}, std::forward<T>(val)};
   }

   constexpr ~named_variant()
      requires (!(std::is_trivially_destructible_v<typename decltype(Members)::type> && ...))
   {
      delete_cur_index();
   }

   constexpr ~named_variant()
      requires (std::is_trivially_destructible_v<typename decltype(Members)::type> && ...)
   = default;

   // TODO: copy/move constructors, copy/move assignment operators
   //       These don't matter too much for now because of compiler limitations
   //       meaning that non-trivial destructable types can't be used

   template<fixed_string Name>
   constexpr auto get() const& noexcept -> const decltype(storage_.[:get_nth_member(get_index_by_name<Name>()):])*
   {
      constexpr auto index = get_index_by_name<Name>();
      if (index != index_) {
         return nullptr;
      }
      return &storage_.[:get_nth_member(index):];
   }

   template<fixed_string Name>
   constexpr auto get() & noexcept -> decltype(storage_.[:get_nth_member(get_index_by_name<Name>()):])*
   {
      constexpr auto index = get_index_by_name<Name>();
      if (index != index_) {
         return nullptr;
      }
      return &storage_.[:get_nth_member(index):];
   }

   template<fixed_string Name, typename T>
      requires std::convertible_to<T, decltype(*std::declval<named_variant>().template get<Name>())>
         && std::is_nothrow_constructible_v<decltype(*std::declval<named_variant>().template get<Name>()), T&&>
   constexpr void set(T&& value)
   {
      *this = create<Name>(std::forward<T>(value));
   }
};
// clang-format on

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

template<typename T>
struct delim_outputter {
   T& iter;
   bool first = true;

   constexpr void operator()() noexcept
   {
      if (!first) {
         *iter = ',';
         ++iter;
         *iter = ' ';
         ++iter;
      }
      first = false;
   }
};

template<typename T>
delim_outputter(T) -> delim_outputter<T>;

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
      typename FmtContext::iterator out = [&]<std::meta::info Type>() -> FmtContext::iterator {
         if constexpr (std::meta::has_identifier(Type)) {
            return std::format_to(ctx.out(), "{}{{", std::meta::identifier_of(Type));
         }
         else if constexpr (std::meta::has_template_arguments(Type)) {
            auto to_ret = std::format_to(ctx.out(), "{}<", std::meta::identifier_of(std::meta::template_of(Type)));
            auto output_delim = delim_outputter{to_ret};
            template for (constexpr auto t_arg : ::define_static_array(std::meta::template_arguments_of(Type)))
            {
               output_delim();
               ctx.advance_to(to_ret);
               if constexpr (std::meta::is_value(t_arg) || std::meta::is_object(t_arg)) {
                  to_ret = std::format_to(to_ret, "{}", as_debug([:t_arg:]));
               }
               else {
                  to_ret = std::format_to(to_ret, "{}", std::meta::display_string_of(t_arg));
               }
            }
            ctx.advance_to(to_ret);
            return std::format_to(to_ret, ">{{");
         }
         else {
            return std::format_to(ctx.out(), "<unnamed>{{");
         }
      }.template operator()<^^T>();

      auto output_delim = delim_outputter{out};

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
         std::format_to(out, ".{} = ", std::meta::identifier_of(mem));
         if constexpr (std::meta::is_class_type(std::meta::type_of(mem))) {
            out = std::format_to(out, "{}", as_debug(value.value.[:mem:]));
         }
         else {
            std::formatter<typename[:std::meta::remove_cvref(std::meta::type_of(mem)):]> member_formatter;
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
   using var = named_variant<named_pair<"wow", int>, named_pair<"wow2", int>, named_pair<"double", double>>;
   constexpr var a = var::create<"wow">(10);
   static_assert(!a.get<"wow2">());
   static_assert(*a.get<"wow">() == 10);
   var b = var::create<"wow">(20);
   b.set<"wow2">(10);
   assert(!b.get<"wow">());
   assert(b.get<"wow2">() && *b.get<"wow2">() == 10);
   b.set<"double">(20);
   assert(b.get<"double">() && *b.get<"double">() == 20.0);
   std::print("{}\n", as_debug(b));
}

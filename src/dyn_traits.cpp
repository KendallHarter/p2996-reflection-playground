#include "common.hpp"
#include "my_tuple.hpp"

#include <cassert>
#include <functional>
#include <memory>
#include <meta>
#include <new>
#include <ranges>
#include <string_view>

// OWNING MANUAL VERSION 1

struct owning_dyn_noise_trait {
private:
   struct base {
      constexpr virtual std::string_view get_noise() const noexcept = 0;
      constexpr virtual int volume(int) const noexcept = 0;
      constexpr virtual ~base() = default;
   };

   template<typename T>
   struct derived : base {
      constexpr explicit derived(T&& value) : value_{std::forward<T>(value)} {}

      constexpr std::string_view get_noise() const noexcept override { return value_.get_noise(); }

      constexpr int volume(int mult) const noexcept override { return value_.volume(mult); }

      constexpr ~derived() override = default;

      T value_;
   };

   std::unique_ptr<base> ptr_;

public:
   template<typename T>
   constexpr explicit owning_dyn_noise_trait(T&& value)
      // std::make_unique<derived> wasn't working...????
      : ptr_{new derived(std::forward<T>(value))}
   {}

   constexpr std::string_view get_noise() const noexcept { return ptr_->get_noise(); }

   constexpr int volume(int mult) const noexcept { return ptr_->volume(mult); }
};

// OWNING MANUAL VERSION 2

struct dyn_noise_funcs_struct {
   std::string_view (*get_noise)(const void*) noexcept;
   int (*volume)(const void*, int) noexcept;
   void (*destroy)(void*) noexcept;
};

template<typename T>
constexpr auto dyn_noise_funcs = dyn_noise_funcs_struct{
   [](const void* obj) noexcept { return static_cast<const T*>(obj)->get_noise(); },
   [](const void* obj, int mult) noexcept { return static_cast<const T*>(obj)->volume(mult); },
   [](void* obj) noexcept { static_cast<T*>(obj)->~T(); },
};

// TODO: Alignment + handle empty classes (can probably just pass nullptr?)
struct owning_dyn_noise_trait_alt final {
private:
   using funcs_ptr = const dyn_noise_funcs_struct*;
   std::unique_ptr<unsigned char[]> data_;

   const funcs_ptr* get_funcs_ptr() const noexcept
   {
      return std::launder(reinterpret_cast<const funcs_ptr*>(data_.get()));
   }

public:
   owning_dyn_noise_trait_alt() = delete;

   template<typename T>
   explicit owning_dyn_noise_trait_alt(T&& value)
      : data_{std::make_unique<unsigned char[]>(sizeof(dyn_noise_funcs_struct*) + sizeof(T))}
   {
      new (data_.get()) funcs_ptr(&dyn_noise_funcs<T>);
      new (data_.get() + sizeof(funcs_ptr)) T(std::forward<T>(value));
   }

   std::string_view get_noise() const noexcept
   {
      return (*get_funcs_ptr())->get_noise(data_.get() + sizeof(funcs_ptr));
   }

   int volume(int mult) const noexcept { return (*get_funcs_ptr())->volume(data_.get() + sizeof(funcs_ptr), mult); }

   ~owning_dyn_noise_trait_alt() { (*get_funcs_ptr())->destroy(data_.get() + sizeof(funcs_ptr)); }
};

// AUTOMATIC NON-OWNING VERSION

constexpr struct {
} default_impl;

template<std::meta::info... Info>
struct outer {
   struct inner;
   consteval { std::meta::define_aggregate(^^inner, {Info...}); }
};

template<std::meta::info... Info>
using cls = outer<Info...>::inner;

template<typename T, std::size_t Size>
constexpr auto array_to_tuple(const T (&arr)[Size])
{
   return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return tuple{arr[I]...};
   }.template operator()(std::make_index_sequence<Size>{});
}

template<typename T, std::size_t Size>
constexpr auto span_to_tuple(const std::span<const T, Size> sp)
{
   return [&]<std::size_t... I>(std::index_sequence<I...>) {
      return tuple{sp[I]...};
   }.template operator()(std::make_index_sequence<Size>{});
}

template<typename RetType, typename... Args>
using func_ptr_maker = RetType (*)(Args...);

template<typename RetType, typename... Args>
using noexcept_func_ptr_maker = RetType (*)(Args...) noexcept;

consteval std::meta::info
   member_func_to_non_member_func(std::meta::info f, std::meta::info trait, bool skip_first = false)
{
   std::vector<std::meta::info> infos;
   if (std::meta::is_function_template(f)) {
      return member_func_to_non_member_func(std::meta::substitute(f, {trait}), trait, true);
   }
   infos.push_back(std::meta::return_type_of(f));
   if (std::meta::is_const(f) || std::meta::is_static_member(f)) {
      infos.push_back(^^const void*);
   }
   else {
      infos.push_back(^^void*);
   }
   for (const auto i : std::meta::parameters_of(f) | std::views::drop(skip_first)) {
      infos.push_back(std::meta::type_of(i));
   }
   return std::meta::substitute(std::meta::is_noexcept(f) ? ^^noexcept_func_ptr_maker : ^^func_ptr_maker, infos);
}

consteval auto get_sorted_funcs_by_name(std::meta::info c) -> std::vector<std::meta::info>
{
   auto f = std::meta::members_of(c, std::meta::access_context::current())
          | std::views::filter([](auto x) { return std::meta::is_function_template(x) || std::meta::is_function(x); })
          | std::views::filter(std::not_fn(std::meta::is_constructor))
          | std::views::filter(std::not_fn(std::meta::is_operator_function))
          | std::views::filter(std::not_fn(std::meta::is_destructor)) | std::ranges::to<std::vector>();

   // This should really be using stable_sort, but Clang currently doesn't support it in constexpr contexts
   // This should be OK because it's always going in in the same order though.
   std::ranges::sort(f, {}, [](auto x) { return std::meta::identifier_of(x); });

   return f;
}

consteval auto partition_sorted_funcs_by_name(const std::span<const std::meta::info> funcs)
   -> std::vector<std::vector<std::meta::info>>
{
   std::vector<std::vector<std::meta::info>> to_ret;
   to_ret.emplace_back();

   auto cur_name = std::meta::identifier_of(funcs[0]);

   for (const auto f : funcs) {
      if (cur_name != std::meta::identifier_of(f)) {
         to_ret.emplace_back();
         cur_name = std::meta::identifier_of(f);
      }
      to_ret.back().push_back(f);
   }

   return to_ret;
}

// We don't need TraitClass, but have it to prevent passing other dyn_trait functions
template<typename TraitClass, auto... Rest>
struct func_caller;

template<typename TraitClass, std::size_t FuncIndex>
struct func_caller<TraitClass, FuncIndex> {
   template<typename Class, typename... Args>
   static constexpr auto operator()(const void* c, Args&&... args) noexcept(
      noexcept((static_cast<const Class*>(c)->funcs_->template get<FuncIndex>())(
         static_cast<const Class*>(c)->data_, std::forward<Args>(args)...))) -> decltype(auto)
   {
      const auto* const ptr = static_cast<const Class*>(c);
      return (ptr->funcs_->template get<FuncIndex>())(ptr->data_, std::forward<Args>(args)...);
   }

   template<typename Class, typename... Args>
   static constexpr auto
      operator()(void* c, Args&&... args) noexcept(noexcept((static_cast<Class*>(c)->funcs_->template get<FuncIndex>())(
         static_cast<Class*>(c)->data_, std::forward<Args>(args)...))) -> decltype(auto)
   {
      auto* const ptr = static_cast<Class*>(c);
      return (ptr->funcs_->template get<FuncIndex>())(ptr->data_, std::forward<Args>(args)...);
   }
};

template<std::size_t Index, typename FuncType>
struct func_caller_helper;

template<std::size_t Index, typename RetType, typename... Args>
struct func_caller_helper<Index, RetType (*)(Args...)> {
   static consteval auto operator()(Args...) noexcept -> std::integral_constant<std::size_t, Index>;
};

template<std::size_t Index, typename RetType, typename... Args>
struct func_caller_helper<Index, RetType (*)(Args...) noexcept> {
   static consteval auto operator()(Args...) noexcept -> std::integral_constant<std::size_t, Index>;
};

// trying to do this with a fixed_string resulted in it using the primary template for some reason
// const char* seems to work and since this is internally used only it doesn't really matter
template<typename TraitClass, std::size_t StartIndex, const char* Name>
struct func_caller<TraitClass, StartIndex, Name> {
   static constexpr std::span<const std::meta::info> funcs = []() consteval -> std::span<const std::meta::info> {
      static constexpr auto funcs = std::define_static_array(get_sorted_funcs_by_name(^^TraitClass));
      const auto sorted_funcs = partition_sorted_funcs_by_name(funcs);

      for (const auto& f : sorted_funcs) {
         if (std::meta::identifier_of(f.front()) == Name) {
            return std::define_static_array(f);
         }
      }

      return {};
   }();

   static_assert(!funcs.empty(), "No matching function name");

   static constexpr auto get_indexer = []() consteval {
      return []<std::size_t... I>(std::index_sequence<I...>) {
         return overload_set{func_caller_helper<
            StartIndex + I,
            typename[:member_func_to_non_member_func(funcs[I], ^^TraitClass):]>{}...};
      }(std::make_index_sequence<funcs.size()>{});
   }();

   template<typename Class, typename... Args>
   static constexpr decltype(auto) operator()(const void* c, Args&&... args) noexcept(noexcept((
      static_cast<const Class*>(c)
         ->funcs_->template get<decltype(get_indexer(std::declval<const Class*>(), std::declval<Args>()...))::value>())(
      static_cast<const Class*>(c)->data_, std::forward<Args>(args)...)))
   {
      const auto* const ptr = static_cast<const Class*>(c);
      return (ptr->funcs_
                 ->template get<decltype(get_indexer(std::declval<const Class*>(), std::declval<Args>()...))::value>())(
         ptr->data_, std::forward<Args>(args)...);
   }

   template<typename Class, typename... Args>
   static constexpr decltype(auto) operator()(void* c, Args&&... args) noexcept(noexcept((
      static_cast<Class*>(c)
         ->funcs_->template get<decltype(get_indexer(std::declval<const Class*>(), std::declval<Args>()...))::value>())(
      static_cast<Class*>(c)->data_, std::forward<Args>(args)...)))
   {
      auto* const ptr = static_cast<Class*>(c);

      return (ptr->funcs_
                 ->template get<decltype(get_indexer(std::declval<const Class*>(), std::declval<Args>()...))::value>())(
         ptr->data_, std::forward<Args>(args)...);
   }
};

consteval auto get_members_and_tuple_type(std::meta::info trait)
   -> std::pair<std::vector<std::meta::info>, std::vector<std::meta::info>>
{
   const auto funcs_by_name = partition_sorted_funcs_by_name(get_sorted_funcs_by_name(trait));
   std::vector<std::meta::info> members;
   std::vector<std::meta::info> func_ptrs;
   std::size_t index = 0;
   for (const auto funcs : funcs_by_name) {
      if (funcs.size() == 1) {
         // No overloads, just a single function
         const auto f = funcs.front();
         if (std::meta::is_function_template(f)) {
            // This is std::meta::annotations_of_with_type in C++26
            const auto is_default_impl
               = !std::meta::annotations_of(std::meta::substitute(f, {trait}), ^^decltype(default_impl)).empty();
            assert(is_default_impl && "Templated functions can only be used for default implementations");
         }
         members.push_back(
            std::meta::reflect_constant(
               std::meta::data_member_spec(
                  std::meta::substitute(^^func_caller, {trait, std::meta::reflect_constant(index)}),
                  {.name = std::meta::identifier_of(f), .no_unique_address = true})));
         index += 1;
         func_ptrs.push_back(member_func_to_non_member_func(f, trait));
      }
      else {
         // Oh no, there's an overload; gotta handle it
         for (const auto& f : funcs) {
            if (std::meta::is_function_template(f)) {
               // This is std::meta::annotations_of_with_type in C++26
               const auto is_default_impl
                  = !std::meta::annotations_of(std::meta::substitute(f, {trait}), ^^decltype(default_impl)).empty();
               assert(is_default_impl && "Templated functions can only be used for default implementations");
            }
            func_ptrs.push_back(member_func_to_non_member_func(f, trait));
         }
         members.push_back(
            std::meta::reflect_constant(
               std::meta::data_member_spec(
                  std::meta::substitute(
                     ^^func_caller,
                     {trait,
                      std::meta::reflect_constant(index),
                      ::reflect_constant_string(std::meta::identifier_of(funcs.front()))}),
                  {.name = std::meta::identifier_of(funcs.front()), .no_unique_address = true})));
         index += funcs.size();
      }
   }
   return {members, func_ptrs};
}

consteval std::meta::info make_non_owning_dyn_trait(std::meta::info trait, bool is_const) noexcept
{
   auto [members, func_ptrs] = get_members_and_tuple_type(trait);
   members.push_back(
      std::meta::reflect_constant(std::meta::data_member_spec(is_const ? ^^const void* : ^^void*, {.name = "data_"})));
   members.push_back(
      std::meta::reflect_constant(
         std::meta::data_member_spec(
            std::meta::add_pointer(std::meta::add_const(std::meta::substitute(^^tuple, func_ptrs))),
            {.name = "funcs_"})));
   return std::meta::substitute(^^cls, members);
}

template<typename Trait, bool IsConst>
using non_owning_dyn_trait_impl = [:make_non_owning_dyn_trait(^^Trait, IsConst):];

// Something about the previous type alias inhibits argument deduction
// So do this dumb workaround instead
template<typename Trait, bool IsConst>
struct non_owning_dyn_trait : non_owning_dyn_trait_impl<Trait, IsConst> {};

template<std::meta::info F, typename Ptr, typename Class, typename... Args>
constexpr auto produce_func_ptr
   = +[](Ptr c, Args... args) noexcept(noexcept(static_cast<Class>(c)->[:F:](args...))) -> decltype(auto) {
   return static_cast<Class>(c)->[:F:](args...);
};

template<std::meta::info F, typename Trait, typename Ptr, typename Class, typename... Args>
constexpr auto produce_default_func_ptr
   = +[](Ptr c, Args... args) noexcept(noexcept(Trait{}.[:F:](*static_cast<Class>(c), args...))) -> decltype(auto) {
   return Trait{}.[:F:](*static_cast<Class>(c), args...);
};

template<std::meta::info F, typename... Args>
constexpr auto produce_default_static_func_ptr
   = +[](const void*, Args... args) noexcept(noexcept([:F:](args...))) -> decltype(auto) { return [:F:](args...); };

template<typename Trait, typename ToStore>
consteval auto make_dyn_trait_pointers()
{
   static constexpr auto func_ptrs = std::define_static_array(get_members_and_tuple_type(^^Trait).second);
   static constexpr auto trait_funcs = std::define_static_array(get_sorted_funcs_by_name(^^Trait));

   static constexpr auto to_store_func = std::define_static_array(
      std::meta::members_of(^^ToStore, std::meta::access_context::current())

      | std::views::filter([](auto x) { return std::meta::is_function_template(x) || std::meta::is_function(x); })
      | std::views::filter(std::not_fn(std::meta::is_constructor))
      | std::views::filter(std::not_fn(std::meta::is_operator_function))
      | std::views::filter(std::not_fn(std::meta::is_destructor)));

   using ret_type = [:std::meta::substitute(^^tuple, func_ptrs):];

   return []<std::size_t... Is>(std::index_sequence<Is...>) {
      return ret_type {
         // clang-format off
         []<std::size_t I>() -> [:func_ptrs[I]:] {
            // clang-format on
            static constexpr auto produce_func_ptr_from_info = [](std::meta::info func_info, std::meta::info sub_into) {
               const auto is_default = sub_into == ^^produce_default_func_ptr;
               std::vector<std::meta::info> args;
               args.push_back(std::meta::reflect_constant(func_info));
               if (is_default) {
                  args.push_back(^^Trait);
               }
               if (std::meta::is_const(func_info) || std::meta::is_static_member(func_info)) {
                  args.push_back(^^const void*);
                  args.push_back(^^const ToStore*);
               }
               else {
                  args.push_back(^^void*);
                  args.push_back(^^ToStore*);
               }
               for (const auto arg :
                    std::meta::parameters_of(func_info) | std::views::drop(static_cast<int>(is_default))) {
                  args.push_back(std::meta::type_of(arg));
               }

               return std::meta::substitute(sub_into, args);
            };
            template for (constexpr auto f : to_store_func)
            {
               static constexpr bool func_signatures_match = []() consteval {
                  if constexpr (!std::meta::is_function_template(f)) {
                     const auto cur_func = std::meta::is_function_template(trait_funcs[I])
                                            ? std::meta::substitute(trait_funcs[I], {^^ToStore})
                                            : trait_funcs[I];
                     const auto params1 = std::meta::parameters_of(f);
                     const std::vector<std::meta::info> params2
                        = std::meta::parameters_of(cur_func)
                        | std::views::drop(static_cast<int>(cur_func != trait_funcs[I]))
                        | std::ranges::to<std::vector>();

                     if (params1.size() != params2.size()) {
                        return false;
                     }

                     for (std::size_t i = 0; i < params1.size(); ++i) {
                        if (std::meta::type_of(params1[i]) != std::meta::type_of(params2[i])) {
                           return false;
                        }
                     }

                     return std::meta::return_type_of(f) == std::meta::return_type_of(cur_func);
                  }

                  return false;
               }();
               if constexpr (
                  std::meta::identifier_of(f) == std::meta::identifier_of(trait_funcs[I]) && func_signatures_match) {
                  return [:produce_func_ptr_from_info(f, ^^produce_func_ptr):];
               }
            }
            // Default implementation
            static constexpr auto f = trait_funcs[I];
            if constexpr (std::meta::is_function_template(f)) {
               // This is std::meta::annotations_of_with_type in C++26
               static constexpr auto f_sub = std::meta::substitute(f, {^^ToStore});
               static constexpr auto is_default = !std::meta::annotations_of(f_sub, ^^decltype(default_impl)).empty();
               static_assert(is_default, "Function templates must be default implementations");

               return [:produce_func_ptr_from_info(f_sub, ^^produce_default_func_ptr):];
            }
            else {
               // This is std::meta::annotations_of_with_type in C++26
               static constexpr auto is_default = !std::meta::annotations_of(f, ^^decltype(default_impl)).empty();
               if constexpr (is_default && std::meta::is_static_member(trait_funcs[I])) {
                  static constexpr auto to_ret = []() {
                     std::vector<std::meta::info> args;
                     args.push_back(std::meta::reflect_constant(f));
                     for (const auto arg : std::meta::parameters_of(f)) {
                        args.push_back(arg);
                     }
                     return std::meta::substitute(^^produce_default_static_func_ptr, args);
                  }();
                  return [:to_ret:];
               }
            }
            throw "invalid name/no default";
         }.template operator()<Is>()...
      };
   }.template operator()(std::make_index_sequence<trait_funcs.size()>{});
}

template<typename DynTrait, typename ToStore>
constexpr auto make_dyn_trait(const ToStore* ptr) noexcept
{
   return non_owning_dyn_trait<DynTrait, true>{
      {.data_ = ptr, .funcs_ = ::define_static_object(make_dyn_trait_pointers<DynTrait, ToStore>())}};
}

template<typename DynTrait, typename ToStore>
constexpr auto make_dyn_trait(ToStore* ptr) noexcept
{
   return non_owning_dyn_trait<DynTrait, false>{
      {.data_ = ptr, .funcs_ = ::define_static_object(make_dyn_trait_pointers<DynTrait, ToStore>())}};
}

template<typename Trait, bool ConstSelf, auto... FuncCallerRest, typename... T>
constexpr auto dyn_call(
   non_owning_dyn_trait<Trait, ConstSelf> self,
   func_caller<Trait, FuncCallerRest...> to_call,
   T&&... args) noexcept(noexcept(to_call.template
                                  operator()<non_owning_dyn_trait<Trait, ConstSelf>>(&self, std::forward<T>(args)...)))
{
   return to_call.template operator()<non_owning_dyn_trait<Trait, ConstSelf>>(&self, std::forward<T>(args)...);
}

// USING THEM

struct noise_trait {
   static std::string_view get_noise() noexcept;

   [[= default_impl]] static constexpr std::string_view get_secondary_noise() noexcept { return "(none)"; }

   template<typename T>
   [[= default_impl]] constexpr int volume(const T& obj) const noexcept
   {
      return obj.volume(1);
   }

   int volume(int) const noexcept;
   void get_louder() noexcept;

   template<typename T>
   [[= default_impl]] constexpr void get_louder_twice(T& obj) noexcept
   {
      obj.get_louder();
      obj.get_louder();
   }
};

struct cow {
   static constexpr std::string_view get_noise() noexcept { return "moo"; }
   constexpr int volume(int multiplier) const noexcept { return volume_ * multiplier; }
   constexpr void get_louder() noexcept { volume_ += 1; }

   int volume_ = 1;
};

struct dog {
   static constexpr std::string_view get_noise() noexcept { return "arf"; }
   static constexpr std::string_view get_secondary_noise() noexcept { return "bark"; }
   constexpr int volume(int multiplier) const noexcept { return volume_ * multiplier; }
   constexpr void get_louder() noexcept { volume_ *= 2; }

   int volume_ = 9;
};

int main()
{
   static_assert(owning_dyn_noise_trait{cow{}}.get_noise() == "moo");
   static_assert(owning_dyn_noise_trait{dog{}}.volume(1) == 9);

   const auto test2 = owning_dyn_noise_trait_alt{cow{}};
   assert(test2.get_noise() == "moo");
   assert(test2.volume(1) == 1);

   static constexpr cow c{};
   static constexpr dog d{};
   // static constexpr non_owning_noise_trait owner1{&c};
   // static_assert(dyn_call(owner1, owner1.get_noise) == "moo");

   static constexpr auto owner2 = make_dyn_trait<noise_trait>(&c);
   static_assert(dyn_call(owner2, owner2.get_noise) == "moo");
   static_assert(noexcept(dyn_call(owner2, owner2.get_noise)));

   static constexpr auto owner3 = make_dyn_trait<noise_trait>(&d);
   static_assert(dyn_call(owner3, owner3.volume) == 9);
   static_assert(dyn_call(owner3, owner3.volume, 2) == 18);
   static_assert(dyn_call(owner3, owner3.get_secondary_noise) == "bark");

   consteval
   {
      cow cow2{};
      const auto trait = make_dyn_trait<noise_trait>(&cow2);
      dyn_call(trait, trait.get_louder);
      assert(dyn_call(trait, trait.volume, 1) == 2);
      assert(dyn_call(trait, trait.get_secondary_noise) == "(none)");
      dyn_call(trait, trait.get_louder_twice);
      assert(dyn_call(trait, trait.volume, 1) == 4);
   }
}

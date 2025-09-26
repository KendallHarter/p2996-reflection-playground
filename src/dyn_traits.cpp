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

// VARIABLE BASED NON-OWNING VERSION

// I don't think there's a good way of doing this with a clean format because there's no way to pass
// the first/the implicit this argument without tons of memory overhead and making it non-copyable, etc.

// TODO: conditional noexcept
template<std::size_t FuncIndex, bool IsConst>
struct func_caller {
   template<typename Class, typename... Args>
      requires(IsConst)
   static constexpr decltype(auto) call(const void* c, Args&&... args) noexcept
   {
      const auto* const ptr = static_cast<const Class*>(c);
      return (ptr->funcs_->template get<FuncIndex>())(ptr->data_, std::forward<Args>(args)...);
   }

   template<typename Class, typename... Args>
      requires(!IsConst)
   static constexpr decltype(auto) call(void* c, Args&&... args) noexcept
   {
      auto* const ptr = static_cast<Class*>(c);
      return (ptr->funcs_->template get<FuncIndex>())(ptr->data_, std::forward<Args>(args)...);
   }
};

// Commenting out since future changes will probably break this
// using non_owning_noise_funcs_struct = tuple<std::string_view (*)(const void*), int (*)(const void*, int)>;

// struct non_owning_noise_trait {
// public:
//    non_owning_noise_trait() = delete;

//    template<typename T>
//    constexpr explicit non_owning_noise_trait(const T* data)
//       : funcs_{define_static_object(
//            non_owning_noise_funcs_struct{
//               [](const void* c) -> decltype(auto) { return static_cast<const T*>(c)->get_noise(); },
//               [](const void* c, int mult) -> decltype(auto) { return static_cast<const T*>(c)->volume(mult); }})}
//       , data_{data}
//    {}

//    [[no_unique_address]] func_caller<0, true> get_noise;
//    [[no_unique_address]] func_caller<1, true> volume;

// private:
//    template<std::size_t, bool>
//    friend struct func_caller;

//    const non_owning_noise_funcs_struct* funcs_;
//    const void* data_;
// };

// TODO: Condition noexcept
template<typename Trait, typename... T>
constexpr auto dyn_call(Trait self, const auto to_call, T&&... args) noexcept
{
   return to_call.template call<Trait>(&self, std::forward<T>(args)...);
}

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

consteval std::meta::info member_func_to_non_member_func(std::meta::info f)
{
   std::vector<std::meta::info> infos;
   infos.push_back(std::meta::return_type_of(f));
   if (std::meta::is_const(f) || std::meta::is_static_member(f)) {
      infos.push_back(^^const void*);
   }
   else {
      infos.push_back(^^void*);
   }
   for (const auto i : std::meta::parameters_of(f)) {
      infos.push_back(std::meta::type_of(i));
   }
   return std::meta::substitute(^^func_ptr_maker, infos);
}

consteval auto get_members_and_tuple_type(std::meta::info trait)
   -> std::pair<std::vector<std::meta::info>, std::vector<std::meta::info>>
{
   auto funcs = std::meta::members_of(trait, std::meta::access_context::current())
              | std::views::filter(std::meta::is_function) | std::views::filter(std::not_fn(std::meta::is_constructor))
              | std::views::filter(std::not_fn(std::meta::is_operator_function))
              | std::views::filter(std::not_fn(std::meta::is_destructor));

   std::vector<std::meta::info> members;
   std::vector<std::meta::info> func_ptrs;
   int index = 0;
   for (const auto f : funcs) {
      members.push_back(
         std::meta::reflect_constant(
            std::meta::data_member_spec(
               std::meta::substitute(
                  ^^func_caller,
                  {std::meta::reflect_constant(index), std::meta::reflect_constant(std::meta::is_const(f))}),
               {.name = std::meta::identifier_of(f), .no_unique_address = true})));
      index += 1;
      func_ptrs.push_back(member_func_to_non_member_func(f));
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
using non_owning_dyn_trait = [:make_non_owning_dyn_trait(^^Trait, IsConst):];

template<std::meta::info F, typename Ptr, typename Class, typename... Args>
constexpr auto produce_func_ptr
   = [](Ptr c, Args... args) -> decltype(auto) { return static_cast<Class>(c)->[:F:](args...); };

template<std::meta::info F, typename... Args>
constexpr auto produce_default_static_func_ptr
   = [](const void*, Args... args) -> decltype(auto) { return [:F:](args...); };

template<typename Trait, typename ToStore>
consteval auto make_dyn_trait_pointers()
{
   static constexpr auto func_ptrs = std::define_static_array(get_members_and_tuple_type(^^Trait).second);
   static constexpr auto trait_funcs = std::define_static_array(
      std::meta::members_of(^^Trait, std::meta::access_context::current()) | std::views::filter(std::meta::is_function)
      | std::views::filter(std::not_fn(std::meta::is_constructor))
      | std::views::filter(std::not_fn(std::meta::is_operator_function))
      | std::views::filter(std::not_fn(std::meta::is_destructor)));

   static constexpr auto to_store_func = std::define_static_array(
      std::meta::members_of(^^ToStore, std::meta::access_context::current())
      | std::views::filter(std::meta::is_function) | std::views::filter(std::not_fn(std::meta::is_constructor))
      | std::views::filter(std::not_fn(std::meta::is_operator_function))
      | std::views::filter(std::not_fn(std::meta::is_destructor)));

   using ret_type = [:std::meta::substitute(^^tuple, func_ptrs):];

   return []<std::size_t... Is>(std::index_sequence<Is...>) {
      return ret_type {
         []<std::size_t I>() -> [ : func_ptrs[I] : ] {
            static constexpr auto produce_func_ptr_from_info = [](std::meta::info func_info) {
               std::vector<std::meta::info> args;
               args.push_back(std::meta::reflect_constant(func_info));
               if (std::meta::is_const(func_info) || std::meta::is_static_member(func_info)) {
                  args.push_back(^^const void*);
                  args.push_back(^^const ToStore*);
               }
               else {
                  args.push_back(^^void*);
                  args.push_back(^^ToStore*);
               }
               for (const auto arg : std::meta::parameters_of(func_info)) {
                  args.push_back(std::meta::type_of(arg));
               }

               return std::meta::substitute(^^produce_func_ptr, args);
            };
            template for (constexpr auto f : to_store_func)
            {
               if constexpr (std::meta::identifier_of(f) == std::meta::identifier_of(trait_funcs[I])) {
                  return [:produce_func_ptr_from_info(f):];
               }
            }
            // Default implementation
            // This is std::meta::annotations_of_with_type in C++26
            static constexpr auto f = trait_funcs[I];
            static constexpr auto is_default = !std::meta::annotations_of(f, ^^decltype(default_impl)).empty();
            if constexpr (is_default) {
               if constexpr (std::meta::is_static_member(trait_funcs[I])) {
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
      .data_ = ptr, .funcs_ = ::define_static_object(make_dyn_trait_pointers<DynTrait, ToStore>())};
}

template<typename DynTrait, typename ToStore>
constexpr auto make_mut_dyn_trait(ToStore* ptr) noexcept
{
   return non_owning_dyn_trait<DynTrait, false>{
      .data_ = ptr, .funcs_ = ::define_static_object(make_dyn_trait_pointers<DynTrait, ToStore>())};
}

// USING THEM

struct noise_trait {
   static std::string_view get_noise() noexcept;

   [[= default_impl]] static constexpr std::string_view get_secondary_noise() noexcept { return "(none)"; }

   int volume(int) const noexcept;
   void get_louder() noexcept;
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

   static constexpr auto owner3 = make_dyn_trait<noise_trait>(&d);
   static_assert(dyn_call(owner3, owner3.volume, 2) == 18);
   static_assert(dyn_call(owner3, owner3.get_secondary_noise) == "bark");

   consteval
   {
      cow cow2{};
      const auto trait = make_mut_dyn_trait<noise_trait>(&cow2);
      dyn_call(trait, trait.get_louder);
      assert(dyn_call(trait, trait.volume, 1) == 2);
      assert(dyn_call(trait, trait.get_secondary_noise) == "(none)");
   }
}

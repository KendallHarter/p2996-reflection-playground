#ifndef RUNTIME_SETTER_STUFF_HPP
#define RUNTIME_SETTER_STUFF_HPP

template<typename SetIn, typename T>
consteval bool can_set_with_type()
{
   static constexpr auto members
      = ::define_static_array(std::meta::nonstatic_data_members_of(^^SetIn, std::meta::access_context::unchecked()));
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
      = ::define_static_array(std::meta::nonstatic_data_members_of(^^SetIn, std::meta::access_context::unchecked()));
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
         = ::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
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
         = ::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()));
      template for (constexpr auto mem : nsdm)
      {
         if (std::meta::identifier_of(mem) == name) {
            return &get_from.[:mem:];
         }
      }
      return std::nullopt;
   }

#endif

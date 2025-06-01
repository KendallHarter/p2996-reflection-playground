import module_test;

#include <experimental/meta>
#include <print>
#include <unordered_map>

namespace a { namespace b {
struct c {
   std::unordered_map<int, int> x;
};
}} // namespace a::b

int main()
{
   using type = a::b::c;
   std::println("{} {}", get_fully_qualified_type(^^decltype(type::x)), get_fully_qualified_name(^^type::x));
}

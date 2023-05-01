#ifndef CAPTURER_ENUM_H
#define CAPTURER_ENUM_H

#include <type_traits>

#define ENABLE_BITMASK_OPERATORS() __ENABLE_BITMASK_OPERATORS__

template<typename T, typename _ = void> struct is_bitmask_enum : std::false_type
{};

template<typename T>
struct is_bitmask_enum<T, std::enable_if_t<sizeof(T::__ENABLE_BITMASK_OPERATORS__)>> : std::true_type
{};

template<class Enum>
concept Bitmask = requires { is_bitmask_enum<Enum>::value&& std::is_enum_v<Enum>; };

// bitwise operators
template<Bitmask Enum> constexpr Enum operator|(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) |
                             static_cast<std::underlying_type_t<Enum>>(rhs));
}

template<Bitmask Enum> constexpr Enum operator&(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) &
                             static_cast<std::underlying_type_t<Enum>>(rhs));
}

template<Bitmask Enum> constexpr Enum operator^(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) ^
                             static_cast<std::underlying_type_t<Enum>>(rhs));
}

template<Bitmask Enum> constexpr Enum operator~(Enum rhs)
{
    return static_cast<Enum>(~static_cast<std::underlying_type_t<Enum>>(rhs));
}

template<Bitmask Enum> constexpr Enum& operator|=(Enum& lhs, Enum rhs)
{
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) |
                            static_cast<std::underlying_type_t<Enum>>(rhs));
    return lhs;
}

template<Bitmask Enum> constexpr Enum& operator&=(Enum& lhs, Enum rhs)
{
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) &
                            static_cast<std::underlying_type_t<Enum>>(rhs));
    return lhs;
}

template<Bitmask Enum> constexpr Enum& operator^=(Enum& lhs, Enum rhs)
{
    lhs = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) ^
                            static_cast<std::underlying_type_t<Enum>>(rhs));
    return lhs;
}

template<Bitmask Enum> constexpr Enum operator<<(Enum lhs, int bits)
{
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) << bits);
}

template<Bitmask Enum> constexpr Enum operator>>(Enum lhs, int bits)
{
    return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(lhs) >> bits);
}

template<Bitmask Enum> constexpr bool any(Enum lhs) { return lhs != static_cast<Enum>(0); }

#endif //! CAPTURER_ENUM_H
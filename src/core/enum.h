#ifndef FFMPEG_EXAMPLES_ENUM_H
#define FFMPEG_EXAMPLES_ENUM_H

#include <type_traits>

#define DEFINE_BITMASK_OPERATORS_FOR(Enum)              \
template<>                                              \
struct is_bitmask_enum<Enum> : std::true_type { };

template<typename T>
struct is_bitmask_enum : std::false_type { };

template<typename Enum>
inline constexpr bool is_bitmask_enum_v = is_bitmask_enum<Enum>::value;

// bitwise operators
template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator | (Enum lhs, Enum rhs) {
    return static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) | static_cast<std::underlying_type_t<Enum>>(rhs)
    );
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator & (Enum lhs, Enum rhs) {
    return static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) & static_cast<std::underlying_type_t<Enum>>(rhs)
    );
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator ^ (Enum lhs, Enum rhs) {
    return static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) ^ static_cast<std::underlying_type_t<Enum>>(rhs)
    );
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator ~ (Enum rhs) {
    return static_cast<Enum>(
            ~static_cast<std::underlying_type_t<Enum>>(rhs)
    );
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator |= (Enum lhs, Enum rhs) {
    lhs = static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) | static_cast<std::underlying_type_t<Enum>>(rhs)
    );
    return lhs;
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator &= (Enum lhs, Enum rhs) {
    lhs = static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) & static_cast<std::underlying_type_t<Enum>>(rhs)
    );
    return lhs;
}

template<typename Enum>
typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum>
operator ^= (Enum lhs, Enum rhs) {
    lhs = static_cast<Enum>(
            static_cast<std::underlying_type_t<Enum>>(lhs) ^ static_cast<std::underlying_type_t<Enum>>(rhs)
    );
    return lhs;
}

#endif
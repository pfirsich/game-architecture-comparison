#pragma once

#include <fmt/core.h>
#include <lua.hpp>

namespace luax {
template <typename... Args>
[[noreturn]] void error(lua_State* L, fmt::format_string<Args...> fstr, Args&&... args)
{
    const auto msg = fmt::format(fstr, std::forward<Args>(args)...);
    lua_pushlstring(L, msg.data(), msg.size());
    lua_error(L); // does not return
    std::abort(); // only to make compiler believe this is [[noreturn]]
}

bool is_integer(lua_State* L, int idx)
{
    const auto n = lua_tonumber(L, idx);
    const auto i = lua_tointeger(L, idx);
    return i == n;
}

template <typename T>
T get_arg(lua_State* L, int idx);

template <>
bool get_arg<bool>(lua_State* L, int idx)
{
    if (!lua_isboolean(L, idx)) {
        error(L, "Expected integer for argument {}, got {}", idx, luaL_typename(L, idx));
    }
    return lua_toboolean(L, idx) != 0;
}

template <>
int get_arg<int>(lua_State* L, int idx)
{
    if (!is_integer(L, idx)) {
        error(L, "Expected integer for argument {}, got {}", idx, luaL_typename(L, idx));
    }
    return static_cast<int>(lua_tointeger(L, idx));
}

template <>
uint32_t get_arg<uint32_t>(lua_State* L, int idx)
{
    if (!is_integer(L, idx)) {
        error(L, "Expected integer for argument {}, got {}", idx, luaL_typename(L, idx));
    }
    return static_cast<uint32_t>(lua_tointeger(L, idx));
}

template <>
float get_arg<float>(lua_State* L, int idx)
{
    if (!lua_isnumber(L, idx)) {
        error(L, "Expected number for argument {}, got {}", idx, luaL_typename(L, idx));
    }
    return static_cast<float>(lua_tonumber(L, idx));
}

template <>
std::string_view get_arg<std::string_view>(lua_State* L, int idx)
{
    if (!lua_isstring(L, idx)) {
        error(L, "Expected string for argument {}, got {}", idx, luaL_typename(L, idx));
    }
    size_t len = 0;
    const auto ptr = lua_tolstring(L, idx, &len);
    assert(ptr);
    return std::string_view(ptr, len);
}

namespace detail {
    template <typename T>
    struct is_optional : std::false_type { };

    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type { };
}

// Partial function template specialization of get_arg is not possible, so we have to wrap
template <typename T>
T get_arg_wrap(lua_State* L, int idx)
{
    if constexpr (detail::is_optional<T>::value) {
        if (lua_isnoneornil(L, idx)) {
            return std::nullopt;
        }
        return get_arg<typename T::value_type>(L, idx);
    } else {
        return get_arg<T>(L, idx);
    }
}

// Instead of min_args and max_args we could deduce it ourselves from the types (detect the last
// non-optional argument) but this is not worth the effort or the compile time increase.
template <typename... Args>
std::tuple<Args...> get_args(
    lua_State* L, size_t min_args = sizeof...(Args), size_t max_args = sizeof...(Args))
{
    const auto num_args = static_cast<size_t>(lua_gettop(L));
    if (num_args < min_args || num_args > max_args) {
        error(L, "Expected between {} and {} arguments, got {}", min_args, max_args, num_args);
    }
    return get_args_impl<Args...>(L, std::index_sequence_for<Args...> {});
}

template <typename... Args, std::size_t... Indices>
std::tuple<Args...> get_args_impl(lua_State* L, std::index_sequence<Indices...>)
{
    return std::make_tuple(get_arg_wrap<Args>(L, Indices + 1)...);
}

struct nil_t { };
inline constexpr nil_t nil;

template <typename T>
void push(lua_State* L, const T& v);

template <>
void push<nil_t>(lua_State* L, const nil_t&)
{
    lua_pushnil(L);
}

template <>
void push<int>(lua_State* L, const int& v)
{
    lua_pushinteger(L, v);
}

template <>
void push<uint32_t>(lua_State* L, const uint32_t& v)
{
    lua_pushnumber(L, static_cast<lua_Number>(v));
}

template <>
void push<float>(lua_State* L, const float& v)
{
    lua_pushnumber(L, static_cast<lua_Number>(v));
}

template <>
void push<bool>(lua_State* L, const bool& v)
{
    lua_pushboolean(L, v ? 1 : 0);
}

template <typename... Args>
int ret(lua_State* L, Args&&... args)
{
    (push(L, std::forward<Args>(args)), ...);
    return sizeof...(Args);
}

int error_handler(lua_State* L)
{
    fmt::println("handler");
    const char* message = lua_tostring(L, 1);

    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1)) {
        fmt::println("no debug found");
        lua_pop(L, 1); // remove non-table result
        lua_pushstring(L, message);
        return 1;
    }

    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        fmt::println("no traceback found");
        lua_pop(L, 2); // remove 'debug' and non-function result
        lua_pushstring(L, message);
        return 1;
    }

    // Call debug.traceback with the error message and level 2 (skip this function)
    lua_pushvalue(L, 1); // error message
    lua_pushinteger(L, 2); // skip this function and error handler
    lua_call(L, 2, 1); // call debug.traceback
    // The stack now contains only the result of debug.traceback

    return 1; // return the traceback string
}

std::string lua_value_to_string(lua_State* L, int idx)
{
    const auto type = lua_type(L, idx);
    switch (type) {
    case LUA_TSTRING:
        return fmt::format("(string) \"{}\"", lua_tostring(L, idx));
    case LUA_TBOOLEAN:
        return fmt::format("(boolean) {}", lua_toboolean(L, idx) ? "true" : "false");
    case LUA_TNUMBER:
        return fmt::format("(number) {}", lua_tonumber(L, idx));
    case LUA_TNIL:
        return "nil";
    case LUA_TTABLE:
        return "(table)";
    case LUA_TFUNCTION:
        return "(function)";
    case LUA_TUSERDATA:
        return fmt::format("(userdata) {}", fmt::ptr(lua_touserdata(L, idx)));
    case LUA_TTHREAD:
        return "(thread)";
    case LUA_TLIGHTUSERDATA:
        return fmt::format("(lightuserdata) {}", fmt::ptr(lua_touserdata(L, idx)));
    default:
        return "unknown";
    }
}

void print_stack(lua_State* L)
{
    for (int i = 1; i < lua_gettop(L); ++i) {
        fmt::println("{}: {}", i, lua_value_to_string(L, i));
    }
}
}
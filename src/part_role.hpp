#pragma once

// STEP layers carry the construction role independently of the display name.
namespace part_role {
enum class Role { Board, Hardware };
inline constexpr const char* boardLayer = "DrawerRole:Board";
inline constexpr const char* hardwareLayer = "DrawerRole:Hardware";
inline const char* layer(Role role) { return role==Role::Board?boardLayer:hardwareLayer; }
}

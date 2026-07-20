#include "../src/WearPermissionPolicy.h"

#include <cassert>
#include <cstdint>

int main()
{
    constexpr std::uint8_t LIMIT_NONE = 0;
    constexpr std::uint8_t LIMIT_XIANQI = 2;
    WearControl::PermissionLevels levels;

    assert(WearControl::MakePermissionKey(LIMIT_XIANQI, 11) !=
           WearControl::MakePermissionKey(LIMIT_XIANQI, 15));

    levels[WearControl::MakePermissionKey(LIMIT_XIANQI, 11)] = 3;
    assert(WearControl::ResolvePermissionLevel(levels, LIMIT_XIANQI, 11, 1) == 3);
    assert(WearControl::ResolvePermissionLevel(levels, LIMIT_XIANQI, 15, 1) == 1);

    levels[WearControl::MakePermissionKey(LIMIT_NONE, 0)] = 5;
    assert(WearControl::ResolvePermissionLevel(levels, LIMIT_XIANQI, 11, 1) == 3);
    assert(WearControl::ResolvePermissionLevel(levels, LIMIT_XIANQI, 15, 1) == 1);

    return 0;
}

#ifndef PLACEMENT_NEW_H
#define PLACEMENT_NEW_H

#include <utility>

namespace AC
{
    // 使用 placement new 替代 std::construct_at
    template <typename T, typename... Args>
    inline T* construct_at(T* p, Args&&... args)
    {
        return ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
    }
}

#endif // PLACEMENT_NEW_H

#pragma once

namespace mob {

#define MOB_ENUM_OPERATORS(E)                                                          \
    inline E operator|(E e1, E e2)                                                     \
    {                                                                                  \
        return (E)((int)e1 | (int)e2);                                                 \
    }                                                                                  \
    inline E operator&(E e1, E e2)                                                     \
    {                                                                                  \
        return (E)((int)e1 & (int)e2);                                                 \
    }                                                                                  \
    inline E operator|=(E& e1, E e2)                                                   \
    {                                                                                  \
        e1 = e1 | e2;                                                                  \
        return e1;                                                                     \
    }

#define MOB_ENUM_FRIEND_OPERATORS(E)                                                   \
    inline friend E operator|(E e1, E e2)                                              \
    {                                                                                  \
        return (E)((int)e1 | (int)e2);                                                 \
    }                                                                                  \
    inline friend E operator&(E e1, E e2)                                              \
    {                                                                                  \
        return (E)((int)e1 & (int)e2);                                                 \
    }                                                                                  \
    inline friend E operator|=(E& e1, E e2)                                            \
    {                                                                                  \
        e1 = e1 | e2;                                                                  \
        return e1;                                                                     \
    }

    template <class E>
    bool is_set(E e, E v)
    {
        static_assert(std::is_enum_v<E>, "this is for enums");
        return (e & v) == v;
    }

    template <class E>
    bool is_any_set(E e, E v)
    {
        static_assert(std::is_enum_v<E>, "this is for enums");
        return (e & v) != E(0);
    }

}  // namespace mob

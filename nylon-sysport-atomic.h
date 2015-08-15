#ifndef WIN_WEAVE_SYSPORT_ATOMIC_H__
#define WIN_WEAVE_SYSPORT_ATOMIC_H__

#include <Windows.h>

namespace Atomic {
    template< class T >
    inline T nylon_sysport_cmpxchg( volatile T* var, T oldval, T newval )
    {
//        char this_is_not_acceptable_you_must_provide_implementation[-1];
        throw std::runtime_error("what??");
    }


static inline unsigned
hpl_cmpxchg__cpp__
(
   volatile long *ptr,
   long old,
   long newval
)
{
}
    

    template<>
    inline long nylon_sysport_cmpxchg<long>( volatile long* var, long old, long newval )
    {
#if 0
        if (*var == oldval)
        {
            *var = newval;
            return oldval;
        }
        else
            return *var;
#elif __GNUC__
        return __sync_val_compare_and_swap( var, old, newval );
// # define LOCK_PREFIX "\n\tlock; "
// 	long prev;
//    __asm__ __volatile__( LOCK_PREFIX "cmpxchgl %1,%2"
//       : "=a"(prev)
//       : "r"(newval), "m"(*ptr), "0"(old)
//       : "memory");
//    return prev;
//        return hpl_cmpxchg__cpp__( var, oldval, newval );
#else
        return InterlockedCompareExchange( var, newval, old );
#endif 
    }

    // This is a bit frustrating, because VS2010 is compiling this function
    // even if it's never instantiated, which makes for a link problem
    // on windows XP which doesn't provide InterlockedCompareExchange64
    // GRR.  I don't think GCC does that.
    
//     template<>
//     long long nylon_sysport_cmpxchg<long long>( volatile long long* var, long long oldval, long long newval )
//     {
//         char this_is_not_acceptable_you_must_provide_implementation[-1];
//         return InterlockedCompareExchange64( var, newval, oldval );
//     }
}

      
#endif

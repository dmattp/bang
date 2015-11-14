#ifndef WIN_WEAVE_SYSPORT_ATOMIC_H__
#define WIN_WEAVE_SYSPORT_ATOMIC_H__

#ifndef __GNUC__
# include <Windows.h>
#endif 

namespace Atomic {

#if __GNUC__    
    inline long nylon_sysport_cmpxchg( volatile long* var, long oldval, long newval )
    {
        return __sync_val_compare_and_swap( var, oldval, newval );
    }

    inline long long nylon_sysport_cmpxchg( volatile long long* var, long long oldval, long long newval )
    {
        return __sync_val_compare_and_swap( var, oldval, newval );
    }
#else
    inline long nylon_sysport_cmpxchg( volatile long* var, long old, long newval )
    {
        return InterlockedCompareExchange( var, newval, old );
    }
#endif

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

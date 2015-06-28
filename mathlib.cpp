
#include "bang.h"

#include <string>
#include <math.h>
#include <stdlib.h>

namespace Math
{
    void random( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        s.push( double(::rand()) );
    }

    void checknumbertype( const Bang::Value& v )
    {
        if (!v.isnum())
            throw std::runtime_error("Math lib incompatible type");
    }

    inline void math1numXform( Bang::Stack& s, double (*operation)(double) )
    {
        Bang::Value& v1 = s.loc_topMutate();
        checknumbertype( v1 );
        v1.mutateNoType( operation( v1.tonum() ) );
    }

    inline void infix2to1( Bang::Stack& s, double (*operation)(double, double) )
    {
        const Bang::Value& v2 = s.loc_top();
        Bang::Value& v1 = s.loc_topMinus1Mutate();

        checknumbertype(v1);
        checknumbertype(v2);
        v1.mutateNoType( operation( v1.tonum(), v2.tonum() ) );
        s.pop_back();
    }
    

    void abs  ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::fabs );  }
    void acos ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::acos );  }
    void asin ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::asin );  }
    void atan ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::atan );  }
    void ceil ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::ceil );  }
    void cos  ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::cos );   }
    void exp  ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::exp );   }
    void floor( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::floor ); }
    void fmod ( Bang::Stack& s, const Bang::RunContext& ) { infix2to1( s, &::fmod );      }
    void log  ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::log );   }
    void pow  ( Bang::Stack& s, const Bang::RunContext& ) { infix2to1( s, &::pow );       }
    void sin  ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::sin );   }
    void sqrt ( Bang::Stack& s, const Bang::RunContext& ) { math1numXform( s, &::sqrt );  }

    void lookup( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Bang::Value& v = s.pop();
        if (!v.isstr())
            throw std::runtime_error("Math library . operator expects string");
        const auto& str = v.tostr();

        const Bang::tfn_primitive p =
            (  str == "abs"    ? &abs
            :  str == "acos"   ? &acos
            :  str == "asin"   ? &asin
            :  str == "atan"   ? &atan
            :  str == "ceil"   ? &ceil
            :  str == "cos"    ? &cos
            :  str == "exp"    ? &exp
            :  str == "floor"  ? &floor
            :  str == "log"    ? &log
            :  str == "pow"    ? &pow
            :  str == "random" ? &random
            :  str == "sin"    ? &sin
            :  str == "sqrt"   ? &sqrt
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("Math library does not implement" + str);
    }
    
} // end namespace Math


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &Math::lookup );
}

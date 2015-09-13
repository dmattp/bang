#include "bang.h"

#include <string>
#include <iterator>
#include <math.h>
#include <stdlib.h>
#include "arraylib.h"

namespace Bang
{
    namespace ArrayNs
    {
        void stackToArray( Stack& s, const RunContext& rc )
        {
            auto toArrayFun = NEW_BANGFUN(Array, s );
            toArrayFun->appendStack(s);
            s.push( STATIC_CAST_TO_BANGFUN(toArrayFun) );
        }

        void stackNew( Stack& s, const RunContext& rc )
        {
            const auto& toArrayFun = NEW_BANGFUN(Array, s );
            s.push( STATIC_CAST_TO_BANGFUN(toArrayFun) );
        }
    

        void lookup( Bang::Stack& s, const Bang::RunContext& ctx)
        {
            const Bang::Value& v = s.pop();
            if (!v.isstr())
                throw std::runtime_error("Array library . operator expects string");
            const auto& str = v.tostr();
        
            const Bang::tfn_primitive p =
                (  str == "from-stack" ? &stackToArray
                :  str == "new"        ? &stackNew // test
                :  nullptr
                );

            if (p)
                s.push( p );
            else
                throw std::runtime_error("Array library does not implement: " + std::string(str));
        }
    }
    
} // end namespace Arraylib


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_arraylib_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &Bang::ArrayNs::lookup );
}

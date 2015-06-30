#include "bang.h"

#include <string>
#include <vector>
#include <iterator>
#include <math.h>
#include <stdlib.h>

#define HAVE_ARRAY_MUTATION 1

namespace Array
{
    using namespace Bang;
    
    class FunctionRestoreStack : public Function
    {
        std::vector<Value> stack_;
    public:
        FunctionRestoreStack( Stack& s )
        //    : pStack_( std::make_shared<std::vector<Value>>() )
        {
            s.giveTo( stack_ );
        }
        FunctionRestoreStack( const std::vector<Value>& s )
        : stack_(s)
        {
        }
        virtual void apply( Stack& s ) // , CLOSURE_CREF running )
        {
            std::copy( stack_.begin(), stack_.end(), s.back_inserter() );
        }
    };
    
    class FunctionStackToArray : public Function
    {
        std::vector<Value> stack_;
    public:
        FunctionStackToArray( Stack& s )
        {
        }

        void appendStack( Stack& s )
        {
            s.giveTo( stack_ );
        }

        //~~~ i still think, sort of, that the shared_ptr here should be to myself
        // rather than the nearest running closure.  Because if I already exist as
        // a shared_ptr, how do I find that?  It's sort of a weakness of the STL
        // because if I'm invoked through a shared_ptr, I never have access to the
        // shared reference counted, but I sort of need that, e.g., to "Clone()" or
        // to do something that takes another reference to me. 
        virtual void apply( Stack& s ) // , CLOSURE_CREF running )
        {
            const Value& msg = s.pop();
            if (msg.isnum())
            {
                s.push( stack_[int(msg.tonum())] );
            }
            else if (msg.isstr())
            {
                const auto& str = msg.tostr();
                
                if (str == "#")
                    s.push( double(stack_.size()) );
#if HAVE_ARRAY_MUTATION
                // I'm a little more willing to accept mutating arrays (vs upvals) just
                // because I don't know why.  The stack-to-array still feels less like part of the core
                // language I guess, and still more of a library.  Maybe I should just move it into
                // a library.  Why not?  It can be moved to a library after all.
                else if (str == "set")
                {
                    int ndx = int(s.pop().tonum());
                    stack_[ndx] = s.pop();
                }
                else if (str == "swap")
                {
                    int ndx1 = int(s.pop().tonum());
                    int ndx2 = int(s.pop().tonum());
                    std::swap( stack_[ndx1], stack_[ndx2] );
//                stack_[ndx] = s.pop();
                }
                else if (str == "insert")
                {
                    int ndx = int(s.pop().tonum());
                    stack_.insert( stack_.begin()+ndx, s.pop() );
                }
                else if (str == "erase")
                {
                    int ndx = int(s.pop().tonum());
                    stack_.erase( stack_.begin()+ndx );
                }
                else if (str == "append")
                {
                    this->appendStack(s);
                }
#endif 
                else if (str == "push")
                {
                    auto pushit = NEW_BANGFUN(FunctionRestoreStack)( stack_ );
                    s.push( STATIC_CAST_TO_BANGFUN(pushit) );
                }
            }
        }
    };

    

    void stackToArray( Stack& s, const RunContext& rc )
    {
        const auto& toArrayFun = NEW_BANGFUN(FunctionStackToArray)( s );
        toArrayFun->appendStack(s);
        s.push( STATIC_CAST_TO_BANGFUN(toArrayFun) );
    }

    void stackNew( Stack& s, const RunContext& rc )
    {
        const auto& toArrayFun = NEW_BANGFUN(FunctionStackToArray)( s );
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
            throw std::runtime_error("Array library does not implement: " + str);
    }
    
} // end namespace Math


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &Array::lookup );
}

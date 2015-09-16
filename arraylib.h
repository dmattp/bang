
#include "bang.h"
#include <vector>

namespace Bang
{
    
    class Array : public Function
    {
        std::vector<Value> stack_;
    public:
        Array( Stack& s )
        {
        }
        
        Array()
        {
        }

        void push_back( const Value& v ) { stack_.push_back( v ); }

        void appendStack( Stack& s )
        {
            s.giveTo( stack_ );
        }

        virtual void indexOperator( const Bang::Value& msg, Stack& stack, const RunContext& )
        {
            if (msg.isnum())
            {
                stack.push( stack_[int(msg.tonum())] );
            }
        }
        virtual void apply( Stack& s ) // , CLOSURE_CREF running )
        {
            const Value& msg = s.pop();
            RunContext* pctx(nullptr);
            this->indexOperator( msg, s, *pctx );
        }
        
        void customOperator( const bangstring& str, Stack& s)
        {
            const static Bang::bangstring opSize("/#");
            const static Bang::bangstring opToStack("/to-stack");
            const static Bang::bangstring opSet("/set");
            const static Bang::bangstring opSwap("/swap");
            const static Bang::bangstring opInsert("/insert");
            const static Bang::bangstring opErase("/erase");
            const static Bang::bangstring opAppend("/append");
            const static Bang::bangstring opPush("/push");
            const static Bang::bangstring opDequeue("/dequeue");
        
            if (str == opSize)
                s.push( double(stack_.size()) );
            else if (str == opToStack)
            {
                std::copy( stack_.begin(), stack_.end(), s.back_inserter() );
            }
            // I'm a little more willing to accept mutating arrays (vs upvals) just
            // because I don't know why.  Because arraylib is currently a library, and libraries
            // are even more tentative and easier to replace than the core language, does not
            // imply new syntax, and can always be retained as a deprecated library alongside
            // mutation free alternatives or something.
            else if (str == opSet)
            {
                int ndx = int(s.pop().tonum());
                stack_[ndx] = s.pop();
            }
            else if (str == opSwap)
            {
                int ndx1 = int(s.pop().tonum());
                int ndx2 = int(s.pop().tonum());
                std::swap( stack_[ndx1], stack_[ndx2] );
//                stack_[ndx] = s.pop();
            }
            else if (str == opInsert)
            {
                int ndx = int(s.pop().tonum());
                stack_.insert( stack_.begin()+ndx, s.pop() );
            }
            else if (str == opErase)
            {
                int ndx = int(s.pop().tonum());
                stack_.erase( stack_.begin()+ndx );
            }
            else if (str == opAppend)
            {
                this->appendStack(s);
            }
            else if (str == opPush)
            {
                stack_.push_back( s.pop() );
            }
            else if (str == opDequeue)
            {
                s.push( stack_.front() );
                stack_.erase( stack_.begin() );
            }
        }
    }; // end, Array class

} // end, Bang namespace


extern "C"
DLLEXPORT
void bang_arraylib_open( Bang::Stack* stack, const Bang::RunContext* );

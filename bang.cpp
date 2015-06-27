/////////////////////////////////////////////////////////////////
// Bang! language interpreter
// (C) 2015 David M. Placek
// All rights reserved, see accompanying [license.txt] file
//////////////////////////////////////////////////////////////////

#define PBIND(...) do {} while (0) // debugging blocks
#define HAVE_MUTATION 0  // boo
#define HAVE_ARRAY_MUTATION 1

#if HAVE_MUTATION
# if __GNUC__
# warning Mutation enabled: You have strayed from the true and blessed path.
#endif 
#endif

/*
  Keywords
    fun fun! as def
    
So really there are only four keywords, all of which are
syntactic variants for creating a function/closure.

Well, with the "module/object" system, there is now a second:

    lookup -- (does not require apply!!!)
  
And there are some built-in operators:

  Operators
     ! -- apply
     ? -- conditional apply

And syntactic sugar

     . -- Object message

And there are primitives, until a better library / module system is in place.
     
  Primitives
    + - * / < > =   -- the usual suspects for numbers and booleans
    #               -- get stack length
    not! and! or! 
    drop! swap! dup! nth! save-stack!
    floor! random! -- these really belong in a math library
    
  Literals
    true false
*/

#include <iostream>
#include <list>
#include <string>
#include <algorithm>
#include <iterator>
#include <functional>

#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sstream>

#if defined(_WIN32)
# include <windows.h> // for load library
# if !__GNUC__
#  include <codecvt> // to convert to wstring
# endif
#else
# include <dlfcn.h>
#endif


const char* const BANG_VERSION = "0.004";

// #define kDefaultScript "c:/m/n2proj/bang/tmp/coro1.bang";
// #define kDefaultScript "c:/m/n2proj/bang/test/prog-01-quicksort.bang";

//~~~temporary #define for refactoring
#define TMPFACT_PROG_TO_RUNPROG(p) &((p)->getAst()->front())

#include "bang.h"

namespace {
    bool gReplMode(false);
    bool gDumpMode(false);
}

namespace Bang {


    static const NthParent kNoParent=NthParent(INT_MAX);


    namespace Ast { class CloseValue; }
    class Upvalue
    {
    private:
        const Ast::CloseValue* closer_; // contains the symbolic name to which the upvalue is bound
        SHAREDUPVALUE parent_;  // the upvalue chain
        Value v_; // the value itself
    public:
        Upvalue( const Ast::CloseValue* closer, SHAREDUPVALUE_CREF parent, const Value& v )
        : closer_( closer ), parent_( parent ), v_( v )
        {
        }

        const Ast::CloseValue* upvalParseChain() { return closer_; } // needed for REPL/EofMarker::run()

        const Value& getUpValue( const NthParent uvnumber )
        {
            return (uvnumber == NthParent(0)) ? v_ : parent_->getUpValue( --uvnumber );
        }

        // lookup by string / name is expensive; used for experimental object
        // system.  obviously if it's "a hit" you can optimize out much of this cost
        const Value& getUpValue( const std::string& uvName );

        SHAREDUPVALUE_CREF nthParent( const NthParent uvnumber )
        {
            return (uvnumber == NthParent(1)) ? parent_ : parent_->nthParent( --uvnumber );
        }
    };

    


namespace {
    template<class T>
    unsigned PtrToHash( const T& ptr )
    {
        return (reinterpret_cast<unsigned>(ptr)&0xFFF0) >> 4;
    }
}

    void Value::tostring( std::ostream& o ) const
    {
        if (type_ == kNum)
            o << v_.num;
        else if (type_ == kBool)
            o << (v_.b ? "true" : "false");
        else if (type_ == kStr)
            o << this->tostr();
        else if (type_ == kFun)
            o << "<fun=" << this->tofun().get() << ">";
        else if (type_ == kFunPrimitive)
            o << "<prim.function>";
        else if (type_ == kThread)
            o << "<thread=" << this->tothread().get() << ">";
        else
            o << "<?Value?>";
    }


    void Value::dump( std::ostream& o ) const
    {
        this->tostring(o);
    }

    void Stack::dump( std::ostream& o ) const
    {
        std::for_each
        (   stack_.begin(), stack_.end(),
            [&]( const Value& v ) { v.dump( o ); o << "\n"; }
        );
    }
    


void indentlevel( int level, std::ostream& o )
{
    if (level > 0)
    {
        o << "  ";
        indentlevel( level - 1, o );
    }
}

namespace Ast { class Base; }    

    RunContext::RunContext()
    : thread(nullptr), prev(nullptr), ppInstr(nullptr)
    {}
    
//     void rebind( const Ast::Program* inprog )
//     {
//         ppInstr = TMPFACT_PROG_TO_RUNPROG(inprog);
//     }

    void RunContext::rebind( const Ast::Base* const * inppInstr, SHAREDUPVALUE_CREF uv )
    {
        ppInstr = inppInstr;
        upvalues_ = uv;
    }
    
    void RunContext::rebind( const Ast::Base* const * inppInstr )
    {
        ppInstr = inppInstr;
    }

namespace Primitives
{
    template <class T>
    void infix2to1( Stack& s, T operation )
    {
        const Value& v2 = s.loc_top();
        Value& v1 = s.loc_topMinus1Mutate();

//        if (v1.isnum() && v2.isnum())
        {
            v1.mutateNoType( operation( v1.tonum(), v2.tonum() ) );
            s.pop_back();
        }
//         else
//             throw std::runtime_error("Binary operator incompatible types");
    }

    template <class T>
    void infix2to1bool( Stack& s, T operation )
    {
        const Value& v2 = s.loc_top();
        Value& v1 = s.loc_topMinus1Mutate();

//        if (v1.isnum() && v2.isnum())
        {
            v1.mutatePrimitiveToBool( operation( v1.tonum(), v2.tonum() ) );
            s.pop_back();
        }
//         else
//             throw std::runtime_error("Binary operator.b incompatible types");
    }
    
    void plus ( Stack& s, const RunContext& )
    {
//        if (s.loc_top().isnum())
        if (s.loc_top().isstr())
        {
            const auto& v2 = s.pop();
            const auto& v1 = s.pop();
            s.push( v1.tostr() + v2.tostr() );
        }
        else
            infix2to1( s, [](double v1, double v2) { return v1 + v2; } );
    }
    void minus( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 - v2; } ); }
    void mult ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 * v2; } ); }
    void div  ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 / v2; } ); }
    void lt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 < v2; } ); }
    void gt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 > v2; } ); }
    void modulo ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return double(int(v1) % int(v2)); } ); }
    void eq( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 == v2; } ); }

    void increment( Stack& s, const RunContext& ) { Value& top = s.loc_topMutate(); top.mutateNoType( top.tonum() + 1 ); }
    void decrement( Stack& s, const RunContext& ) { Value& top = s.loc_topMutate(); top.mutateNoType( top.tonum() - 1 ); }
    
    void stacklen( Stack& s, const RunContext& ctx)
    {
        s.push(double(s.size()));
    }

    void swap( Stack& s, const RunContext& ctx)
    {
        const Value& v1 = s.pop();
        const Value& v2 = s.pop();
        s.push( v1 );
        s.push( v2 );
    }

    void drop( Stack& s, const RunContext& ctx)
    {
        s.pop_back();
    }

    void nth( Stack& s, const RunContext& ctx)
    {
        const Value& ndx = s.pop();
        if (ndx.isnum())
        {
            const Value& found = s.nth(int(ndx.tonum()));
            s.push( found );
        }
    }

    void dup( Stack& s, const RunContext& ctx)
    {
        s.push( s.loc_top() );
    }
    
    
    void lognot( Stack& s, const RunContext& ctx)
    {
        Value& v = s.loc_topMutate();
        if (v.isbool())
            v.mutateNoType( !v.tobool() );
        else
            throw std::runtime_error("Logical NOT operator incompatible type");
    }

    void logand( Stack& s, const RunContext& ctx)
    {
        const Value& v2 = s.pop();
        const Value& v1 = s.pop();
        if (v1.isbool() && v2.isbool())
            s.push( v1.tobool() && v2.tobool() );
        else
            throw std::runtime_error("Logical AND operator incompatible type");
    }
    void logor( Stack& s, const RunContext& ctx)
    {
        const Value& v2 = s.pop();
        const Value& v1 = s.pop();
        if (v1.isbool() && v2.isbool())
            s.push( v1.tobool() || v2.tobool() );
        else
            throw std::runtime_error("Logical AND operator incompatible type");
    }

    void tostring( Stack& s, const RunContext& ctx )
    {
        const Value& v1 = s.pop();
        std::ostringstream os;
        v1.tostring( os );
        s.push( os.str() );
    }
    
    void printone( Stack& s, const RunContext& ctx )
    {
        const Value& v1 = s.pop();
        //std::cout << "V=";
        v1.dump( std::cout );
        std::cout << std::endl;
    }

    void print( Stack& s, const RunContext& ctx )
    {
        const Value& vfmt = s.pop();
        const auto& fmt = vfmt.tostr();
        const int fmtlen = fmt.size();

        std::function<void(size_t)> subpar;

        subpar = [&]( size_t pos ) {
            using namespace std;
            auto found_x = fmt.rfind( "%@", pos );
            auto found_s = fmt.rfind( "%s", pos );
            auto found =
            (  found_x != string::npos ?
                (   found_s != string::npos
                ?   max( found_x, found_s )
                :   found_x
                )
            :   found_s
            );
            if (found == string::npos)
            {
                if (pos == string::npos)
                    std::cout << fmt;
                else    
                    std::cout << fmt.substr( 0, pos + 1 );
            }
            else
            {
                const Value& v = s.pop();
                if (found > 0)
                    subpar( found - 1 );
                v.tostring(std::cout); //  << v.tostring(); // "XXX";
                size_t afterParam = found + 2;
                if (pos==std::string::npos)
                    std::cout << fmt.substr( afterParam ); //
                else
                    std::cout << fmt.substr( afterParam, pos - afterParam + 1 );
            }
        };

        subpar( std::string::npos );
        
        //std::cout << std::endl;
    }
    
    void beginStackBound( Stack& s, const RunContext& ctx )
    {
        s.beginBound();
    }
    void endStackBound( Stack& s, const RunContext& ctx )
    {
        s.endBound();
    }

} // end, namespace Primitives


static tfn_primitive bangprimforchar( int c )
{
    return
    (  c == '#' ? Primitives::stacklen

    // I would prefer to see these treated in a way that user defined
    // operators and these guys are "on the same footing", so to speak,
    // ie, so that this is all in a library or something, or they are
    // defined/imported the same way a user-defined operator would be,
    // e.g., they just push functions.  Or something like that.
    // 'stdoperators' require! import!
    // I mean, I guess pushing them directly in the Rst will be "faster" than
    // doing a lookup and pushing an upvalue, so that's something - especially
    // for logical / comparison operators, but I'd still like to see them not be a special case
    :  c == '+' ? Primitives::plus
    :  c == '-' ? Primitives::minus
    :  c == '<' ? Primitives::lt
    :  c == '>' ? Primitives::gt
    :  c == '=' ? Primitives::eq
    :  c == '*' ? Primitives::mult
    :  c == '/' ? Primitives::div
    :  c == '\\' ? Primitives::drop
    :  c == '%' ? Primitives::modulo
    :  c == '(' ? Primitives::beginStackBound
    :  c == ')' ? Primitives::endStackBound

    ///////////////////////////////// toys+test        
    :  c == '@' ? Primitives::printone
    /////////////////////////////////
        
    :  nullptr
    );
}



namespace Ast
{
    class Base
    {
    public:
        virtual void dump( int, std::ostream& o ) const = 0;
        virtual void run( Stack& stack, const RunContext& ) const
        {
            throw std::runtime_error("Ast::Base::run should never be called");
        }
        enum EAstInstr {
            kUnk,
            kBreakProg,
            kCloseValue,
            kApply,
            kApplyUpval,
            kApplyProgram,
            kApplyFunRec,
            kIfElse,
            kTCOApply,
            kTCOApplyUpval,
            kTCOApplyProgram,
            kTCOApplyFunRec,
            kTCOIfElse,
            kMakeCoroutine,
            kYieldCoroutine,
            kEofMarker
        };
        Base() : instr_(kUnk) {}
        Base( EAstInstr i ) : instr_( i ) {}
        bool isTailable() const { return instr_ != kUnk && instr_ != kBreakProg; } //  && instr_ != kApplyFun; }
            // return instr_ == kApply || instr_ == kConditionalApply || instr_ == kApplyUpval; }

        EAstInstr instr_;

        void convertToTailCall()
        {
            switch (instr_)
            {
                case kApplyFunRec:  instr_ = kTCOApplyFunRec;  break;
                case kIfElse:       instr_ = kTCOIfElse;       break;
                case kApplyProgram: instr_ = kTCOApplyProgram; break;
                case kApplyUpval:   instr_ = kTCOApplyUpval;   break;
                case kApply:        instr_ = kTCOApply;        break;
            }
        }
    private:
    };

    class CloseValue : public Base
    {
        const CloseValue* pUpvalParent_;
        std::string paramName_;
    public:
        CloseValue( const CloseValue* parent, const std::string& name )
        : Base( kCloseValue ),
          pUpvalParent_( parent ),
          paramName_( name )
        {}
        const std::string& valueName() const { return paramName_; }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "CloseValue(" << paramName_ << ") " << std::hex << PtrToHash(this) << std::dec << "\n";
        }
        
        const NthParent FindBinding( const std::string& varName ) const
        {
            if (this->paramName_ == varName)
            {
                return NthParent(0);
            }
            else
            {
                if (!pUpvalParent_)
                    return kNoParent;

                auto fbp = pUpvalParent_->FindBinding(varName);

                return (fbp == kNoParent ) ? fbp : ++fbp;
            }
        }

        const NthParent FindBinding( const CloseValue* target ) const
        {
            if (this == target)
            {
                return NthParent(0);
            }
            else
            {
                if (!pUpvalParent_)
                    return kNoParent;

                auto fbp = pUpvalParent_->FindBinding(target);

                return (fbp == kNoParent ) ? fbp : ++fbp;
            }
        }
        
    };
}


    const Value& Upvalue::getUpValue( const std::string& uvName )
    {
        if (uvName == closer_->valueName())
        {
            return v_;
        }
        else
        {
            if (parent_)
                return parent_->getUpValue( uvName );
            else
            {
                std::cerr << "Error Looking for dynamic upval=\"" << uvName << "\"\n";
                throw std::runtime_error("Could not find dynamic upval");
            }
        }
    }
    

const Ast::Base* gFailedAst = nullptr;
struct AstExecFail {
    const Ast::Base* pStep;
    std::runtime_error e;
    AstExecFail( const Ast::Base* step, const std::runtime_error& e )
    : pStep(step), e(e)
    {}
};

struct ParseFail : public std::runtime_error
{
    std::string mywhat( const std::string& where, const std::string& emsg ) const
    {
        std::ostringstream oss;
        oss << " in " << where << ": " << emsg;
        return oss.str();
    }
    ParseFail( const std::string& where, const std::string& emsg )
    : std::runtime_error( mywhat( where, emsg ) )
    {}
};
    

namespace Ast
{
    class BreakProg : public Base
    {
    public:
        BreakProg() : Base( kBreakProg ) {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "---\n";
        }
    };
    class EofMarker : public Base
    {
    public:
        EofMarker()
        : Base( kEofMarker )
        {}
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "<<< EOF >>>\n";
        }
    };

    class PushLiteral : public Base
    {
    public:
        Value v_;
    public:
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushLiteral v=";
            v_.dump( o );
            o << "\n";
        }
        
        PushLiteral( const Value& v )
        : v_(v)
        {}

        virtual void run( Stack& stack, const RunContext& ) const
        {
            stack.push( v_ );
        }
    };

    class PushPrimitive: public Base
    {
//        friend void OptimizeAst( std::vector<Ast::Base*>& ast );

    public:
        tfn_primitive primitive_;
        std::string desc_;
    public:
        PushPrimitive( tfn_primitive primitive, const std::string& desc )
        : primitive_( primitive ),
          desc_( desc )
        {
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushPrimitive op='" << desc_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class ApplyPrimitive: public PushPrimitive
    {
//        friend void OptimizeAst( std::vector<Ast::Base*>& ast );
    public:
        ApplyPrimitive( const PushPrimitive* pp )
        : PushPrimitive( *pp )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            if (gFailedAst == this)
                o << "FAIL *** ";
//            o << this << " ";
            indentlevel(level, o);
            o << "ApplyPrimitive op='" << desc_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class MakeCoroutine : public Base
    {
    public:
        MakeCoroutine()
        : Base(kMakeCoroutine)
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "MakeCoroutine\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const
        {}

        static void go( Stack& stack, Thread* incaller );
    };

    class YieldCoroutine : public Base
    {
    public:
        YieldCoroutine()
        : Base( kYieldCoroutine )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "YieldCoroutine\n";
        }
    };
    
    class PushUpval : public Base
    {
    protected:
        std::string name_;
        NthParent uvnumber_; // index into the active Upvalues
    public:
        PushUpval( const std::string& name, NthParent uvnumber )
        : name_( name ),
          uvnumber_( uvnumber )
        {
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushUpval #" << uvnumber_.toint() << " name='" << name_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

#if HAVE_MUTATION    
    class SetUpval : public Base
    {
    protected:
        std::string name_;
        NthParent uvnumber_; // index into the active Upvalues
    public:
        SetUpval( const std::string& name, NthParent uvnumber )
        : name_( name ),
          uvnumber_( uvnumber )
        {
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "SetUpval #" << uvnumber_.toint() << " name='" << name_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };
#endif 
    
    class ApplyUpval : public PushUpval
    {
    public:
        ApplyUpval( const PushUpval* puv )
        :  PushUpval( *puv )
        {
            instr_ = kApplyUpval;
        }
          
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyUpval #" << uvnumber_.toint() << " name='" << name_ << "'\n";
        }

        const Value& getUpValue( const RunContext& rc ) const
        {
            return rc.getUpValue( uvnumber_ );
        }
    };

    class PushUpvalByName : public Base
    {
    public:
        PushUpvalByName() {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushUpvalByName\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class Require : public Base
    {
    public:
        Require() {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Require\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const;
    };

    
    class Apply : public Base
    {
    public:
        Apply() : Base( kApply ) {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Apply";
            if (gFailedAst == this)
                o << " ***                                 <== *** FAILED *** ";
            o << std::endl;
        }
    };


    class Program : public Base
    {
    public:
        typedef std::vector<Ast::Base*> astList_t;
    protected:
        const Program* pParent_;
        astList_t ast_;

    public:
        Program( const Program* parent, const astList_t& ast )
        : pParent_(parent), ast_( ast )
        {}

        Program( const Program* parent )  // empty program ~~~ who uses this? hmm
        : pParent_( parent )
        {}

        void setAst( const astList_t& newast )
        {
            ast_ = newast;
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec << " Program\n";
            std::for_each
            (   ast_.begin(), ast_.end(),
                [&]( const Ast::Base* ast ) { ast->dump( level+1, o ); }
            );
        }

        const astList_t* getAst() const { return &ast_; }

        // 'run' pushes the program onto the stack as a BoundProgram.
        // 
        void run( Stack& stack, const RunContext& ) const;
    }; // end, class Ast::Program

    // optimization, immediately run program rather than executing
    // push+apply
    class ApplyProgram : public Program
    {
    public:
        ApplyProgram( const Program* pf )
        : Program( *pf )
        {
            instr_ = kApplyProgram;
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec << " ApplyProgram\n";
            std::for_each
            (   ast_.begin(), ast_.end(),
                [&]( const Ast::Base* ast ) { ast->dump( level+1, o ); }
            );
        }
    };

    class IfElse : public Base
    {
        Ast::Program* if_;
        Ast::Program* else_;
    public:
        IfElse( Ast::Program* if__, Ast::Program* else__ )
        : Base( kIfElse ),
          if_(if__),
          else_( else__ )
        {}

        Ast::Program* branchTaken( Stack& stack ) const
        {
            bool isCond = stack.loc_top().tobool();
            stack.pop_back();
            return isCond ? if_ : else_;
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "If";
            if_->dump( level, o );
            if (else_)
            {
                indentlevel(level, o);
                o << "Else";
                else_->dump( level, o );
            }
        }
    }; // end class IfElse


    class PushFunctionRec : public Base
    {
    protected:
    public:
        Ast::Program* pRecFun_;
        NthParent nthparent_;
        PushFunctionRec( Ast::Program* other, NthParent boundAt )
        : pRecFun_( other ),
          nthparent_(boundAt)
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushFunctionRec[" << std::hex << PtrToHash(pRecFun_) << std::dec << "/" << nthparent_ .toint() << "]" << std::endl;
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class ApplyFunctionRec : public PushFunctionRec
    {
    public:
        ApplyFunctionRec( const Ast::PushFunctionRec* pfr )
        : PushFunctionRec( *pfr )
        {
            instr_ = kApplyFunRec;
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyFunctionRec:" << instr_ << "[" << std::hex << PtrToHash(pRecFun_) << std::dec << "/" << nthparent_ .toint() << "]" << std::endl;
        }
    };
    
} // end, namespace Ast



#if !USE_GC
template< class Tp >    
struct FcStack
{
    FcStack<Tp>* prev;
    static FcStack<Tp>* head;
};

template <class Tp>
FcStack<Tp>* FcStack<Tp>::head(nullptr);

//    static int gAllocated;

template <class Tp>
struct SimpleAllocator
{
    typedef Tp value_type;
    SimpleAllocator(/*ctor args*/) {}
    template <class T> SimpleAllocator(const SimpleAllocator<T>& other){}
    template <class U> struct rebind { typedef SimpleAllocator<U> other; };
    
    Tp* allocate(std::size_t n)
    {
        FcStack<Tp>* hdr;
        if (FcStack<Tp>::head)
        {
            hdr = FcStack<Tp>::head;
            FcStack<Tp>::head = hdr->prev;
        }
        else
        {
            hdr = static_cast<FcStack<Tp>*>( malloc( n * (sizeof(Tp) + sizeof(FcStack<Tp>)) ) );
//            ++gAllocated;
        }
        Tp* mem = reinterpret_cast<Tp*>( hdr + 1 );
//        std::cerr << "ALLOCATE=" << gAllocated << "\n"; //  FunctionClosure p=" << mem << " size=" << n << " sizeof<shptr>=" << sizeof(std::shared_ptr<FunctionClosure>) << " sizeof Tp=" << sizeof(Tp) << std::endl;
        return mem;
    }
    void deallocate(Tp* p, std::size_t n)
    {
        FcStack<Tp>* hdr = reinterpret_cast<FcStack<Tp>*>(p);
        --hdr;
        static char freeOne;
        if ((++freeOne & 0xF) == 0)
        {
//            std::cerr << "DEALLOCATE=" << gAllocated << "\n"; //  FunctionClosure p=" << p << " size=" << n << " sizeof<shptr>=" <<
//            sizeof(std::shared_ptr<FunctionClosure>) << " sizeof Tp=" << sizeof(Tp) << std::endl;
//            --gAllocated;
            free( hdr );
        }
        else
        {
            hdr->prev = FcStack<Tp>::head;
            FcStack<Tp>::head = hdr;
        }
    }
    void construct( Tp* p, const Tp& val ) { new (p) Tp(val); }
    void destroy(Tp* p) { p->~Tp(); }
};
template <class T, class U>
bool operator==(const SimpleAllocator<T>& a, const SimpleAllocator<U>& b)
{
    return &a == &b;
}
template <class T, class U>
bool operator!=(const SimpleAllocator<T>& a, const SimpleAllocator<U>& b)
{
    return &a != &b;
}

#endif

    
#if !USE_GC    
SimpleAllocator< std::shared_ptr<Upvalue> > gUpvalAlloc;
#define NEW_UPVAL(c,p,v) std::allocate_shared<Upvalue>( gUpvalAlloc, c, p, v )
#endif


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
        std::copy( stack_.begin(), stack_.end(), std::back_inserter( s.stack_ ) );
    }
};

class FunctionStackToArray : public Function
{
    std::vector<Value> stack_;
public:
    FunctionStackToArray( Stack& s )
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
#endif 
            else if (str == "push")
            {
                auto pushit = NEW_BANGFUN(FunctionRestoreStack)( stack_ );
                s.push( STATIC_CAST_TO_BANGFUN(pushit) );
            }
        }
    }
};


namespace Primitives {
    void savestack( Stack& s, const RunContext& rc )
    {
        const auto& restoreFunction = NEW_BANGFUN(FunctionRestoreStack)( s );
        s.push( STATIC_CAST_TO_BANGFUN(restoreFunction) );
    }
    void stackToArray( Stack& s, const RunContext& rc )
    {
        const auto& toArrayFun = NEW_BANGFUN(FunctionStackToArray)( s );
        s.push( STATIC_CAST_TO_BANGFUN(toArrayFun) );
    }
}


        BoundProgram::BoundProgram( const Ast::Program* program, SHAREDUPVALUE_CREF upvalues )
        : // Function(true),
          program_( program ), upvalues_( upvalues )
        {}
    
        void BoundProgram::dump( std::ostream & out )
        {
            program_->dump( 0, std::cerr );
        }
    
    void throwNoFunVal( const Value& v )
    {
        std::ostringstream oss;
        oss << "Called apply without function.2; found type=" << v.type_ << " V=";
        v.dump(oss);
        throw std::runtime_error(oss.str());
    }

SimpleAllocator<RunContext> gAllocRc;

class Thread
{
public:
    Bang::Stack stack;
    RunContext* callframe;
    Thread* pCaller;
    Thread()
    : callframe( nullptr ),
      pCaller( nullptr )
    {}
    Thread( Thread* incaller )
    : callframe( nullptr ),
      pCaller( incaller )
    {}
};

    RunContext::RunContext( Thread* inthread, const Ast::Base* const *inppInstr, SHAREDUPVALUE_CREF uv )
    :  thread(inthread),
       prev(inthread->callframe),
       ppInstr( inppInstr ), // , /*initialupvalues(uv),*/
       upvalues_( uv )
    {}
    
void RunApplyValue( const Value& v, Stack& stack, const RunContext& frame )
{
    switch (v.type())
    {
        case Value::kFun: v.tofun()->apply(stack); break;
        case Value::kFunPrimitive: v.tofunprim()( stack, frame ); break;
        default: throwNoFunVal(v);
    }
}
    
    Ast::Program* parseStdinToProgram( const Ast::CloseValue* closeValueChain );
void repl_prompt()
{
    std::cout << "Bang! " << std::flush;
}

void xferstack( Thread* from, Thread* to )
{
    from->stack.giveTo( to->stack );
}

static Thread gNullThread;
    
Thread* pNullThread( &gNullThread );
    
void RunProgram
(   
    Thread* pThread,
    const Ast::Program* inprog,
    SHAREDUPVALUE inupvalues
)
{
// ~~~todo: save initial upvalue, destroy when closing program?
restartNonTail:
    pThread->callframe =
        new (gAllocRc.allocate(sizeof(RunContext)))
        RunContext( pThread, TMPFACT_PROG_TO_RUNPROG(inprog), inupvalues );
restartThread:
    pThread->callframe->thread = pThread;
    Stack& stack = pThread->stack;
restartReturn:
    RunContext& frame = *(pThread->callframe);
restartTco:
    try
    {
        while (true)
        {
            const Ast::Base* pInstr = *(frame.ppInstr++);
            switch (pInstr->instr_)
            {
                default:
                    pInstr->run( stack, frame );
                    break;

                case Ast::Base::kBreakProg:
                {
                    RunContext* prev = frame.prev;
                    frame.~RunContext();
                    gAllocRc.deallocate( &frame, sizeof(RunContext) );
                    pThread->callframe = prev;
                    if (prev)
                        goto restartReturn;
                    else if (pThread->pCaller)
                    {
                        xferstack( pThread, pThread->pCaller );
                        pThread = pThread->pCaller;
                        goto restartThread;
                    }
                    else
                        return;
                }

                case Ast::Base::kEofMarker:
                {
                    stack.dump( std::cout );
                    repl_prompt();
    
                    try
                    {
                        SHAREDUPVALUE_CREF uv = frame.upvalues_;
                        Ast::Program *pProgram =
                            parseStdinToProgram( uv ? uv->upvalParseChain() : static_cast<const Ast::CloseValue*>(nullptr) );
                        frame.rebind( TMPFACT_PROG_TO_RUNPROG(pProgram) ); 
                        goto restartTco;
                    }
                    catch (const std::exception& e )
                    {
                        std::cerr << "Error: " << e.what() << std::endl;
                    }
                }
                break;

                case Ast::Base:: kMakeCoroutine:
                    Ast::MakeCoroutine::go( stack, pThread );
                    break;

                case Ast::Base::kYieldCoroutine:
                    if (pThread->pCaller)
                    {
                        xferstack( pThread, pThread->pCaller );
                        pThread = pThread->pCaller;
                        goto restartThread;
                    }
                    else
                        return; // nowhere to go

                case Ast::Base::kCloseValue:
                    frame.upvalues_ =
                        NEW_UPVAL
                        (   reinterpret_cast<const Ast::CloseValue*>(pInstr),
                            frame.upvalues_,
                            stack.pop()
                        );
                    break; // goto restartTco;

                case Ast::Base::kTCOApplyFunRec:
                {
                    // no dynamic cast, should be safe since we know the type from instr_
//                    std::cerr << "got kTCOApplyFunRec" << std::endl;
                    const Ast::ApplyFunctionRec* afn = reinterpret_cast<const Ast::ApplyFunctionRec*>(pInstr);
                    frame.rebind
                    (  TMPFACT_PROG_TO_RUNPROG(afn->pRecFun_),
                        (afn->nthparent_ == kNoParent) ? SHAREDUPVALUE() : frame.nthBindingParent( afn->nthparent_ )
                    );
                    goto restartTco;
                }
                break;

                case Ast::Base::kApplyFunRec:
                {
                    const Ast::ApplyFunctionRec* afn = reinterpret_cast<const Ast::ApplyFunctionRec*>(pInstr);
                    inprog = afn->pRecFun_;
                    inupvalues = (afn->nthparent_ == kNoParent) ? SHAREDUPVALUE() : frame.nthBindingParent( afn->nthparent_ );
                    goto restartNonTail;
                }
                break;
                
                case Ast::Base::kTCOIfElse:
                {
                    const Ast::IfElse* ifelse = reinterpret_cast<const Ast::IfElse*>(pInstr);
                    const Ast::Program* p = ifelse->branchTaken(stack);
                    if (p)
                    {
                        frame.rebind( TMPFACT_PROG_TO_RUNPROG(p) );
                        goto restartTco;
                    }
                }
                break;

                case Ast::Base::kIfElse:
                {
                    const Ast::IfElse* ifelse = reinterpret_cast<const Ast::IfElse*>(pInstr);
                    const Ast::Program* p = ifelse->branchTaken(stack);
                    if (p)
                    {
                        inprog = p;
                        inupvalues = frame.upvalues_;
                        goto restartNonTail;
                    }
                }
                break;
                
                case Ast::Base::kTCOApplyProgram:
                {
                    //std::cerr << "got kTCOApplyProgram\n";
                    const Ast::ApplyProgram* afn = reinterpret_cast<const Ast::ApplyProgram*>(pInstr); 
                    frame.rebind( TMPFACT_PROG_TO_RUNPROG(afn) );
                    goto restartTco;
                }
                break;

                case Ast::Base::kApplyProgram:
                {
                    //std::cerr << "got kTCOApplyProgram\n";
                    const Ast::ApplyProgram* afn = reinterpret_cast<const Ast::ApplyProgram*>(pInstr);
                    inprog = afn;
                    inupvalues = frame.upvalues_;
                    goto restartNonTail;
                }
                break;
                
                case Ast::Base::kTCOApplyUpval:
                {
                    const Value& v = reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( frame );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                            goto restartTco;
                
                    }
                }
                break;
                
                case Ast::Base::kApplyUpval:
                {
                    const Value& v = reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( frame );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            inprog = pbound->program_;
                            inupvalues = pbound->upvalues_;
                            goto restartNonTail;
                    }
                }
                break;
                
                case Ast::Base::kTCOApply:
                {
                    const Value& v = stack.pop();
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                            goto restartTco;
                    }
                }
                break;

                case Ast::Base::kApply:
                {
                    const Value& v = stack.pop();
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            inprog = pbound->program_;
                            inupvalues = pbound->upvalues_;
                            goto restartNonTail;
                    }
                }
                break;
            } // end,instr switch
        } // end, while loop incrementing PC

    }
    catch (const std::runtime_error& e)
    {
        throw AstExecFail( *(frame.ppInstr), e );
    }
}

    void BoundProgram::apply( Stack& s )
    {
        // this is handled in main RunProgram machine
        throw std::runtime_error("BoundProgram::Apply should not be called");
    }

    
    void Ast::Program::run( Stack& stack, const RunContext& rc ) const
    {
        stack.push( NEW_BANGFUN(BoundProgram)( this, rc.upvalues() ) );
    }


SHAREDUPVALUE_CREF RunContext::upvalues() const
{
    return upvalues_;
}

SHAREDUPVALUE_CREF RunContext::nthBindingParent( const NthParent n ) const
{
    return n == NthParent(0) ? upvalues_ : upvalues_->nthParent( n );
}
    
const Value& RunContext::getUpValue( NthParent uvnumber ) const
{
    return upvalues_->getUpValue( uvnumber );
}

const Value& RunContext::getUpValue( const std::string& uvName ) const
{
    return upvalues_->getUpValue( uvName );
}

void Ast::PushFunctionRec::run( Stack& stack, const RunContext& rc ) const
{
    SHAREDUPVALUE uv =
        (this->nthparent_ == kNoParent) ? SHAREDUPVALUE() : rc.nthBindingParent( this->nthparent_ );
    stack.push( NEW_BANGFUN(BoundProgram)( pRecFun_, uv ) ); //STATIC_CAST_TO_BANGFUN(newfun) );
}

void Ast::PushPrimitive::run( Stack& stack, const RunContext& rc ) const
{
    stack.push( primitive_ );
};

void Ast::ApplyPrimitive::run( Stack& stack, const RunContext& rc ) const
{
    primitive_( stack, rc );
};

void Ast::PushUpval::run( Stack& stack, const RunContext& rc) const
{
    stack.push( rc.getUpValue( uvnumber_ ) );
};

void Ast::MakeCoroutine::go( Stack& stack, Thread* incaller )
{
    // virtual void run( Stack& stack, const RunContext& ) const;
    const Value& v = stack.pop(); // function basis for coroutine
    if (!v.isboundfun())
        throw std::runtime_error("MakeCoroutine requires a bound function");
    
    BoundProgram* pbound = v.toboundfun(); 
    
    auto thread = NEW_BANGTHREAD( incaller );

    // start me off pointing at the BoundProgram, ready to roll
    thread->callframe =
        new (gAllocRc.allocate(sizeof(RunContext)))
        RunContext( thread.get(), TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
    
    stack.push( Value(thread) );
}
    

#if HAVE_MUTATION    
void Ast::SetUpval::run( Stack& stack, const RunContext& rc) const
{
    const Value& v = rc.getUpValue( uvnumber_ );
    const_cast<Value&>(v) = stack.pop(); // see the sort of horrible things mutation forces me to do
};
#endif 
    
// this nearly lets us create a sort of hash table / record / object system!
//
// This is really sort of 'experiment' code to make a quick&dirty object system /
// immutable record sort of thing
void Ast::PushUpvalByName::run( Stack& stack, const RunContext& rc) const
{
    const Value& vName = stack.pop();
    
    if (!vName.isstr())
        throw std::runtime_error("PushUpvalByName (lookup) did not find string!");
    
    stack.push( rc.getUpValue( vName.tostr() ) );
}



bool iseof(int c)
{
    return c == EOF;
}

struct ErrorEof
{
    ErrorEof() {}
};

struct ErrorNoMatch
{
    ErrorNoMatch() {}
};

class RegurgeStream
{
public:
    RegurgeStream() {}
    virtual char getc() = 0;
    virtual void accept() = 0;
    virtual void regurg( char ) = 0;
    virtual std::string sayWhere() { return "(unsure where)"; }
    virtual ~RegurgeStream() {}
};

class StreamMark : public RegurgeStream
{
    RegurgeStream& stream_;
    std::string consumned_;
    //StreamMark( const StreamMark&  );
    StreamMark& operator=( const StreamMark& );
public:
    StreamMark( RegurgeStream& stream )
    : stream_( stream )
    {}
    StreamMark( StreamMark& stream )
    : stream_( stream )
    {}

    virtual std::string sayWhere() {
        return stream_.sayWhere();
    }

    void dump( std::ostream& out, const std::string& context ) const
    {
        out << "  {{SM @ " << context << "}}: consumned=[[" << consumned_ << "\n";
    }

    char getc()
    {
        char rv = stream_.getc();
        consumned_.push_back(rv);
        return rv;
    }

    void accept()
    {
        consumned_.clear();
    }
    void regurg( char c )
    {
        char lastc = consumned_.back();
        if (lastc != c )
            throw std::runtime_error("parser regurged char != last");
        consumned_.pop_back();
        stream_.regurg(c);
    }
    ~StreamMark()
    {
//        std::cerr << "     ~SM: returning(" << consumned_.size() << ") regurg_.size(" << regurg_.size() << ")\n";
        // consumned stuff is pushed back in reverse
        std::for_each( consumned_.rbegin(), consumned_.rend(),
            [&]( char c ) { stream_.regurg(c); } );
    }
};

class RegurgeIo  : public RegurgeStream
{
    std::list<char> regurg_;
protected:
    int getcud()
    {
        char rv;
        
        if (regurg_.empty())
            return EOF;

        rv = regurg_.back();
        regurg_.pop_back();

        return rv;
    }
public:
    virtual void regurg( char c )
    {
        regurg_.push_back(c);
    }

    void accept() {}
};

class RegurgeStdinRepl : public RegurgeIo
{
    bool atEof_;
public:
    RegurgeStdinRepl()
    : atEof_(false)
    {}
    

    char getc()
    {
        int icud = RegurgeIo::getcud();

        if (icud != EOF)
            return icud;

        if (atEof_)
            throw ErrorEof();
        
        int istream = fgetc(stdin);
        if (istream == EOF || istream == 0x0d || istream == 0x0a) // CR
        {
            atEof_ = true;
            return 0x0a; // LF is close enough
        }
        else
        {
            return istream;
        }
    }
};


class RegurgeFile : public RegurgeIo
{
    std::string filename_;
    FILE* f_;
    struct Location {
        int lineno_;
        int linecol_;
        void advance( char c ) {
//            std::cerr << "ADV: " << c << "\n";
            if (c=='\n') {
                ++lineno_;
                linecol_ = 0;
            } else {
                ++linecol_;
            }
        }
        void rewind( char c ) {
//            std::cerr << "REW: " << c << "\n";
            if (c=='\n') {
                --lineno_;
                linecol_ = -1;
            } else {
                --linecol_;
            }

        }
        Location() : lineno_(1), linecol_(0) {}
    } loc_;
//    Location err_;
public:
    RegurgeFile( const std::string& filename )
    : filename_( filename )
    {
        f_ = fopen( filename.c_str(), "r");
    }
    ~RegurgeFile()
    {
        fclose(f_);
    }
    virtual std::string sayWhere()
    {
        std::ostringstream oss;
        oss << "FILE=" << filename_ << " L" << loc_.lineno_ << " C" << loc_.linecol_;
        return oss.str();
    }

    char getc()
    {
        int icud = RegurgeIo::getcud();

        if (icud != EOF)
        {
            loc_.advance( icud );
            return icud;
        }
        
        int istream = fgetc(f_);
        if (istream==EOF)
            throw ErrorEof();
        else
        {
            loc_.advance( istream );
            return istream;
        }
    }

    virtual void regurg( char c )
    {
        loc_.rewind(c);
        RegurgeIo::regurg(c);
    }
};
    



bool eatwhitespace( StreamMark& stream )
{
    bool bGotAny = false;
    StreamMark mark( stream );
    try
    {
        while (isspace(mark.getc()))
        {
            bGotAny = true;
            mark.accept();
        }
    }
    catch (const ErrorEof& )
    {
        bGotAny = true;
    }

    return bGotAny;
}


class Parser
{
    class Number
    {
        double value_;
    public:
        Number( StreamMark& stream )
        {
            StreamMark mark(stream);

            bool gotDecimal = false;
            bool gotOne = false;
            bool isNegative = false;
            double val = 0;
            double fractional_adj = 0.1;
            char c = mark.getc();

            if (c=='-')
            {
                c = mark.getc();
                if (!isdigit(c))
                    throw ErrorNoMatch();
                mark.accept();
                isNegative = true;
            }

            while (isdigit(c))
            {
                gotOne = true;
                mark.accept();
                if (gotDecimal)
                {
                    val = val + (fractional_adj * (c - '0'));
                    fractional_adj = fractional_adj / 10;
                }
                else    
                {
                    val = val * 10 + (c - '0');
                }
                c = mark.getc();
                if (!gotDecimal)
                {
                    if (c=='.')
                    {
                        gotDecimal = true;
                        c = mark.getc(); 
                        if (!isdigit(c)) // decimal point must be followed by a number
                            break;
                    }
                }
            }

            if (c == 'e' || c == 'E') // scientific notation
            {
                bool negativeExp = false;
                c = mark.getc();

                if (c == '+')
                    c = mark.getc();

                if (c == '-')
                {
                    c = mark.getc();
                    if (isdigit(c))
                    {
                        negativeExp = true;
                        mark.accept();
                    }
                }
                double exp = 0;
                while (isdigit(c))
                {
                    exp = exp * 10 + (c - '0');
                    mark.accept();
                    c = mark.getc();
                }
                if (negativeExp)
                    exp = exp * -1;
                
                val = val * ::pow(10,exp);
            }
            
            value_ = val;
            if (isNegative)
                value_ = value_ * -1.0;
            if (!gotOne)
                throw ErrorNoMatch();
        }
        double value() { return value_; }
    }; // end, class Number


    class Boolean
    {
        bool value_;
    public:
        Boolean( StreamMark& stream )
        {
            StreamMark mark(stream);

            if (eatReservedWord("true", mark))
                value_ = true;
            else if (eatReservedWord("false", mark))
                value_ = false;
            else
                throw ErrorNoMatch();

            mark.accept();
        }
        bool value() { return value_; }
    };
    

    struct ParsingRecursiveFunStack;
    class Program
    {
        Ast::Program::astList_t ast_;
    public:
        Program( StreamMark&, const Ast::Program* parent, const Ast::CloseValue* upvalueChain,
            ParsingRecursiveFunStack* pRecParsing );

        const Ast::Program::astList_t& ast() { return ast_; }
    };

    class Comment
    {
    public:
        Comment( StreamMark& stream )
        {
            StreamMark mark(stream);
            eatwhitespace(stream);
            if ( '-' == mark.getc() && '-' == mark.getc())
            {
                while (mark.getc() != '\n')
                    mark.accept();
            }
            else
                throw ErrorNoMatch();
        }
    };

    class ParseString
    {
        std::string content_;
    public:
        ParseString( StreamMark& stream )
        {
            StreamMark mark(stream);
            char delim = mark.getc();
            if (delim == '\'' || delim == '"') // simple strings, no escaping etc
            {
                auto bi = std::back_inserter(content_);
                for (char cstr = mark.getc(); cstr != delim; cstr = mark.getc())
                {
                    if (cstr == '\\')
                    {
                        char c = mark.getc();
                        switch (c) {
                            case 'n':  cstr = '\n'; break;
                            case '"':  cstr = '"';  break;
                            case '\'': cstr = '\''; break;
                            case '\\': cstr = '\\'; break;
                            case 'r':  cstr = '\r'; break;
                            default: cstr = '?'; break;
                        }
                    }
                    *bi++ = cstr;
                }
                mark.accept();
            }
            else
                throw ErrorNoMatch();
        }
        const std::string& content() { return content_; }
    };

    class ParseLiteral
    {
        Value value_;
    public:
        ParseLiteral( StreamMark& mark )
        {
            try
            {
                value_ = Value(ParseString(mark).content());
                return;
            } catch ( const ErrorNoMatch& ) {}

            try
            {
                value_ = Value(Number(mark).value());
                return;
            } catch ( const ErrorNoMatch& ) {}

            value_ = Value(Boolean(mark).value()); // allow to throw if not found
        }

        const Value& value() { return value_; }
    };

    class Identifier
    {
        std::string name_;
    public:
        Identifier( StreamMark& stream )
        {
            bool gotAny = false;
            StreamMark mark(stream);
            char c = mark.getc();
            while (isalpha(c) || c == '_')
            {
                name_.push_back(c);
                mark.accept();

                for (c = mark.getc(); isalpha(c) || isdigit(c) || (c=='_'); c = mark.getc())
                {
                    name_.push_back(c);
                    mark.accept();
                }

                if (c != '-') // hyphen allowed in identifier if followed by alpha
                    break;
                
                c = mark.getc();
                if (isalpha(c))
                    name_.push_back('-');
            }
            if (name_.size() < 1)
                throw ErrorNoMatch();
        }
        const std::string& name() { return name_; }
    };

    static bool eatReservedWord( const std::string& rw, StreamMark& stream )
    {
        StreamMark mark(stream);

        for (auto it = rw.begin(); it != rw.end(); ++it)
        {
            if (mark.getc() != *it)
                return false;
        }
        mark.accept();
        return true;
    }

    struct ParsingRecursiveFunStack
    {
        ParsingRecursiveFunStack* prev;
        Ast::Program *parentProgram;
        const Ast::CloseValue* parentsBinding;
        std::string progname_;
        std::pair<Ast::Program*,const Ast::CloseValue*> FindProgramForIdent( const std::string& ident )
        {
            if (progname_ == ident)
            {
//                std::cerr << "found parent prog, ident=" << ident << " CloseValue=" << parentsBinding << "\n";
                return std::pair<Ast::Program*,const Ast::CloseValue*>( parentProgram, parentsBinding );
            }
            else if (prev)
                return prev->FindProgramForIdent( ident );
            else
                return std::pair<Ast::Program*,const Ast::CloseValue*>( nullptr, nullptr );
        }
        
        ParsingRecursiveFunStack
        (   ParsingRecursiveFunStack* p,
            Ast::Program* parent,
            const Ast::CloseValue* binding,
            const std::string& progname
//            Ast::CloseValue* upvals
        )
        :  prev( p ),
           parentProgram(parent),
           parentsBinding( binding ),
           progname_( progname )
        {}
    };
    
    class Fundef
    {
        bool postApply_;
        Ast::Program* pNewProgram_;
    public:
        ~Fundef() { delete pNewProgram_; }
        
        Fundef( StreamMark& stream,
            const Ast::CloseValue* upvalueChain,
            // const Ast::PushFun* pParentFun,
            ParsingRecursiveFunStack* pRecParsing
        )
        :  postApply_(false),
           pNewProgram_(nullptr)
        {
            bool isAs = false;
            StreamMark mark(stream);

            eatwhitespace(mark);

            if (eatReservedWord( "fun", mark ))
                ;
            else
                throw ErrorNoMatch();

            if (isAs)
                postApply_ = true;
            else
            {
                char c = mark.getc();
                if (c == '!')
                    postApply_ = true;
                else
                    mark.regurg(c);
            }

            eatwhitespace(mark);

            // pNewFun_ =  new Ast::PushFun( pParentFun );

            Ast::Program::astList_t functionAst;
            try
            {
                Identifier param(mark);
                eatwhitespace(mark);
                Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, param.name() );
                upvalueChain = cv;
                functionAst.push_back( cv );
            }
            catch ( const ErrorNoMatch& ) // identifier is optional
            {
            }

            char c = mark.getc();
            if (c != '=') // this seems unneccesary sometimes. therefore "as"
            {
                std::cerr << "got '" << c << "', expecting '='\n";
                throw ParseFail( mark.sayWhere(), "function def must be followed by '='"); 
            }

            //~~~ programParent??
            Program program( mark, nullptr, upvalueChain, pRecParsing );
            const auto& subast = program.ast();
            std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );
            pNewProgram_ = new Ast::Program( nullptr, functionAst );

            mark.accept();
        }
        bool hasPostApply() const { return postApply_; }
        Ast::Program* stealProgram() { auto rc = pNewProgram_; pNewProgram_ = nullptr; return rc; }
    }; // end, class Fundef

    class Defdef
    {
        bool postApply_;
        Ast::Program* pDefProg_;
        std::unique_ptr<std::string> defname_;
    public:
        ~Defdef() { delete pDefProg_; } // delete pWithDefFun_; 
        Defdef( StreamMark& stream, const Ast::CloseValue* upvalueChain, ParsingRecursiveFunStack* pRecParsing )
        : postApply_(false), pDefProg_(nullptr)
        {
            const Ast::CloseValue* const lastParentUpvalue = upvalueChain;
            StreamMark mark(stream);

            eatwhitespace(mark);
            
            if (!eatReservedWord( "def", mark ))
                throw ErrorNoMatch();

            char c = mark.getc();
            if (c == '!')
                postApply_ = true;
            else
                mark.regurg(c);

            eatwhitespace(mark);

            c = mark.getc();
            if (c != ':')
            {
                std::cerr << "got '" << c << "' expecting ':'" << std::endl;
                throw ParseFail( mark.sayWhere(), "def name must start with ':'");
            }

            try
            {
                Identifier param(mark);
                eatwhitespace(mark);
                defname_ = std::unique_ptr<std::string>(new std::string(param.name()));
            }
            catch ( const ErrorNoMatch& )
            {
                throw ParseFail( mark.sayWhere(), "identifier must follow \"def :\"");
            }

            Ast::Program::astList_t functionAst;
            try
            {
                Identifier param(mark);
                eatwhitespace(mark);
                Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, param.name() );
                upvalueChain = cv;
                functionAst.push_back( cv );
            }
            catch ( const ErrorNoMatch& )
            {
            }

            if (mark.getc() != '=') // this seems unneccesary
            {
                std::cerr << "got '" << c << "', expecting '='\n";
                throw ParseFail( mark.sayWhere(), "function def must be followed by '='"); // throw ErrorNoMatch();
            }

            // std::string defFunName = pWithDefFun_->getParamName();
            pDefProg_ = new Ast::Program(nullptr);
            ParsingRecursiveFunStack recursiveStack( pRecParsing, pDefProg_, lastParentUpvalue, *defname_ ); // , pWithDefFun_);
            Program progdef( mark, nullptr, upvalueChain, &recursiveStack ); // pDefFun_, &recursiveStack );
            const auto& subast = progdef.ast();
            std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );
            pDefProg_->setAst( functionAst );
            mark.accept();
        }

        bool hasPostApply() const { return postApply_; }
        Ast::Program* stealDefProg() { auto rc = pDefProg_; pDefProg_ = nullptr; return rc; }
        const std::string& getDefName() { return *defname_; }
    }; // end, class Defdef
    
    Program* program_;
public:
    Parser( StreamMark& mark, const Ast::Program* parent )
    {
        program_ = new Program( mark, parent, nullptr, nullptr ); // 0,0,0 = no upvalues, no recursive chain
    }

    Parser( StreamMark& mark, const Ast::CloseValue* upvalchain )
    {
        program_ = new Program( mark, nullptr /*parent*/, upvalchain, nullptr ); // 0,0,0 = no upvalues, no recursive chain
    }
    
    const Ast::Program::astList_t& programAst()
    {
        return program_->ast();
    }
};

void OptimizeAst( std::vector<Ast::Base*>& ast )
//void OptimizeAst( Ast::Program::astList_t& ast )
{
    class NoOp : public Ast::Base
    {
    public:
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "NoOp\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const {}
    };
    static NoOp noop;

    auto delNoops = [&]() {
        for (int i = ast.size() - 1; i >= 0; --i)
        {
            if (ast[i] == &noop)
                ast.erase( ast.begin() + i );
        }
    };
    
    for (int i = 0; i < ast.size() - 1; ++i)
    {
        Ast::Base* step = ast[i];

        if (1 && !dynamic_cast<const Ast::ApplyProgram*>(step))
        {
            const Ast::Program* pup = dynamic_cast<const Ast::Program*>(step);
            if (pup && dynamic_cast<const Ast::Apply*>(ast[i+1]))
            {
//                std::cerr << "Replacing PushFun param=" << (pup->hasParam() ? pup->getParamName() : "-") << std::endl;
                Ast::ApplyProgram* newfun = new Ast::ApplyProgram( pup );
                ast[i] = newfun;
                ast[i+1] = &noop;
                // delete pup; // keep that guy around! (unfortunate, but needed for dynamic name lookup unless we
                // 'reparent' or redesign this thing) ? still true??
                ++i;
                continue;
            }
        }

        if (1 && !dynamic_cast<const Ast::ApplyFunctionRec*>(step))
        {
            const Ast::PushFunctionRec* pup = dynamic_cast<const Ast::PushFunctionRec*>(step);
            if (pup && dynamic_cast<const Ast::Apply*>(ast[i+1]))
            {
//                std::cerr << "Replacing PushFun param=" << (pup->hasParam() ? pup->getParamName() : "-") << std::endl;
                Ast::ApplyFunctionRec* newfun = new Ast::ApplyFunctionRec( pup );
//                newfun->reparentKids(pup); // ugly
                ast[i] = newfun;
                ast[i+1] = &noop;
                
                // delete pup; // keep that guy around! (unfortunate, but needed for dynamic name lookup unless we
                // 'reparent' or redesign this thing
                ++i;
                continue;
            }
        }
        
        if (!dynamic_cast<const Ast::ApplyUpval*>(step))
        {
            const Ast::PushUpval* pup = dynamic_cast<const Ast::PushUpval*>(step);
            if (pup && dynamic_cast<const Ast::Apply*>(ast[i+1]))
            {
                 ast[i] = new Ast::ApplyUpval( pup );
                 ast[i+1] = &noop;
                 delete pup;
                 ++i;
                 continue;
            }
        }
        
        if (!dynamic_cast<const Ast::ApplyPrimitive*>(step))
        {
            const Ast::PushPrimitive* pp = dynamic_cast<const Ast::PushPrimitive*>(step);
            if (pp && dynamic_cast<const Ast::Apply*>(ast[i+1]))
            {
                ast[i] = new Ast::ApplyPrimitive( pp );
                ast[i+1] = &noop;
                delete step;
                ++i;
                continue;
            }
        }
    }

    delNoops();

    for (int i = 0; i < ast.size() - 1; ++i)
    {
        const Ast::PushLiteral* plit = dynamic_cast<const Ast::PushLiteral*>(ast[i]);
        if (plit && plit->v_.isnum() && plit->v_.tonum() == 1.0)
        {
            Ast::ApplyPrimitive* pprim = dynamic_cast<Ast::ApplyPrimitive*>(ast[i+1]);
            if (pprim)
            {
                if (pprim->primitive_ == &Primitives::plus)
                {
                    pprim->primitive_ = &Primitives::increment;
                    pprim->desc_ = "increment";
                    ast[i] = &noop;
                }
                else if (pprim->primitive_ == &Primitives::minus)
                {
                    pprim->primitive_ = &Primitives::decrement;
                    pprim->desc_ = "decrement";
                    ast[i] = &noop;
                }
            }
        }
    }

    delNoops();
    
    const int astLen = ast.size();
    if (astLen > 1) // last instr should always be "kBreakProg"
    {
        Ast::Base* possibleTail = ast[astLen-2];
        if (possibleTail->isTailable() && ast[astLen-1]->instr_ == Ast::Base::kBreakProg)
        {
//            std::cerr << "got possibleTail convertToTailCall" << std::endl;
            possibleTail->convertToTailCall();
        }
    }
}

namespace Primitives {
    //////////////////////////////////////////////////////////////////
    // yucky system specific code
    //////////////////////////////////////////////////////////////////
#if __GNUC__    
    typedef __attribute__((__stdcall__)) int (*tfn_libopen)( Stack*, const RunContext* );
#else
    typedef __declspec(dllexport) int (*tfn_libopen)( Stack*, const RunContext* );
#endif
    
#if defined(_WIN32)
    void crequire( Bang::Stack&s, const RunContext& rc )
    {
        const auto& v = s.pop();
        const auto& libname = v.tostr();
#if UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wpath = converter.from_bytes( libname );
        const auto& llstr = wpath.c_str();
#else
        const auto& llstr = libname.c_str();
#endif 
        HMODULE hlib = LoadLibrary( llstr );
        if (!hlib)
        {
            std::cerr << "Could not load library=" << libname << std::endl;
            return;
        }
        auto proc = GetProcAddress( hlib, "bang_open" );
        if (!proc)
        {
            std::cerr << "Could not find 'bang_open()' in library=" << libname << std::endl;
            return;
        }
//         std::cerr << "Calling 'bang_open()' in library=" << libname 
//                       << " stack=0x" << std::hex <<  (void*)&s << std::dec << std::endl;

        reinterpret_cast<tfn_libopen>(proc)( &s, &rc );
    }
#else
    void crequire( Bang::Stack&s, const RunContext& rc )
    {
        const auto& v = s.pop();
        std::string libname = v.tostr();

        if (libname.substr(libname.size()-1-3,std::string::npos) != ".so")
        {
            libname += ".so";
        }

        const auto& llstr = libname.c_str();

        void *lib = dlopen( llstr, RTLD_NOW | RTLD_GLOBAL );

        if (!lib)
        {
            std::cerr << "Could not load library=" << libname << std::endl;
            return;
        }
        void* voidproc = dlsym( lib, "bang_open" );
        if (!voidproc)
        {
            std::cerr << "Could not find 'bang_open()' in library=" << libname << std::endl;
            return;
        }
//         std::cerr << "Calling 'bang_open()' in library=" << libname 
//                       << " stack=0x" << std::hex <<  (void*)&s << std::dec << std::endl;

        reinterpret_cast<tfn_libopen>(voidproc)( &s, &rc );
    }
#endif
}


    


Parser::Program::Program
(   StreamMark& stream,
    const Ast::Program* parent,
    const Ast::CloseValue* upvalueChain,
    ParsingRecursiveFunStack* pRecParsing
)
{
//    std::cerr << "enter Program fun=" << pCurrentFun << "\n";
    try
    {
        bool bHasOpenBracket = false;
        
        while (true)
        {
            eatwhitespace(stream);
            
            //////////////////////////////////////////////////////////////////
            // Strip comments, push literals
            //////////////////////////////////////////////////////////////////
            try
            {
                Comment c(stream);
                continue;
            } catch ( const ErrorNoMatch& ) {}

            try
            {
                ParseLiteral aLiteral(stream);
                ast_.push_back( new Ast::PushLiteral(aLiteral.value()) );
                continue;
            } catch ( const ErrorNoMatch& ) {}


            {
                StreamMark mark(stream);            

                if (eatReservedWord("as", mark))
                {
                    if (eatwhitespace(mark))
                    {
                        Identifier valueName( mark );
                        mark.accept();
                        Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, valueName.name() );
                        upvalueChain = cv;
                        ast_.push_back( cv );
                        continue;
                    }
                }
            }
            
            //////////////////////////////////////////////////////////////////
            // Define functions; 'fun', 'fun!', and 'def' keywords
            //////////////////////////////////////////////////////////////////
            try
            {
                Fundef fun( stream, upvalueChain, pRecParsing );

                ast_.push_back( fun.stealProgram() );

                if (fun.hasPostApply())
                    ast_.push_back( new Ast::Apply() );

                continue;
            } catch ( const ErrorNoMatch& ) {}

            try
            {
                Defdef fun( stream, upvalueChain, pRecParsing );
                
                ast_.push_back( fun.stealDefProg() );
                
                if (fun.hasPostApply())
                    ast_.push_back( new Ast::Apply() );
                else
                {
                    Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, fun.getDefName() );
                    upvalueChain = cv;
                    ast_.push_back( cv );
                }
                continue;
            } catch ( const ErrorNoMatch& ) {}

            /////// Single character operators ///////
            StreamMark mark(stream);            
            char c = mark.getc();

            //////////////////////////////////////////////////////////////////
            // ; or } delimeters close a program
            //////////////////////////////////////////////////////////////////
            if (c == ';')
            {
                mark.accept();
                break;
            }
            else if (c == '{') // technically, this should only be accepted at the head of the Program (before any
                               // statement other than whitespace) - but this is close enough for now
            {
                mark.accept();
                bHasOpenBracket = true;
                continue;
            }
            else if (c == ':')
            {
//                std::cerr << "  got : breakout\n";
                break; // second half of else clause, break back to higher level Program parsing
            }
            else if (c == '}')
            {
                // If I opened the bracket, then eat it; if not, let it go, let it go- up, up, up!
                // to close all previous scopes until we find where the bracket was opened
                if (bHasOpenBracket)
                    mark.accept(); 
                break;
            }

            //////////////////////////////////////////////////////////////////
            // Single character operators
            //////////////////////////////////////////////////////////////////
            if (c == '!')
            {
                mark.accept();
                ast_.push_back( new Ast::Apply() );
                continue;
            }
#if HAVE_MUTATION            
            else if (c == '|')
            {
                if (mark.getc() == '>')
                {
                    eatwhitespace(mark);
                    Identifier ident( mark );
                    mark.accept();
                    const NthParent upvalNumber = upvalueChain->FindBinding( ident.name() );
                    ast_.push_back( new Ast::SetUpval(ident.name(), upvalNumber) );
                    continue;
                }
            }
#endif 
            else if (c == '?')
            {
//                std::cerr << "found if-else\n";
                // ast_.push_back( new Ast::ConditionalApply() );
                mark.accept();
                try
                {
                    Program ifBranch( stream, nullptr, upvalueChain, pRecParsing );
                    Ast::Program* ifProg = new Ast::Program( nullptr, ifBranch.ast() );
                    Ast::Program* elseProg = nullptr;
                    eatwhitespace(stream);
                    c = mark.getc();
                    if (c != ':')
                    {
                        mark.regurg(c); // no single char operator found
                    }
                    else
                    {
                        mark.accept();
//                        std::cerr << "  found else clause\n";
                        Program elseBranch( stream, nullptr, upvalueChain, pRecParsing );
                        elseProg = new Ast::Program( nullptr, elseBranch.ast() );
                    }
                        
                    ast_.push_back( new Ast::IfElse( ifProg, elseProg ) );
                }
                catch (...)
                {
                    std::cerr << "?? error on ifelse\n";
                }
                continue;
            }
            else if (c == '.') // "Object Method / Message / Dereference / Index" operator 
            {
                /* Object methods: swap the methodname and object so that we "apply" the
                   object, with a string for the methodname as the first parameter provided
                   to the object call.  This allows objects to be simple closures which
                   receive a string name for the method */
                mark.accept();
                try
                {
                    Identifier methodName( mark );
                    mark.accept();
                    
                    ast_.push_back( new Ast::PushLiteral( Value(methodName.name())) );                    
                    ast_.push_back( new Ast::PushPrimitive( &Primitives::swap, "swap" ) );
                       ast_.push_back( new Ast::Apply() ); // haveto apply the swap
                    ast_.push_back( new Ast::Apply() ); // now apply to the object!!
                    continue;
                }
                catch (const ErrorNoMatch& )
                {
                    std::runtime_error("method operator (.) must be followed by an identifier");
                }
            }
            else // check for "C++ Function" primitives: +, -, *, /, ~
            {
                tfn_primitive prim = bangprimforchar(c);
                if (prim)
                {
                    mark.accept();
                    ast_.push_back( new Ast::PushPrimitive( prim, std::string(&c, (&c)+1) ) );
                    ast_.push_back( new Ast::Apply() );
                    continue;
                }
            }
                
            mark.regurg(c); // no single char operator found

            //////////////////////////////////////////////////////////////////
            // Reserved Words
            //////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////
            // Identifiers
            // catch all after everything else fails
            //////////////////////////////////////////////////////////////////
            try
            {
                Identifier ident( mark );
                
                if (ident.name() == "lookup")
                {
                    mark.accept();
                    ast_.push_back( new Ast::PushUpvalByName() );
                    continue;
                }

                if (ident.name() == "coroutine")
                {
                    if (mark.getc()=='!')
                    {
                        mark.accept();
                        ast_.push_back( new Ast::MakeCoroutine() );
                        continue;
                    }
                }

                if (ident.name() == "yield")
                {
                    if (mark.getc()=='!')
                    {
                        mark.accept();
                        ast_.push_back( new Ast::YieldCoroutine() );
                        continue;
                    }
                }
                
                if (ident.name() == "require")
                {
                    mark.accept();
                    ast_.push_back( new Ast::Require() );
                    continue;
                }
                
                auto rwPrimitive = [&] ( const std::string& kw, tfn_primitive prim ) -> bool {
                    if (ident.name() == kw)
                    {
                        mark.accept();
                        ast_.push_back( new Ast::PushPrimitive( prim, kw ) );
                        return true;
                    }
                    return false;
                };
                
                if (rwPrimitive( "not",    &Primitives::lognot ) ) continue;
                if (rwPrimitive( "and",    &Primitives::logand ) ) continue;
                if (rwPrimitive( "or",     &Primitives::logor  ) ) continue;
                if (rwPrimitive( "drop",   &Primitives::drop   ) ) continue;
                if (rwPrimitive( "swap",   &Primitives::swap   ) ) continue;
                if (rwPrimitive( "dup",    &Primitives::dup    ) ) continue;
                if (rwPrimitive( "nth",    &Primitives::nth    ) ) continue;
                if (rwPrimitive( "print",   &Primitives::print   ) ) continue;
                if (rwPrimitive( "save-stack",    &Primitives::savestack    ) ) continue;
                if (rwPrimitive( "stack-to-array",    &Primitives::stackToArray    ) ) continue;
//                if (rwPrimitive( "require_math",    &Primitives::require_math    ) ) continue;
                if (rwPrimitive( "crequire",    &Primitives::crequire    ) ) continue;
                if (rwPrimitive( "tostring", &Primitives::tostring    ) ) continue;
//                if (rwPrimitive( "require",    &Primitives::require    ) ) continue;
            
                bool bFoundRecFunId = false;

                if (pRecParsing)
                {
                    const auto& recfunpair = pRecParsing->FindProgramForIdent(ident.name());
                    if (recfunpair.first)
                    {
                        auto nthbinding = upvalueChain->FindBinding( recfunpair.second );
                        ast_.push_back( new Ast::PushFunctionRec( recfunpair.first, nthbinding ) );
                        bFoundRecFunId = true;
                    }
                }

                if (!bFoundRecFunId)
                {
                    const NthParent upvalNumber = upvalueChain ? upvalueChain->FindBinding( ident.name() ) : kNoParent;
                
                    if (upvalNumber == kNoParent)
                    {
                        std::cerr << "Could not find binding for var=" << ident.name() << " uvchain=" << (void*)upvalueChain << std::endl;
                        throw ParseFail( mark.sayWhere(), "Unbound variable" );
                    }

                    ast_.push_back( new Ast::PushUpval(ident.name(), upvalNumber) );
                }
                
                mark.accept();
            }
            catch( const ErrorNoMatch& )
            {
                std::cerr << "Cannot parse at '" << c << "'" << std::endl;
                throw ParseFail( mark.sayWhere(), "unparseable token");
            }
        } // end, while true

        stream.accept();
        ast_.push_back( new Ast::BreakProg() );
        OptimizeAst( ast_ );
    }
    catch (const ErrorEof&)
    {
        stream.accept();
        if (gReplMode)
            ast_.push_back( new Ast::EofMarker() );
        else
            ast_.push_back( new Ast::BreakProg() );
        OptimizeAst( ast_ );
    }
}


class RequireKeyword
{
    const std::string fileName_;
    bool stdin_;
public:
    RequireKeyword( const char* fname )
    :  fileName_( fname ? fname : "" ),
       stdin_( !fname )
    {
    }


    Ast::Program* parseToProgram( RegurgeIo& stream, bool bDump, const Ast::CloseValue* upvalchain )
    {
        StreamMark mark(stream);

        try
        {
            Parser parser( mark, upvalchain );

            Ast::Program* p = new Ast::Program( nullptr /* parent */, parser.programAst() );
            
            if (bDump)
                p->dump( 0, std::cerr );

            return p;
        }
        catch (const ErrorEof& )
        {
            std::cerr << "Found EOF without valid program!" << std::endl;
        }
        catch (const std::runtime_error& e )
        {
            std::cerr << "Runtime parse error b01: " << e.what() << std::endl;
            throw e;
        }

        return nullptr;
    }

    
    Ast::Program* parseNoUpvals( RegurgeIo& stream, bool bDump )
    {
        return parseToProgram( stream, bDump, nullptr );

        // fun->setAst( *pProgram );

        // return pProgram;
    }

    std::shared_ptr<BoundProgram> parseToBoundProgramNoUpvals( Stack& stack, bool bDump )
    {
        Ast::Program* fun;

        if (stdin_)
        {
            RegurgeStdinRepl strmStdin;
            fun = this->parseNoUpvals( strmStdin, bDump );
        }
        else
        {
            RegurgeFile strmFile( fileName_ );
            try
            {
                fun = this->parseNoUpvals( strmFile, bDump );
            }
            catch( const ParseFail& e )
            {
                std::cerr << "Error parsing program: " << e.what();
                throw e;
            }
        }

        // BANGCLOSURE noParentClosure(nullptr);
        SHAREDUPVALUE noUpvals;
        const auto& boundprog = NEW_BANGFUN(BoundProgram)( fun, noUpvals );

        return boundprog;
    }
    
    void parseAndRun( Thread& thread, bool bDump)
    {
        const auto& closure = parseToBoundProgramNoUpvals( thread.stack, bDump );
            
        try
        {
            RunProgram( &thread, closure->program_, closure->upvalues_ );
            // closure->apply( stack ); // , closure );
        }
        catch( AstExecFail ast )
        {
            gFailedAst = ast.pStep;
            closure->dump( std::cerr );
            std::cerr << "Runtime AST exec Error" << gFailedAst << ": " << ast.e.what() << std::endl;
        }
        catch( const std::exception& e )
        {
            std::cerr << "Runtime exec Error: " << e.what() << std::endl;
        }
    }
}; // end, class RequireKeyword




void Ast::Require::run( Stack& stack, const RunContext& rc ) const
{
    const auto& v = stack.pop(); // get file name
    if (!v.isstr())
        throw std::runtime_error("no filename found for require??");

    RequireKeyword me( v.tostr().c_str() );

    const auto& closure = me.parseToBoundProgramNoUpvals( stack, false );
    
    stack.push( closure ); // STATIC_CAST_TO_BANGFUN(closure) );
    // auto newfun = std::make_shared<FunctionRequire>( s.tostr() );
}

    Ast::Program* parseStdinToProgram( const Ast::CloseValue* closeValueChain )
    {
        RequireKeyword req(nullptr);
        RegurgeStdinRepl strmStdin;
        return
        req.parseToProgram
        (   strmStdin,
            gDumpMode,
            closeValueChain
        );
    }
    
} // end namespace Bang



/*
Generate test output:

  dir test\*.bang | %{ .\bang $_ | out-file -encoding ascii .\test\$("out." + $_.name + ".out") }

Test against reference output:

  dir test\*.bang | %{ $t=.\bang $_;  $ref = cat .\test\$("out." + $_.name + ".out"); if (compare-object $t $ref) { throw "FAILED $($_.name)!" } }  
 */
int bangmain( int argc, char* argv[] )
{
    std::cerr << "Bang! v" << BANG_VERSION << " - Welcome!" << std::endl;

    bool bDump = false;
    if (argc > 1)
    {
        if (std::string("-dump") == argv[1])
        {
            bDump = true;
            argv[1] = argv[2];
            --argc;
        }

        if (std::string("-i") == argv[1])
        {
            gReplMode = true;
            argv[1] = argv[2];
            --argc;
        }
    }
    const char* fname = argv[1];
    if (argc < 2)
    {
#ifdef kDefaultScript
        bDump = true;
        fname = kDefaultScript;
#else
        fname = nullptr; // kDefaultScript;
        gReplMode = true;
#endif 
    }

    gDumpMode = bDump;

    Bang::Thread thread;

    if (gReplMode)
        Bang::repl_prompt();
    
    do
    {
        try
        {
            Bang::RequireKeyword requireMain( fname );
            requireMain.parseAndRun( thread, bDump );
            thread.stack.dump( std::cout );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    while (gReplMode);
    
    std::cerr << "toodaloo!" << std::endl;
}

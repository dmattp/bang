//////////////////////////////////////////////////////////////////
// Bang! language interpreter
// (C) 2015 David M. Placek
// All rights reserved, see accompanying [license.txt] file
//////////////////////////////////////////////////////////////////

#define PBIND(...) do {} while (0)


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

#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sstream>

#if defined(_WIN32)
# include <windows.h> // for load library
# if !__GNUC__
#  include <codecvt> // to convert to wstring
# endif 
#endif


const char* const BANG_VERSION = "0.002";
const char* const kDefaultScript = "c:\\m\\n2proj\\bang\\test\\7.bang";

#include "bang.h"

namespace {
    bool gReplMode(false);
    bool gDumpMode(false);
}

namespace Bang {


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
            o << "(function)";
        else if (type_ == kFunPrimitive)
            o << "(prim.function)";
        else
            o << "(???)";
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
    

class NthParent {
    int nth_;
public:
    explicit NthParent( int n ) : nth_(n) {}
    NthParent operator++() const { return NthParent(nth_+1); }
    NthParent operator--() const { return NthParent(nth_-1); }
    bool operator==( NthParent other ) const { return nth_ == other.nth_; }
    int toint() const { return nth_; } // use judiciously, please
};

static const NthParent kNoParent=NthParent(INT_MAX);


void indentlevel( int level, std::ostream& o )
{
    if (level > 0)
    {
        o << "  ";
        indentlevel( level - 1, o );
    }
}

class FunctionClosure;
class RunContext
{
    friend class FunctionClosure;
public:
    CLOSURE_CREF pActiveFun_;
public:    
    RunContext( CLOSURE_CREF fun )
    : pActiveFun_(fun)
    {
    }
    CLOSURE_CREF running() const { return pActiveFun_; }

    const Value& getUpValue( NthParent up ) const;
    const Value& getUpValue( const std::string& uvName ) const;
};


namespace Primitives
{
    template <class T>
    void infix2to1( Stack& s, T operation )
    {
        const Value& v2 = s.loc_top();
        Value& v1 = s.loc_topMinus1Mutate();

        if (v1.isnum() && v2.isnum())
        {
            v1.mutateNoType( operation( v1.tonum(), v2.tonum() ) );
            s.pop_back();
        }
        else
            throw std::runtime_error("Binary operator incompatible types");
    }

    template <class T>
    void infix2to1bool( Stack& s, T operation )
    {
        const Value& v2 = s.loc_top();
        Value& v1 = s.loc_topMinus1Mutate();

        if (v1.isnum() && v2.isnum())
        {
            v1.mutatePrimitiveToBool( operation( v1.tonum(), v2.tonum() ) );
            s.pop_back();
        }
        else
            throw std::runtime_error("Binary operator.b incompatible types");
    }
    
    void plus ( Stack& s, const RunContext& )
    {
        if (s.loc_top().isnum())
            infix2to1( s, [](double v1, double v2) { return v1 + v2; } );
        else if (s.loc_top().isstr())
        {
            const auto& v2 = s.pop();
            const auto& v1 = s.pop();
            s.push( v1.tostr() + v2.tostr() );
        }
    }
    void minus( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 - v2; } ); }
    void mult ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 * v2; } ); }
    void div  ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 / v2; } ); }
    void lt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 < v2; } ); }
    void gt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 > v2; } ); }
    void modulo ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return double(int(v1) % int(v2)); } ); }
    void eq( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 == v2; } ); }
    
    
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
        vfmt.dump( std::cout );
        std::cout << std::endl;
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
        virtual void run( Stack& stack, const RunContext& ) const = 0;
        enum EAstInstr {
            kUnk,
            kBreakProg,
            kApply,
            kApplyUpval,
            kApplyFun,
            kTCOApply,
            kTCOApplyUpval,
            kTCOApplyFun
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
                case kApply:      instr_ = kTCOApply;      break;
                case kApplyFun:   instr_ = kTCOApplyFun;   break;
                case kApplyUpval: instr_ = kTCOApplyUpval; break;
            }
        }
    private:
    };
}

const Ast::Base* gFailedAst = nullptr;
struct AstExecFail {
    const Ast::Base* pStep;
    std::runtime_error e;
    AstExecFail( const Ast::Base* step, const std::runtime_error& e )
    : pStep(step), e(e)
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
        virtual void run( Stack& stack, const RunContext& ) const
        {}
    };
    class EofMarker : public Base
    {
    public:
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "<<< EOF >>>\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class PushLiteral : public Base
    {
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
    protected:
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
    
    class PushUpval : public Base
    {
    protected:
        std::string name_;
        NthParent uvnumber_; // index in the containing PushFun's array of upvalues
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

        virtual void run( Stack& stack, const RunContext& ) const;

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
        
        virtual void run( Stack& stack, const RunContext& ) const;
        static bool applyOrIsClosure( const Value& v, Stack& stack, const RunContext& );
    };


    class PushFun;

    // never loaded directly into program tree - always a member of PushFun
    class Program //  : public Base
    {
    public:
        typedef std::vector<Ast::Base*> astList_t;
    private:
        astList_t ast_;

    public:
        Program( const astList_t& ast )
        : ast_( ast )
        {}

        Program()  // empty program ~~~ who uses this? hmm
        {}

        Program& add( Ast::Base* action ) {
            ast_.push_back( action );
            return *this;
        }
        Program& add( Ast::Base& action ) {
            ast_.push_back( &action );
            return *this;
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Program\n";
            std::for_each
            (   ast_.begin(), ast_.end(),
                [&]( const Ast::Base* ast ) { ast->dump( level+1, o ); }
            );
        }

        const astList_t* getAst() { return &ast_; }

        // void run( Stack& stack, std::shared_ptr<Function> pFunction ); 
    }; // end, class Ast::Program


    class IfElse : public Base
    {
        Ast::Program* if_;
        Ast::Program* else_;
    public:
        IfElse( Ast::Program* if__, Ast::Program* else__ )
        : if_(if__),
          else_( else__ )
        {}

        Ast::Program* branchTaken( Stack& stack ) const
        {
            bool isCond = stack.loc_top().tobool();
            stack.pop_back();
            return isCond ? if_ : else_;
        }
        
        void run( Stack& stack, const RunContext& rc ) const;
        
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
    };
    
    class PushFun : public Base
    {
    protected:
        const PushFun* pParentFun_;
        Ast::Program*  astProgram_;
        std::string* pParam_;

    public:
        Ast::Program* getProgram() const { return astProgram_; }

        PushFun( const Ast::PushFun* pParentFun )
        : pParentFun_(pParentFun),
          astProgram_( nullptr ),
          pParam_(nullptr)
        {
        }

        void setAst( const Ast::Program& astProgram ) { astProgram_ = new Ast::Program(astProgram); }
        PushFun& setParamName( const std::string& param ) { pParam_ = new std::string(param); return *this; }
        bool hasParam() const { return pParam_ ? true : false; }
        std::string getParamName() const { return *pParam_; }
        bool DoesBind( const std::string& varName ) const { return pParam_ && *pParam_ == varName; }

        const NthParent FindBinding( const std::string& varName ) const
        {
            if (this->DoesBind(varName))
            {
                return NthParent(0);
            }
            else
            {
                if (!pParentFun_)
                    return kNoParent;

                auto fbp = pParentFun_->FindBinding(varName);

                return (fbp == kNoParent ) ? fbp : ++fbp;
            }
        }
        
        virtual void dumpshort( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec <<
                " PushFun(" << (pParam_ ? *pParam_ : "--") << ")";
//             if (upvalues_.size() > 0)
//                 o << " " << upvalues_.size() << " upvalues";
        }


        virtual void dump( int level, std::ostream& o ) const
        {
            this->dumpshort( level, o );
            o << ":";  
            astProgram_->dump( level, o );
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    }; // end, class Ast::PushFun

    class ApplyFun : public PushFun
    {
    public:
        ApplyFun( const PushFun* pf )
        : PushFun( *pf )
        {
            instr_ = kApplyFun;
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec <<
                " ApplyFun(" << (pParam_ ? *pParam_ : "--") << ")";
            o << ":";  
            astProgram_->dump( level, o );
        }
        virtual void run( Stack& stack, const RunContext& ) const;
    };


    class PushFunctionRec : public Base
    {
        Ast::PushFun* pRecFun_;
    public:
        PushFunctionRec( Ast::PushFun* other )
        : pRecFun_( other )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "PushFunctionRec[" << std::hex << PtrToHash(pRecFun_) << std::dec << "]" << std::endl;
        }

        virtual void run( Stack& stack, const RunContext& ) const;
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

class FunctionClosure;

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

    
// Function should be Constructed when the PushFun is pushed,
// thereby binding to the pushing context and preserving upvalues.
// ParamValue will be set when it is invoked, which could happen
// multiple times in theory.

//~~~ @todo: maybe separate out closure which binds a variable from
// one which doesn't?  If nothing else this could make upvalue lookup chain
// shorter
class FunctionClosure : public Function
{
    const Ast::PushFun* pushfun_;                      // permanent
    BANGCLOSURE pParent_;  // set when pushed??
    Value paramValue_;                          // set when applied??
    friend class Ast::PushFunctionRec;
public:

    static BANGCLOSURE lexicalMatch(BANGCLOSURE start, const Ast::PushFun* target)
    {
        while (start && start->pushfun_ != target)
            start = start->pParent_;
        
        return start;
    }
    
    FunctionClosure( const Ast::PushFun& pushfun, CLOSURE_CREF pParent )
    : Function(true),
      pushfun_( &pushfun ),
      pParent_( pParent ) 
    {
    }

    const Value& paramValue() const // const std::string& uvName ) const
    {
        return paramValue_;
    }

    const Value& getUpValue( NthParent uvnumber )
    {
        if (uvnumber == NthParent(0))
            return paramValue_;
        else
            return pParent_->getUpValue( --uvnumber );
    }

    // lookup by string / name is expensive; used for experimental object
    // system.  obviously if it's "a hit" you can optimize out much of this cost
    const Value& getUpValue( const std::string& uvName )
    {
        const NthParent uvnum = pushfun_->FindBinding(uvName);
        if (uvnum == kNoParent)
        {
            std::cerr << "Error Looking for dynamic upval=\"" << uvName << "\"\n";
            throw std::runtime_error("Could not find dynamic upval");
        }
        return getUpValue( uvnum );
    }

    // the pedant in me says "bind" should return a new type, a "BoundFunction" or something;
    // that honestly may clear some of the confusion up... what color is the bike under your desk,
    // object lifetimes and all that, no mutation...
    // but some of that may fall out with optimizations to separate out function that don't bind a param
    void bindParams( Stack& s ) 
    {
        if (pushfun_->hasParam()) // bind parameter
            paramValue_ = s.pop();
    }

    Ast::Program* getProgram() const
    {
        return pushfun_->getProgram();
    }

    CLOSURE_CREF getParent() const { return pParent_; }

    const Ast::PushFun* pushfun() { return pushfun_; }

    void apply( Stack& s, CLOSURE_CREF myself );
    
}; // end, class FunctionClosure


#if !USE_GC    
SimpleAllocator< std::shared_ptr<FunctionClosure> > gFuncloseAlloc;

# undef  NEW_CLOSURE
# define NEW_CLOSURE(a,b)           std::allocate_shared<FunctionClosure>( gFuncloseAlloc, a, b )
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
    virtual void apply( Stack& s, CLOSURE_CREF running )
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
    virtual void apply( Stack& s, CLOSURE_CREF running )
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


void RunProgram
(   Stack& stack,
    BANGCLOSURE pFunction,
    Ast::Program* pProgram
)
{
restartTco:
    RunContext rc( pFunction );

    const Ast::Base* const* ppInstr = &(pProgram->getAst()->front());

#if 0    
    auto ApplyValue = [&]( const Value& calledFun ) -> bool
    {
        if (Ast::Apply::applyOrIsClosure( calledFun, stack, rc ))
        {
            // "rebind" RunProgram() parameters
            pFunction = DYNAMIC_CAST_TO_CLOSURE(calledFun.tofun());
            pFunction->bindParams( stack );
            pProgram = pFunction->getProgram();
            return true;
        } // end , closure
        else
            return false;
    };
#endif 
    
    try
    {
        while (true)
        {
            const Ast::Base* pInstr = *ppInstr++;
            switch (pInstr->instr_)
            {
                case Ast::Base::kBreakProg:
                    return;

                default:
                    pInstr->run( stack, rc );
                    break;

                case Ast::Base::kTCOApplyFun:
                {
                    // no dynamic cast, should be safe since we know the type from instr_
                    const Ast::ApplyFun* afn = reinterpret_cast<const Ast::ApplyFun*>(pInstr); 
                    pFunction = NEW_CLOSURE(*afn, rc.running());
                    pFunction->bindParams(stack);
                    pProgram = afn->getProgram();
                    goto restartTco;
                }
                break;
                    
                case Ast::Base::kTCOApplyUpval:
                {
                    // no dynamic cast, should be safe since we know the type from instr_
#if 0
                    if (ApplyValue( reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( rc ) ))
                        goto restartTco;
#else
                    const Value& calledFun = reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( rc );
                    if (Ast::Apply::applyOrIsClosure( calledFun, stack, rc ))
                    {
                        // "rebind" RunProgram() parameters
                        pFunction = DYNAMIC_CAST_TO_CLOSURE(calledFun.tofun());
                        pFunction->bindParams( stack );
                        pProgram = pFunction->getProgram();
                        goto restartTco;
                    } // end , closure
#endif 
                }
                break;

                case Ast::Base::kTCOApply:
                {
#if 0
                    if (ApplyValue( stack.pop() ))
                        goto restartTco;
#else
                    const Value& calledFun = stack.pop();
                    if (Ast::Apply::applyOrIsClosure( calledFun, stack, rc ))
                    {
                        // "rebind" RunProgram() parameters
                        pFunction = DYNAMIC_CAST_TO_CLOSURE(calledFun.tofun());
                        pFunction->bindParams( stack );
                        pProgram = pFunction->getProgram();
                        goto restartTco;
                    } // end , closure
#endif 
                }
                break;
            } // end,instr switch
        }
    }
    catch (const std::runtime_error& e)
    {
        throw AstExecFail( *ppInstr, e );
    }
}
    
void FunctionClosure::apply( Stack& s, CLOSURE_CREF myself )
{
    myself->bindParams( s );
    RunProgram( s, myself, this->getProgram() );
}

    void Ast::IfElse::run( Stack& stack, const RunContext& rc ) const
    {
        Ast::Program* p = this->branchTaken(stack);
        if (p)
            RunProgram( stack, rc.running(), p );
    }
    

const Value& RunContext::getUpValue( NthParent uvnumber ) const
{
    return pActiveFun_->getUpValue( uvnumber );
}

const Value& RunContext::getUpValue( const std::string& uvName ) const
{
    return pActiveFun_->getUpValue( uvName );
}


// PushFun::run is what runs when a lambda is pushed on the
// stack.  At this point we "grab" the parent from the
// run context
void Ast::PushFun::run( Stack& stack, const RunContext& rc ) const
{
    const auto& pfr = NEW_CLOSURE(*this, rc.running());
    stack.push( STATIC_CAST_TO_BANGFUN(pfr) );
}

void Ast::ApplyFun::run( Stack& stack, const RunContext& rc ) const
{
    const auto& pfr = NEW_CLOSURE(*this, rc.running());
    pfr->apply( stack, pfr );
}
    

void Ast::PushFunctionRec::run( Stack& stack, const RunContext& rc ) const
{
    BANGCLOSURE myself = FunctionClosure::lexicalMatch( rc.running(), pRecFun_ );
    CLOSURE_CREF myselfsParent = myself->getParent();
    const auto& otherfun = NEW_CLOSURE( *pRecFun_, myselfsParent );
    stack.push( STATIC_CAST_TO_BANGFUN(otherfun) );
}



bool Ast::Apply::applyOrIsClosure( const Value& v, Stack& stack, const RunContext& rc )
{
    if (v.isfunprim())
    {
        const tfn_primitive pprim = v.tofunprim();
        pprim( stack, rc );
    }
    else if( v.isfun() )
    {
        const auto& pfun = v.tofun();
        if (pfun->isClosure())
            return true;
        else
            pfun->apply( stack, rc.running() );
    }
    else
    {
        std::ostringstream oss;
        oss << "Called apply without function; found type=" << v.type_ << " V=";
        v.dump(oss);
        throw std::runtime_error(oss.str());
    }
    
    return false;
}

void Ast::ApplyUpval::run( Stack& stack, const RunContext& rc) const
{
    const Value& v = this->getUpValue( rc );
    if (Ast::Apply::applyOrIsClosure( v, stack, rc ))
        v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
};


void Ast::Apply::run( Stack& stack, const RunContext& rc ) const
{
    const Value& v = stack.pop();

    if (Ast::Apply::applyOrIsClosure( v, stack, rc ))
        v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
}

// void Ast::ConditionalApply::run( Stack& stack, const RunContext& rc) const
// {
//     const bool shouldExec = Ast::ConditionalApply::foundTrue(stack);

//     // always pop the function, even if not taken, for consistency's sake
//     const Value& v = stack.pop();
    
//     if (shouldExec && Ast::Apply::applyOrIsClosure( v, stack, rc ))
//         v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
// }


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
    void regurg( char c )
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
    FILE* f_;
public:
    RegurgeFile( FILE* f )
    : f_(f)
    {}

    char getc()
    {
        int icud = RegurgeIo::getcud();

        if (icud != EOF)
            return icud;
        
        int istream = fgetc(f_);
        if (istream==EOF)
            throw ErrorEof();
        else
            return istream;
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
        Program( StreamMark&, const Ast::PushFun* pCurrentFun, ParsingRecursiveFunStack* pRecParsing );

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
                    *bi++ = cstr;
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
//            std::cerr << "In ParseLiteral\n";
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

//    class Ast::PushFun;
    struct ParsingRecursiveFunStack
    {
        ParsingRecursiveFunStack* prev;
        Ast::PushFun* fun;
        Ast::PushFun* bindingfun;

        Ast::PushFun* FindPushFunForIdent( const std::string& ident )
        {
            if (bindingfun->getParamName() == ident)
            {
                return fun;
            }
            else if (prev)
            {
                return prev->FindPushFunForIdent( ident );
            }
            else
                return nullptr;
        }
        
        ParsingRecursiveFunStack( ParsingRecursiveFunStack* p, Ast::PushFun* f, Ast::PushFun* b )
                : prev(p), fun(f), bindingfun(b)
        {}
    };
    
    
    class Fundef
    {
        bool postApply_;
        Ast::PushFun* pNewFun_;
    public:
        ~Fundef() { delete pNewFun_; }
        
        Fundef( StreamMark& stream, const Ast::PushFun* pParentFun, ParsingRecursiveFunStack* pRecParsing )
        :  postApply_(false),
           pNewFun_(nullptr)
        {
            bool isAs = false;
            StreamMark mark(stream);

            eatwhitespace(mark);

            if (eatReservedWord( "fun", mark ))
                ;
            else if (eatReservedWord( "as", mark ))
            {
                if (!eatwhitespace(mark)) 
                    throw ErrorNoMatch(); // whitespace required after 'as'
                isAs = true;
            }
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

            pNewFun_ =  new Ast::PushFun( pParentFun );
            
            try
            {
                Identifier param(mark);
                pNewFun_->setParamName( param.name() );
                eatwhitespace(mark);
            }
            catch ( const ErrorNoMatch& ) // identifier is optional
            {
            }

            if (!isAs)
            {
                char c = mark.getc();
                if (c != '=') // this seems unneccesary sometimes. therefore "as"
                {
                    std::cerr << "got '" << c << "', expecting '='\n";
                    throw std::runtime_error("function def must be followed by '='"); // throw ErrorNoMatch();
                }
            }

            Program program( mark, pNewFun_, pRecParsing );

            pNewFun_->setAst( program.ast() );

            mark.accept();
        }
        bool hasPostApply() const { return postApply_; }
        Ast::PushFun* stealPushFun() { auto rc = pNewFun_; pNewFun_ = nullptr; return rc; }
    }; // end, class Fundef


    class Defdef
    {
        bool postApply_;
        Ast::PushFun* pDefFun_;
        Ast::PushFun* pWithDefFun_;
    public:
        ~Defdef() { delete pDefFun_; delete pWithDefFun_; }
        Defdef( StreamMark& stream, const Ast::PushFun* pParentFun, ParsingRecursiveFunStack* pRecParsing )
        :  postApply_(false),
           pDefFun_(nullptr),
           pWithDefFun_(nullptr)
        {
            StreamMark mark(stream);

            eatwhitespace(mark);
            
            if (!eatReservedWord( "def", mark ))
                throw ErrorNoMatch();

            eatwhitespace(mark);

            char c = mark.getc();
            if (c != ':')
            {
                std::cerr << "got '" << c << "' expecting ':'" << std::endl;
                throw std::runtime_error("def name must start with ':'");
            }

            pDefFun_ =  new Ast::PushFun( pParentFun );
            pWithDefFun_ =  new Ast::PushFun( pParentFun );
            
            try
            {
                Identifier param(mark);
                pWithDefFun_->setParamName( param.name() );
                eatwhitespace(mark);
            }
            catch ( const ErrorNoMatch& )
            {
                throw std::runtime_error("identifier must follow \"def :\"");
            }

            try
            {
                Identifier param(mark);
                pDefFun_->setParamName( param.name() );
                eatwhitespace(mark);
            }
            catch ( const ErrorNoMatch& )
            {
            }

            if (mark.getc() != '=') // this seems unneccesary
            {
                std::cerr << "got '" << c << "', expecting '='\n";
                throw std::runtime_error("function def must be followed by '='"); // throw ErrorNoMatch();
            }

            // std::string defFunName = pWithDefFun_->getParamName();

            ParsingRecursiveFunStack recursiveStack(pRecParsing, pDefFun_, pWithDefFun_);
            Program progdef( mark, pDefFun_, &recursiveStack );
            pDefFun_->setAst( progdef.ast() );

            Program withdefprog( mark, pWithDefFun_, nullptr );
            pWithDefFun_->setAst( withdefprog.ast() );
            
            mark.accept();
        }

        Ast::PushFun* stealDefFun() { auto rc = pDefFun_; pDefFun_ = nullptr; return rc; }
        Ast::PushFun* stealWithDefFun() { auto rc = pWithDefFun_; pWithDefFun_ = nullptr; return rc; }
    }; // end, class Fundef

    
    Program* program_;
public:
    Parser( StreamMark& mark, const Ast::PushFun* pCurrentFun )
    {
        program_ = new Program( mark, pCurrentFun, nullptr ); // 0,0,0 = no current function, no self-name
    }

    Ast::Program astProgram()
    {
        return Ast::Program( program_->ast() );
    }
};

void OptimizeAst( Ast::Program::astList_t& ast )
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
    
    const int end = ast.size() - 1;
    for (int i = 0; i < end; ++i)
    {
        Ast::Base* step = ast[i];
        if (1 && !dynamic_cast<const Ast::ApplyFun*>(step))
        {
            const Ast::PushFun* pup = dynamic_cast<const Ast::PushFun*>(step);
            if (pup && dynamic_cast<const Ast::Apply*>(ast[i+1]))
            {
//                std::cerr << "Replacing PushFun param=" << (pup->hasParam() ? pup->getParamName() : "-") << std::endl;
                Ast::ApplyFun* newfun = new Ast::ApplyFun( pup );
//                newfun->reparentKids(pup); // ugly
                ast[i] = newfun;
                ast[i+1] = &noop;
                
                // delete pup; // keep that guy around! (unfortunate, but needed for dynamic name lookup unless we
                // 'reparent' or redesign this thing
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

    }

    for (int i = ast.size() - 1; i >= 0; --i)
    {
        if (ast[i] == &noop)
            ast.erase( ast.begin() + i );
    }

    const int astLen = ast.size();
    if (astLen > 1) // last instr should always be "kBreakProg"
    {
        Ast::Base* possibleTail = ast[astLen-2];
        if (possibleTail->isTailable() && ast[astLen-1]->instr_ == Ast::Base::kBreakProg)
        {
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
    void crequire( Bang::Stack&s, const RunContext& rc )
    {
#if defined(_WIN32)
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
#endif
    }
}


    


Parser::Program::Program( StreamMark& stream, const Ast::PushFun* pCurrentFun, ParsingRecursiveFunStack* pRecParsing )
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

            
            //////////////////////////////////////////////////////////////////
            // Define functions; 'fun', 'fun!', 'as', and 'def' keywords
            //////////////////////////////////////////////////////////////////
            try
            {
                Fundef fun( stream, pCurrentFun, pRecParsing );

                ast_.push_back( fun.stealPushFun() );

                if (fun.hasPostApply())
                    ast_.push_back( new Ast::Apply() );

                continue;
            } catch ( const ErrorNoMatch& ) {}


            try
            {
                Defdef fun( stream, pCurrentFun, pRecParsing );
                
                ast_.push_back( fun.stealDefFun() );
                ast_.push_back( fun.stealWithDefFun() );
                ast_.push_back( new Ast::Apply() );  // apply definition to withdef program

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
            else if (c == '?')
            {
//                std::cerr << "found if-else\n";
                // ast_.push_back( new Ast::ConditionalApply() );
                mark.accept();
                try
                {
                    Program ifBranch( stream, pCurrentFun, pRecParsing );
                    Ast::Program* ifProg = new Ast::Program( ifBranch.ast() );
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
                        Program elseBranch( stream, pCurrentFun, pRecParsing );
                        elseProg = new Ast::Program( elseBranch.ast() );
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
                    Ast::PushFun* pRecFunForIdent = pRecParsing->FindPushFunForIdent(ident.name());
                    if (pRecFunForIdent)
                    {
                        ast_.push_back( new Ast::PushFunctionRec( pRecFunForIdent) );
                        bFoundRecFunId = true;
                    }
                }

                if (!bFoundRecFunId)
                {
                    const NthParent upvalNumber = pCurrentFun->FindBinding( ident.name() );
                
                    if (upvalNumber == kNoParent)
                    {
                        std::cerr << "Could not find binding for var=" << ident.name() << " fun=" << (void*)pCurrentFun << std::endl;
                        throw std::runtime_error("Unbound variable");
                    }

//////                    char c = mark.getc();
                    // if "push value" followed by "drop", simply dont push.
                    // Maybe this syntax allows to specify available upvalues for lookup on record/object system.
                    // Even without object system it's still an optimization
                    // of a pointless operation.
//////                    if (c != '~') 
                    {
//////                        mark.regurg(c);
                        ast_.push_back( new Ast::PushUpval(ident.name(), upvalNumber) );
                    }
                }
                mark.accept();
            }
            catch( const ErrorNoMatch& )
            {
                std::cerr << "Cannot parse at '" << c << "'" << std::endl;
                throw std::runtime_error("unparseable token");
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

    Ast::Program* parseToProgram( RegurgeIo& stream, bool bDump, const Ast::PushFun* self )
    {
        StreamMark mark(stream);

        try
        {
            Parser parser( mark, self );

            Ast::Program* p = new Ast::Program( parser.astProgram() );
            
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
            std::cerr << "Runtime parse error: " << e.what() << std::endl;
            throw e;
        }

        return nullptr;
    }
    
    Ast::PushFun* parseNoParent( RegurgeIo& stream, bool bDump )
    {
        Ast::PushFun* fun = new Ast::PushFun(nullptr);

        Ast::Program* pProgram = parseToProgram( stream, bDump, fun );

        fun->setAst( *pProgram );

        return fun;
    }

    BANGCLOSURE parseToClosureNoParent(Stack& stack, bool bDump )
    {
        Ast::PushFun* fun;

        if (stdin_)
        {
            RegurgeStdinRepl strmStdin;
            fun = this->parseNoParent( strmStdin, bDump );
        }
        else
        {
            FILE* fstream = fopen( fileName_.c_str(), "r");
            RegurgeFile strmFile( fstream );
            fun = this->parseNoParent( strmFile, bDump );
            fclose( fstream );
        }

        BANGCLOSURE noParentClosure(nullptr);
        auto closure = NEW_CLOSURE(*fun, noParentClosure);

        return closure;
    }
    
    void parseAndRun(Stack& stack, bool bDump)
    {
        try
        {
            auto closure = parseToClosureNoParent( stack, bDump );
            try
            {
                closure->apply( stack, closure );
            }
            catch( AstExecFail ast )
            {
                gFailedAst = ast.pStep;
                closure->getProgram()->dump( 0, std::cerr );
                std::cerr << "Runtime AST exec Error" << gFailedAst << ": " << ast.e.what() << std::endl;
            }
        }
        catch( const std::exception& e )
        {
            std::cerr << "Runtime exec Error: " << e.what() << std::endl;
        }
    }
}; // end, class RequireKeyword

void repl_prompt()
{
    std::cout << "Bang! " << std::flush;
}

void Ast::EofMarker::run( Stack& stack, const RunContext& rc ) const
{
//    std::cerr << "Found EOF!" << std::endl;

    stack.dump( std::cout );
    repl_prompt();
    
    CLOSURE_CREF self = rc.running();
    RequireKeyword req(nullptr);
    RegurgeStdinRepl strmStdin;
    try {
        Ast::Program *pProgram = req.parseToProgram( strmStdin, gDumpMode, self->pushfun() );
        RunProgram( stack, self, pProgram );
    } catch (const std::exception& e ) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
    


void Ast::Require::run( Stack& stack, const RunContext& rc ) const
{
    const auto& v = stack.pop(); // get file name
    if (!v.isstr())
        throw std::runtime_error("no filename found for require??");

    RequireKeyword me( v.tostr().c_str() );
    BANGCLOSURE noFunction(nullptr);
    const auto& closure = me.parseToClosureNoParent( stack, false );
    stack.push( STATIC_CAST_TO_BANGFUN(closure) );
    // auto newfun = std::make_shared<FunctionRequire>( s.tostr() );
}
    
} // end namespace Bang



/*
Generate test output:

  dir test\*.bang | %{ .\bang $_ | out-file -encoding ascii .\test\$("out." + $_.name + ".out") }

Test against reference output:

  dir test\*.bang | %{ $t=.\bang $_;  $ref = cat .\test\$("out." + $_.name + ".out"); if (compare-object $t $ref) { throw "FAILED $($_.name)!" } }  
 */
int main( int argc, char* argv[] )
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
        // bDump = true;
        fname = nullptr; // kDefaultScript;
//        fname = kDefaultScript;
        gReplMode = true;
    }

    gDumpMode = bDump;

    Bang::Stack stack;

    if (gReplMode)
        Bang::repl_prompt();
    
    do
    {
        try
        {
            Bang::RequireKeyword requireMain( fname );
            requireMain.parseAndRun( stack, bDump );
            stack.dump( std::cout );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    while (gReplMode);
    
    std::cerr << "toodaloo!" << std::endl;
}

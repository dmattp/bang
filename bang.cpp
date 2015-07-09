/////////////////////////////////////////////////////////////////
// Bang! language interpreter
// (C) 2015 David M. Placek
// All rights reserved, see accompanying [license.txt] file
//////////////////////////////////////////////////////////////////

#define HAVE_MUTATION 1  // enable '|>' operator; boo

#if HAVE_MUTATION
# if __GNUC__
# warning Mutation enabled: You have strayed from the true and blessed path.
#endif 
#endif
#define HAVE_DOT_OPERATOR 1
#define DOT_OPERATOR_INLINE 1

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
#include <algorithm>
#include <iterator>
#include <map>

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


// #define kDefaultScript "c:/m/n2proj/bang/tmp/coro1.bang";
//#define kDefaultScript "c:/m/n2proj/bang/tmp/cbad1.bang";
// #define kDefaultScript "c:/m/n2proj/bang/test/prog-01-quicksort.bang";

//~~~temporary #define for refactoring
#define TMPFACT_PROG_TO_RUNPROG(p) &((p)->getAst()->front())

#include "bang.h"

namespace {
    
    bool gDumpMode(false);
    template <class E = std::runtime_error>
    class ebuild
    {
        std::ostringstream oss;
    public:
        template<class T> ebuild& operator <<( const T & t )
        {
            oss << t;
            return *this;
        }
        ebuild() { }
        // template <class T> ebuild( T&& t ) : E( std::forward(t) ) {}
        
        void throw_()
        {
            throw E( oss.str() );
        }
    };
#define bangerr(...) for ( ebuild<__VA_ARGS__> err; true; err.throw_() ) err



    
    
    
}

namespace Bang {

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
SimpleAllocator< Upvalue > gUpvalAlloc;
#endif 

DLLEXPORT void GCDellocator<Upvalue>::freemem(Upvalue* p)
{
    gUpvalAlloc.deallocate(p, sizeof(Upvalue));
}
    
        
#if !USE_GC    
#define NEW_UPVAL(c,p,v) gcptr<Upvalue>( new (gUpvalAlloc.allocate(sizeof(Upvalue))) Upvalue( c, p, v ) )
#endif

    

    std::map<std::string,bangstring> gProgStrings;
    typedef std::pair<std::string,bangstring> itsval_t;
    bangstring internstring( const std::string& s )
    {
        if (s.length() > 32)
            return bangstring(s);
        
        auto it_interned = gProgStrings.find(s);
        if (it_interned == gProgStrings.end())
        {
            bangstring newbs( s );
            itsval_t pair( s, newbs );
            gProgStrings.insert( pair );
            return newbs;
        }
        else
        {
            return it_interned->second;
        }
    }



    static const NthParent kNoParent=NthParent(INT_MAX);


    namespace Ast { class CloseValue; }



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
        {
            o << this->tostr(); //  << " :" << std::hex << this->tostr().gethash() << std::dec;
        }
        else if (type_ == kFun)
        {
            auto f = this->tofun().get();
            o << "<cfun="
              << this->tofun().get()
              << ' '
              << typeid(*f).name() // dynamic type information is nice here
              << '>';
        }
        else if (type_ == kBoundFun)
            o << "<bangfun=" << this->toboundfun()->program_ << ">";
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

    DLLEXPORT void Stack::dump( std::ostream& o ) const
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
        double v2 = s.loc_top().tonum();
        s.pop_back();
        Value& v1 = s.loc_topMutate();

//        if (v1.isnum() && v2.isnum())
        {
            v1.mutateNoType( operation( v1.tonum(), v2 ) );
        }
//         else
//             throw std::runtime_error("Binary operator incompatible types");
    }

    template <class T>
    void infix2to1bool( Stack& s, T operation )
    {
        double v2 = s.loc_top().tonum();
        s.pop_back();
        Value& v1 = s.loc_topMutate();

//        if (v1.isnum() && v2.isnum())
        {
            v1.mutatePrimitiveToBool( operation( v1.tonum(), v2 ) );
        }
//         else
//             throw std::runtime_error("Binary operator.b incompatible types");
    }
    
    void plus ( Stack& s, const RunContext& )
    {
//        if (s.loc_top().isnum())
        if (s.loc_top().isstr())
        {
            bangstring v2 = s.loc_top().tostr();
            s.pop_back();
            bangstring v1 = s.pop().tostr();
            s.push( v1 + v2 );
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

    void format2ostream(  Stack& s, std::ostream& strout )
    {
        const Value& vfmt = s.pop();
        const std::string fmt = vfmt.tostr();
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
                    strout << fmt;
                else    
                    strout << fmt.substr( 0, pos + 1 );
            }
            else
            {
                const Value& v = s.pop();
                if (found > 0)
                    subpar( found - 1 );
                v.tostring(strout); //  << v.tostring(); // "XXX";
                size_t afterParam = found + 2;
                if (pos==std::string::npos)
                    strout << fmt.substr( afterParam ); //
                else
                    strout << fmt.substr( afterParam, pos - afterParam + 1 );
            }
        };

        subpar( std::string::npos );
    }

    void print( Stack& s, const RunContext& ctx )
    {
        format2ostream( s, std::cout );
    }

    void format( Stack& s, const RunContext& ctx )
    {
        std::ostringstream oss;
        format2ostream( s, oss );
        s.push( oss.str() );
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

    class CloseValue : public Base
    {
        const CloseValue* pUpvalParent_;
        bangstring paramName_;
    public:
        CloseValue( const CloseValue* parent, const bangstring& name )
        : Base( kCloseValue ),
          pUpvalParent_( parent ),
          paramName_( name )
        {}
        const bangstring& valueName() const { return paramName_; }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "CloseValue(" << paramName_ << ") " << std::hex << PtrToHash(this) << std::dec << "\n";
        }
        
        const NthParent FindBinding( const bangstring& varName ) const
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

        bool Upvalue::binds( const bangstring& name ) const
        {
            return closer_->valueName() == name;
        }
    
    const Value& Upvalue::getUpValue( const bangstring& uvName ) const
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
                bangerr() << "Could not find dynamic upval=\"" << uvName << "\"\n";
                // throw std::runtime_error("Could not find dynamic upval");
            }
        }
    }
    

const Ast::Base* gFailedAst = nullptr;


    class StreamMark;

    class ImportParsingContext : public ParsingContext
    {
        ParsingContext& parsecontext_;
        StreamMark& stream_;
        const Ast::Program* parent_;
        const Ast::CloseValue* upvalueChain_;
        void* pRecParsing_; /* ParsingRecursiveFunStack */
    public:
        ImportParsingContext
        (   ParsingContext& parsecontext,
            StreamMark& stream,
            const Ast::Program* parent,
            const Ast::CloseValue* upvalueChain,
            void* pRecParsing
        ) :
        parsecontext_(parsecontext), stream_(stream),
        parent_(parent), upvalueChain_( upvalueChain ),
        pRecParsing_( pRecParsing )
        {
        }
        virtual Ast::Base* hitEof( const Ast::CloseValue* uvchain );
    };
    

    
namespace Ast
{
    DLLEXPORT void BreakProg::dump( int level, std::ostream& o ) const
    {
        indentlevel(level, o);
        o << "---\n";
    }
    DLLEXPORT void EofMarker::repl_prompt( Stack& stack ) const
    {
        stack.dump( std::cout );
        parsectx_.interact.repl_prompt();
    }
    DLLEXPORT EofMarker::EofMarker( ParsingContext& ctx )
    : Base( kEofMarker ),
      parsectx_(ctx)
    {}
    DLLEXPORT void EofMarker::dump( int level, std::ostream& o ) const
    {
        indentlevel(level, o);
        o << "<<< EOF >>>\n";
    }
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

    class ApplyDotOperator : public Base
    {
    public:
        Value msgStr_; // method
        ApplyDotOperator( const bangstring& msgStr )
        :
#if DOT_OPERATOR_INLINE
        Base( kApplyDotOperator ),
#endif 
        msgStr_( msgStr )
        {}
        const Value& getMsgVal() const { return msgStr_; }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyDotOperator(" << msgStr_.tostr() << ")\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const
#if DOT_OPERATOR_INLINE
        { throw std::runtime_error("ApplyDotOperator::run should not be called"); }
#else
        ;
#endif 
    };

    class ApplyDotOperatorUpval : public Base
    {
    public:
        Value msgStr_; // method
    private:
        NthParent uvnumber_; // index into the active Upvalues
    public:
        ApplyDotOperatorUpval( const bangstring& msgStr, NthParent uvnumber )
        :
#if DOT_OPERATOR_INLINE
        Base( kApplyDotOperatorUpval ),
#endif 
        msgStr_( msgStr ),
        uvnumber_( uvnumber )
        {}
        const Value& getMsgVal() const { return msgStr_; }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyDotOperatorUpval(" << uvnumber_.toint() << '/' << msgStr_.tostr() << ")\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const
#if DOT_OPERATOR_INLINE
        { throw std::runtime_error("ApplyDotOperatorUpval::run should not be called"); }
#else
        ;
#endif
        const Value& getUpValue( const RunContext& rc ) const
        {
            return rc.getUpValue( uvnumber_ );
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
        bool xferstack_;
    public:
        YieldCoroutine(bool xferstack )
        : Base( kYieldCoroutine ),
          xferstack_( xferstack )
        {}

        bool shouldXferstack() const { return xferstack_; }

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
    public:
        NthParent uvnumber_; // index into the active Upvalues
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
        // ParsingContext& parsectx_;
    public:
        Require() {} // ParsingContext& parsectx) : parsectx_( parsectx ) {}
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

namespace {
    SHAREDUPVALUE replace_upvalue( SHAREDUPVALUE_CREF tail, const std::string& varname, const Value& newval )
    {
        if (tail->binds(varname))
        {
            return NEW_UPVAL( tail->upvalParseChain(), tail->nthParent(NthParent(1)), newval );
        }
        else
        {
            SHAREDUPVALUE_CREF currparent = tail->nthParent( NthParent(1) );
            
            if (!currparent)
                throw std::runtime_error("rebind-fun: could not find upvalue="+varname);
            
            SHAREDUPVALUE_CREF newparent = replace_upvalue( currparent, varname, newval );
            
            return NEW_UPVAL( tail->upvalParseChain(), newparent, tail->getUpValue(NthParent(0)));
        }
    }
}


namespace Primitives {
    void savestack( Stack& s, const RunContext& rc )
    {
        const auto& restoreFunction = NEW_BANGFUN(FunctionRestoreStack)( s );
        s.push( STATIC_CAST_TO_BANGFUN(restoreFunction) );
    }
    void rebindFunction( Stack& s, const RunContext& rc )
    {
        const Value& vbindname = s.pop();
        const Value& v = s.pop();
        if (!v.isboundfun())
            throw std::runtime_error("rebind-fun: not a bound function");
        auto bprog = v.toboundfun();
        const auto& bindname = vbindname.tostr();
        const Value& newval = s.pop();

        SHAREDUPVALUE newchain = replace_upvalue( bprog->upvalues_, bindname, newval );

        const auto& newfun = NEW_BANGFUN(BoundProgram)( bprog->program_, newchain );
        s.push( newfun );
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


    void Thread::setcallin(Thread*caller)
    {
        if (!callframe)
        {
            // maybe this should just be done in constructor.
            
            // start me off pointing at the BoundProgram, ready to roll
            // no dynamic cast for quickness. responsibility of caller to insure passed Function is a BoundFunction.
            BoundProgram* pbound = reinterpret_cast<BoundProgram*>( boundProg_.get() );

            this->callframe =
                new (gAllocRc.allocate(sizeof(RunContext)))
                RunContext( this, TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
        }

        //~~~ i think this is not quite right.  what if i'm called multiple times from separate threads;
        // then yield out to intermediate callers; when i'm done, who do i return to?  I shouldn't return
        // to the last caller (to whom I returned control via yield), but maybe there should be a caller
        // stack or something, such that when I yield I pop off the last caller.  Or no, maybe it shouldn't
        // matter; because I always return to the last person who resumed me, so I think this is correct.
        // perhaps there should be something to e.g., set pCaller to nullptr after I yield back to that
        // caller at least.
        pCaller = caller;
    }
    
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
    
void xferstack( Thread* from, Thread* to )
{
    from->stack.giveTo( to->stack );
}

static Thread gNullThread;
DLLEXPORT Thread* pNullThread( &gNullThread );
DLLEXPORT Thread* Thread::nullthread() { return &gNullThread; }
    
DLLEXPORT void RunProgram
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

//                dobreakprog:
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
                    {
//                        std::cerr << "exiting prog stack=" << &(pThread->stack) <<  " stksize=" << pThread->stack.size() << "\n";
                        return;
                    }
                }

                case Ast::Base::kYieldCoroutine:
                    if (pThread->pCaller)
                    {
                        if (reinterpret_cast<const Ast::YieldCoroutine*>(pInstr)->shouldXferstack())
                            xferstack( pThread, pThread->pCaller );
                        pThread = pThread->pCaller;
                        goto restartThread;
                    }
                    else
                        return; // nowhere to go

                

                case Ast::Base::kEofMarker:
                {
                    auto pEof = reinterpret_cast<const Ast::EofMarker*>(pInstr);
                    pEof->repl_prompt(stack);

                    while (true)
                    {
                        try
                        {
                            SHAREDUPVALUE_CREF uv = frame.upvalues_;
                            Ast::Program *pProgram = pEof->getNextProgram( uv ); //
                            if (pProgram)
                            {
                                frame.rebind( TMPFACT_PROG_TO_RUNPROG(pProgram) );
                                //~~~ okay this is tricky, because if there's an execute error on a REPL, I'd
                                // really like to go back to the same context (preserve upvalues) but I sort
                                // of lose that information if I'm doing TCO, which I'd like to do because don't
                                // burn stack frames in the REPL.  I almost have to add the upvalue chain to the
                                // exception or something.  Or preserve the upvalue prior to reading the
                                // next line, ie, if I close values in the line, and then it crashes, do I have
                                // those values in the chain?  I guess I probably should, since the stack is modified.
                                // so whatever the uv chain is at the time of the crash should be preserved for the
                                // next statement
                                goto restartTco;
                            }
                            else
                            {
                                // std::cerr << "doing break prog\n";
                                // should i just return here, like yield does?  Does it matter?
                                // goto dobreakprog;
                                // return seems to work better, not sure i understand it 100% yay.
                                // hitting ; to break scope blows up hmm
                                return;
                            }
                        }
                        catch (const std::exception& e ) // parse error
                        {
                            //~~~ @todo: shouldn't I dump this here?  reportError or something?
                            pEof->repl_prompt(stack);
                        }
                    }
                }
                break;

                case Ast::Base:: kMakeCoroutine:
                    Ast::MakeCoroutine::go( stack, pThread );
                    break;

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

#define KTHREAD_CASE \
                case Value::kThread: { auto other = v.tothread().get(); other->setcallin(pThread); xferstack(pThread,other); pThread = other; goto restartThread; }
                
                /* 150629 Coroutine issue:  Currently, coroutine yield returns to creating thread, not calling thread. */
                case Ast::Base::kTCOApplyUpval:
                {
                    const Value& v = reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( frame );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        KTHREAD_CASE
                        case Value::kBoundFun:
                        {
                            auto pbound = v.toboundfunhold();
                            frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                            goto restartTco;
                        }
                
                    }
                }
                break;
                
                /* 150629 Coroutine issue:  Currently, coroutine yield returns to creating thread, not calling thread. */
                case Ast::Base::kApplyUpval:
                {
                    const Value& v = reinterpret_cast<const Ast::ApplyUpval*>(pInstr)->getUpValue( frame );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        KTHREAD_CASE
                        case Value::kBoundFun:
                            auto pbound = v.toboundfunhold();
                            inprog = pbound->program_;
                            inupvalues = pbound->upvalues_;
                            goto restartNonTail;
                    }
                }
                break;

#if DOT_OPERATOR_INLINE
                case Ast::Base::kApplyDotOperator:
                {
                    const Value& v = stack.pop(); // the function to apply
                    stack.push( reinterpret_cast<const Ast::ApplyDotOperator*>(pInstr)->getMsgVal() );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        KTHREAD_CASE
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            inprog = pbound->program_;
                            inupvalues = pbound->upvalues_;
                            goto restartNonTail;
                    }
                }
                break;
                case Ast::Base::kApplyDotOperatorUpval:
                {
                    auto adoup = reinterpret_cast<const Ast::ApplyDotOperatorUpval*>(pInstr);
                    const Value& v = adoup->getUpValue( frame ); 
                    stack.push( adoup->getMsgVal() );
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        KTHREAD_CASE
                        case Value::kBoundFun:
                            BoundProgram* pbound = v.toboundfun();
                            inprog = pbound->program_;
                            inupvalues = pbound->upvalues_;
                            goto restartNonTail;
                    }
                }
                break;

#endif 
                
                case Ast::Base::kTCOApply:
                {
                    const Value& v = stack.pop();
                    switch (v.type())
                    {
                        default: RunApplyValue( v, stack, frame ); break;
                        //~~~ should this setcallin? as with KTHREAD_CASE? or is it intentionally different
                        case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                        case Value::kBoundFun:
                            auto pbound = v.toboundfunhold();
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
                        KTHREAD_CASE    
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
        throw AstExecFail( *(frame.ppInstr), e.what() );
    }
}




    DLLEXPORT void CallIntoSuspendedCoroutine( Bang::Thread *bthread, const BoundProgram* bprog )
    {
        // this is an awful mess.
        auto prevcaller = bthread->pCaller;
        auto prevcf = bthread->callframe;
        bthread->pCaller = nullptr;
        bthread->callframe = nullptr;
        // auto bprog = v->toboundfun(); 
        Bang::RunProgram( bthread, bprog->program_, bprog->upvalues_ );
        bthread->pCaller = prevcaller;
        bthread->callframe = prevcf;
    }

#if !DOT_OPERATOR_INLINE
    void Ast::ApplyDotOperator::run( Stack& stack, const RunContext& ctx ) const
        {
            // now apply
            const Value& v = stack.pop(); // the function to apply
            stack.push( v_ );
            switch (v.type())
            {
                case Value::kFun: v.tofun()->apply(stack); break;
                case Value::kFunPrimitive: v.tofunprim()( stack, ctx ); break;
                case Value::kBoundFun:
                {
                    BoundProgram* bprog = v.toboundfun();
//                    std::cerr << "dot operator calling boundfun upval1=" << bprog->upvalues_->upvalParseChain()->valueName() << "\n";
                    // RunProgram( ctx.thread, bprog->program_, bprog->upvalues_ );
                    CallIntoSuspendedCoroutine( ctx.thread, bprog );
//                    frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                }
                break;
//                case Value::kThread: { auto other = v.tothread().get(); other->setcallin(pThread);
//                xferstack(pThread,other); pThread = other; goto restartThread; }
                default:
                    throw std::runtime_error("dot operator not supported on value type");
            }
        }
#endif 
    
    
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

const Value& RunContext::getUpValue( const bangstring& uvName ) const
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
    
    auto thread = NEW_BANGTHREAD( v.tofun() );

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
    if (!stack.loc_top().isstr())
    {
        throw std::runtime_error("PushUpvalByName (lookup) name is not string" );
    }

    bangstring uvname = stack.poptostr();
    stack.push( rc.getUpValue( uvname ) );
}



bool iseof(int c)
{
    return c == EOF;
}

struct ErrorNoMatch
{
    ErrorNoMatch() {}
};

    int RegurgeIo::getcud()
    {
        char rv;
        
        if (regurg_.empty())
            return EOF;

        rv = regurg_.back();
        regurg_.pop_back();

        return rv;
    }
    

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
}; // end, class StreamMark



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
        Program( ParsingContext& pc, StreamMark&, const Ast::Program* parent, const Ast::CloseValue* upvalueChain,
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
                            default:
                                //*bi++ = '\\';
                                cstr = c;
                                break;
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
//                std::cerr << "ParseLiteral string:"; value_.tostring(std::cerr); std::cerr << "\n";
                
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

    static const Ast::CloseValue* getParamBindings
    (   StreamMark& mark,
        Ast::Program::astList_t& functionAst,
        const Ast::CloseValue* upvalueChain
    )
    {
        std::vector< std::string > params;
        while (true)
        {
            try
            {
                Identifier param(mark);
                eatwhitespace(mark);
                params.push_back( param.name() );
                mark.accept();
            }
            catch ( const ErrorNoMatch& ) // identifier is optional
            {
                break; 
            }
        }

        while (params.size() > 0)
        {
            auto cv = new Ast::CloseValue( upvalueChain, internstring(params.back()) );
            functionAst.push_back( cv );
            upvalueChain = cv;
            params.pop_back();
        }
            

        char c = mark.getc();
        if (c != '=') // this seems unneccesary sometimes. therefore "as"
        {
            bangerr(ParseFail) << "got '" << c << "', expecting '=' in "
                               << mark.sayWhere() << ": function def / param list must be followed by '='"; 
        }

        return upvalueChain;            
    }
    
    
    class Fundef
    {
        bool postApply_;
        Ast::Program* pNewProgram_;
    public:
        ~Fundef() { delete pNewProgram_; }
        
        Fundef( ParsingContext& parsectx, StreamMark& stream,
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
            upvalueChain = getParamBindings( mark, functionAst, upvalueChain );

            //~~~ programParent??
            Program program( parsectx, mark, nullptr, upvalueChain, pRecParsing );
            const auto& subast = program.ast();
            std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );
            pNewProgram_ = new Ast::Program( nullptr, functionAst );

            mark.accept();
        }
        bool hasPostApply() const { return postApply_; }
        Ast::Program* stealProgram() { auto rc = pNewProgram_; pNewProgram_ = nullptr; return rc; }
    }; // end, Fundef class

    class Defdef
    {
        bool postApply_;
        Ast::Program* pDefProg_;
        std::unique_ptr<std::string> defname_;
    public:
        ~Defdef() { delete pDefProg_; } // delete pWithDefFun_; 
        Defdef( ParsingContext& parsectx, StreamMark& stream, const Ast::CloseValue* upvalueChain, ParsingRecursiveFunStack* pRecParsing )
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
                bangerr(ParseFail) << "got '" << c << "' expecting ':' in "
                                   << mark.sayWhere() << " - def name must start with ':'";
            }

            try
            {
                Identifier param(mark);
                eatwhitespace(mark);
                defname_ = std::unique_ptr<std::string>(new std::string(param.name()));
            }
            catch ( const ErrorNoMatch& )
            {
                bangerr(ParseFail) << "in " << mark.sayWhere() << "identifier must follow \"def :\"";
            }

            Ast::Program::astList_t functionAst;
            upvalueChain = getParamBindings( mark, functionAst, upvalueChain );

            // std::string defFunName = pWithDefFun_->getParamName();
            pDefProg_ = new Ast::Program(nullptr);
            ParsingRecursiveFunStack recursiveStack( pRecParsing, pDefProg_, lastParentUpvalue, *defname_ ); // , pWithDefFun_);
            Program progdef( parsectx, mark, nullptr, upvalueChain, &recursiveStack ); // pDefFun_, &recursiveStack );
            const auto& subast = progdef.ast();
            std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );
            pDefProg_->setAst( functionAst );
            mark.accept();
        }

        bool hasPostApply() const { return postApply_; }
        Ast::Program* stealDefProg() { auto rc = pDefProg_; pDefProg_ = nullptr; return rc; }
        const std::string& getDefName() { return *defname_; }
    }; // end, Defdef class
    
    Program* program_;
public:
    Parser( ParsingContext& ctx, StreamMark& mark, const Ast::Program* parent )
    {
        program_ = new Program( ctx, mark, parent, nullptr, nullptr ); // 0,0,0 = no upvalues, no recursive chain
    }

    Parser( ParsingContext& ctx, StreamMark& mark, const Ast::CloseValue* upvalchain )
    {
        program_ = new Program( ctx, mark, nullptr /*parent*/, upvalchain, nullptr ); // 0,0,0 = no upvalues, no recursive chain
    }
    
    const Ast::Program::astList_t& programAst()
    {
        return program_->ast();
    }
};

    Ast::Base* ImportParsingContext::hitEof( const Ast::CloseValue* uvchain )
    {
//        std::cerr << "import hit eof! (continue next prog)\n";

        Parser p( parsecontext_, stream_, uvchain );

        Ast::Program pushprog( parent_, p.programAst() );
        return new Ast::ApplyProgram( &pushprog );
    }
    

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
        const Ast::PushUpval* pup = dynamic_cast<const Ast::PushUpval*>(ast[i]);
        if (pup)
        {
            Ast::ApplyDotOperator* pdot = dynamic_cast<Ast::ApplyDotOperator*>(ast[i+1]);
            if (pdot)
            {
                ast[i] = new Ast::ApplyDotOperatorUpval( pdot->msgStr_.tostr(), pup->uvnumber_ );
                ast[i+1] = &noop;
            }
        }
    }

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
            bangerr() << "Could not load library=" << libname;
        }
        auto proc = GetProcAddress( hlib, "bang_open" );
        if (!proc)
        {
            bangerr() << "Could not find 'bang_open()' in library=" << libname;
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


    DLLEXPORT
    Ast::Program* ParseToProgram
    (   ParsingContext& parsectx,
        RegurgeStream& stream,
        bool bDump,
        const Ast::CloseValue* upvalchain
    )
    {
        StreamMark mark(stream);

        try
        {
            Parser parser( parsectx, mark, upvalchain );

            Ast::Program* p = new Ast::Program( nullptr /* parent */, parser.programAst() );
            
            if (bDump)
                p->dump( 0, std::cerr );

            return p;
        }
        catch (const ErrorEof& )
        {
            bangerr() << "Found EOF without valid program!";
        }
        catch (const std::runtime_error& e )
        {
            bangerr() << "Parsing: " << e.what();
        }

        return nullptr;
    }

    //~~~ okay, why parse to BoundProgram if we know there are no upvals?  Why not just
    // parse to Ast::Program and go with that?
    DLLEXPORT Ast::Program* RequireKeyword::parseToProgramNoUpvals( ParsingContext& ctx, bool bDump )
    {
        Ast::Program* fun;

//         if (stdin_)
//         {
//             RegurgeStdinRepl strmStdin;
//             fun = ParseToProgram( ctx, strmStdin, bDump, nullptr );
//         }
//         else
        {
            RegurgeFile strmFile( fileName_ );
            try
            {
                fun = ParseToProgram( ctx, strmFile, bDump, nullptr );
            }
            catch( const ParseFail& e )
            {
                bangerr(ParseFail) << "e01a Parsing: " <<  e.what();
            }
        }

        return fun;
    }
    

//     //~~~ okay, why parse to BoundProgram if we know there are no upvals?  Why not just
//     // parse to Ast::Program and go with that?
//     std::shared_ptr<BoundProgram> RequireKeyword::parseToBoundProgramNoUpvals( ParsingContext& ctx, bool bDump )
//     {
//         Ast::Program* fun = this->parseToProgramNoUpvals( ctx, bDump );

//         // BANGCLOSURE noParentClosure(nullptr);
//         SHAREDUPVALUE noUpvals;
//         const auto& boundprog = NEW_BANGFUN(BoundProgram)( fun, noUpvals );

//         return boundprog;
//     }

    

Parser::Program::Program
(   ParsingContext& parsecontext,
    StreamMark& stream,
    const Ast::Program* parent,
    const Ast::CloseValue* upvalueChain,
    ParsingRecursiveFunStack* pRecParsing
)
{
//    std::cerr << "enter Program parent=" << parent << " uvchain=" << upvalueChain << "\n";
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
//                std::cerr << "got literal:"; aLiteral.value().tostring(std::cerr); std::cerr << "\n";
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
                        Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, internstring(valueName.name()) );
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
                Fundef fun( parsecontext, stream, upvalueChain, pRecParsing );

                ast_.push_back( fun.stealProgram() );

                if (fun.hasPostApply())
                    ast_.push_back( new Ast::Apply() );

                continue;
            } catch ( const ErrorNoMatch& ) {}

            try
            {
                Defdef fun( parsecontext, stream, upvalueChain, pRecParsing );
                
                ast_.push_back( fun.stealDefProg() );
                
                if (fun.hasPostApply())
                    ast_.push_back( new Ast::Apply() );
                else
                {
                    Ast::CloseValue* cv = new Ast::CloseValue( upvalueChain, internstring(fun.getDefName()) );
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
                    Program ifBranch( parsecontext, stream, nullptr, upvalueChain, pRecParsing );
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
                        Program elseBranch( parsecontext, stream, nullptr, upvalueChain, pRecParsing );
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

#if HAVE_DOT_OPERATOR
                    ast_.push_back( new Ast::ApplyDotOperator( internstring(methodName.name())) );
#else
                    ast_.push_back( new Ast::PushLiteral( Value(methodName.name())) );                    
                    ast_.push_back( new Ast::PushPrimitive( &Primitives::swap, "swap" ) );
                       ast_.push_back( new Ast::Apply() ); // haveto apply the swap
                    ast_.push_back( new Ast::Apply() ); // now apply to the object!!
#endif 
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

                if (ident.name() == "yield-nil")
                {
                    if (mark.getc()=='!')
                    {
                        mark.accept();
                        ast_.push_back( new Ast::YieldCoroutine(false) );
                        continue;
                    }
                }
                
                if (ident.name() == "yield")
                {
                    if (mark.getc()=='!')
                    {
                        mark.accept();
                        ast_.push_back( new Ast::YieldCoroutine(true) );
                        continue;
                    }
                }
                
                if (ident.name() == "require")
                {
                    mark.accept();
                    ast_.push_back( new Ast::Require() );
                    continue;
                }

                if (ident.name() == "import")
                {
                    mark.accept();
                    Ast::Base* prev = ast_.back();
                    ast_.pop_back();
                    Ast::PushLiteral* who = dynamic_cast<Ast::PushLiteral*>(prev);
                    if (!who || !who->v_.isstr())
                        throw std::runtime_error("import requires a preceding string");
                    ImportParsingContext importContext
                    (   parsecontext, stream,
                        parent, upvalueChain,
                        pRecParsing
                    );
                    RequireKeyword requireImport( who->v_.tostr().c_str() );
                    delete prev;
                    // bah, really need to pass in parent's upvalue chain here
                    auto prog = requireImport.parseToProgramNoUpvals( importContext, gDumpMode ); // DUMP
                    //~~~ bah, a mess
                    auto importAst = prog->getAst();
                    std::copy( importAst->begin(), importAst->end(), std::back_inserter(ast_));
                    // ast_.push_back( new Ast::Apply() );
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
                if (rwPrimitive( "format",   &Primitives::format   ) ) continue;
                if (rwPrimitive( "save-stack",    &Primitives::savestack    ) ) continue;
                if (rwPrimitive( "rebind",    &Primitives::rebindFunction    ) ) continue;
//                if (rwPrimitive( "stack-to-array",    &Primitives::stackToArray    ) ) continue;
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
                        bangerr(ParseFail) << "Could not find binding for var=" << ident.name() << " uvchain=" << (void*)upvalueChain 
                                           << " in " << mark.sayWhere();
                    }

                    ast_.push_back( new Ast::PushUpval(ident.name(), upvalNumber) );
                }
                
                mark.accept();
            }
            catch( const ErrorNoMatch& )
            {
                bangerr(ParseFail) << " in " << mark.sayWhere() << "unparseable token [" << c << "]";
            }
        } // end, while true

        stream.accept();
        ast_.push_back( new Ast::BreakProg() );
        OptimizeAst( ast_ );
    }
    catch (const ErrorEof&)
    {
        stream.accept();
        ast_.push_back( parsecontext.hitEof( upvalueChain ) );
        OptimizeAst( ast_ );
    }
}



    // Require keyword always reads full required file without returning control
    // to repl/interactive environment, so when EOF is found, we insert BreakProg
    class RequireParsingContext : public ParsingContext
    {
    public:
        RequireParsingContext() {}
        Ast::Base* hitEof( const Ast::CloseValue* uvchain )
        {
            return new Ast::BreakProg();
        }
    };


void Ast::Require::run( Stack& stack, const RunContext& rc ) const
{
    const auto& v = stack.pop(); // get file name
    if (!v.isstr())
        throw std::runtime_error("no filename found for require??");

    RequireKeyword me( v.tostr().c_str() );
    RequireParsingContext parsectx_;
    
    auto fun = me.parseToProgramNoUpvals( parsectx_, gDumpMode );
    SHAREDUPVALUE noUpvals;
    const auto& closure = NEW_BANGFUN(BoundProgram)( fun, noUpvals );

    stack.push( closure ); // STATIC_CAST_TO_BANGFUN(closure) );
    // auto newfun = std::make_shared<FunctionRequire>( s.tostr() );
}

} // end namespace Bang





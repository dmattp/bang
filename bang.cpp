/////////////////////////////////////////////////////////////////
// Bang! language interpreter
// (C) 2015 David M. Placek
// All rights reserved, see accompanying [license.txt] file
//////////////////////////////////////////////////////////////////

#define HAVE_MUTATION 0  // enable '|>' operator; boo

#if HAVE_MUTATION
# if __GNUC__
// "Making something variable is easy. Controlling duration of constancy is the trick" - Alan Perlis
# warning Mutation enabled: You have strayed from the true and blessed path.
#endif 
#endif
#define HAVE_DOT_OPERATOR 1
#define DOT_OPERATOR_INLINE 1

#define LCFG_KEEP_PROFILING_STATS 0

#define LCFG_OPTIMIZE_OPVV2V_WITHLIT 1
#define LCFG_USE_INDEX_OPERATOR 1

#define LCFG_HAVE_SAVE_BINDINGS 1 // deprecated;  favor ^bind and ^^bind

#define LCFG_HAVE_TAV_SWAP 0

// #define LCFG_TRYJIT 1

#define LCFG_INTLITERAL_OPTIMIZATION LCFG_TRYJIT

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
     ? -- conditional / if
     : -- conditional / else
     ~ /not      -- logical not
     + - * /     -- the usual suspects for numbers
     < > = <= >= -- comparison- less than, greater than, equal, lte, gte, ne
     =~ <>       -- not equal
     <~ >~       -- gte (less than not), lte (greater than not)
     #           -- get stack length
     /and /or    -- logical and / or
     . []        -- Index operator / Object message
 
  Delimiters
     ; -- close current program scope
     {} -- open / close matched program scope

And syntactic sugar


And there are primitives, until a better library / module system is in place.
     
  Primitives
    drop! swap! dup! nth! save-stack!
    floor! random! -- these really belong in a math library
    
  Literals
    true false
*/

#include <iostream>
#include <algorithm>
#include <iterator>
#include <map>
//#include <mutex> // for threadsafe upvalue allocator

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



#include "bang.h"


//~~~temporary #define for refactoring
#define TMPFACT_PROG_TO_RUNPROG(p) &((p)->getAst()->front())
#define FRIENDOF_RUNPROG friend DLLEXPORT void Bang::RunProgram( Thread* pThread, const Ast::Program* inprog, SHAREDUPVALUE inupvalues );
#define FRIENDOF_OPTIMIZE friend void Bang::OptimizeAst( std::vector<Ast::Base*>& ast, const Bang::Ast::CloseValue* upvalueChain );



#if LCFG_TRYJIT

extern "C" {
  #include "lightning.h"
}
static jit_state_t *_jit;

typedef double (*pifi)(double);    /* Pointer to Int Function of Int */

class MyJitter
{
public:
    MyJitter()
    {
        init_jit("/m/n2proj/bang//bang.exe");
    }
};

static MyJitter& initjitter()
{
    static MyJitter jitter;
    return jitter;
}

//int main(int argc, char *argv[])
pifi jitit_increment( double constval, Bang::EOperators eop )
{
  jit_node_t  *in;
  pifi         incr;

  initjitter();

  _jit = jit_new_state();
  jit_prolog();                    /*      prolog              */
  in = jit_arg_d();                  /*      in = arg            */
  jit_getarg_d(JIT_F0, in);          /*      getarg R0           */
  switch( eop ) {
      case Bang::kOpPlus: jit_addi_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpMinus: jit_subi_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpMult: jit_muli_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpDiv: jit_divi_d(JIT_F0, JIT_F0, constval);  break;
  }
  jit_retr_d(JIT_F0);                /*      retr   R0           */

  incr = reinterpret_cast<pifi>(jit_emit());
  jit_clear_state();

  /* call the generated code, passing 5 as an argument */
  //printf("%d + 1 = %d\n", 5, incr(5));

//  jit_destroy_state();
//  finish_jit();
  return incr;
}


typedef double (*pifi_up)(void* upvals ); // , void* thread );    /* Pointer to Int Function of Int */
typedef void (*pifi_up_void)(void* pthread, void* upvals ); // , void* thread );    /* Pointer to Int Function of Int */


pifi_up jitit_increment_uv0( double constval, Bang::EOperators eop, int uvnum, bool isreg )
{
  jit_node_t  *in;
//  jit_node_t  *in2;
  pifi_up         incr;

  initjitter();

  _jit = jit_new_state();
  jit_prolog();                    /*      prolog              */

  in = jit_arg();                  /*      in = arg            */
//  in2 = jit_arg();                  /*      in = arg            */

  jit_getarg(JIT_R0, in);          /*      getarg R0           */
//  jit_getarg(JIT_R1, in2);          /*      getarg R0           */

  if (isreg)
  {
      const size_t BVOffset = offsetof(Bang::Thread,r0_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
      jit_ldxi_d( JIT_F0, JIT_R0, BVOffset );
  }
  else
  {
      for ( ; uvnum > 0; --uvnum)
      {
//      std::cerr << "uvnum=" << uvnum << "\n";
          const size_t parentOffset = offsetof( Bang::Upvalue, parent_ );
          jit_ldxi( JIT_R0, JIT_R0, parentOffset );
      }
      const size_t BVOffset = offsetof(Bang::Upvalue,v_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
      jit_ldxi_d( JIT_F0, JIT_R0, BVOffset );
  }
  
  switch( eop ) {
      case Bang::kOpPlus: jit_addi_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpMinus: jit_subi_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpMult: jit_muli_d(JIT_F0, JIT_F0, constval);  break;
      case Bang::kOpDiv: jit_divi_d(JIT_F0, JIT_F0, constval);  break;
  }
  jit_retr_d(JIT_F0);                /*      retr   R0           */

  incr = reinterpret_cast<pifi_up>(jit_emit());

// // //   std::cout << "disassemble; reg=" << isreg << " uvnum=" << uvnum << "\n";
// // //   jit_disassemble();
// // //   std::cout << "=================================\n";
  
  jit_clear_state();

  /* call the generated code, passing 5 as an argument */
  //printf("%d + 1 = %d\n", 5, incr(5));

//  jit_destroy_state();
//  finish_jit();
  return incr;
}


class  Litnumop2reg
{
public:
    void addOperation( double constval, Bang::EOperators eop )
    {
        switch( eop ) {
            case Bang::kOpPlus: jit_addi_d(JIT_F0, JIT_F0, constval);  break;
            case Bang::kOpMinus: jit_subi_d(JIT_F0, JIT_F0, constval);  break;
            case Bang::kOpMult: jit_muli_d(JIT_F0, JIT_F0, constval);  break;
            case Bang::kOpDiv: jit_divi_d(JIT_F0, JIT_F0, constval);  break;
        }
    }
    void addReverseOperation( double constval, Bang::EOperators eop )
    {
        switch( eop ) {
            case Bang::kOpPlus:  jit_addi_d(JIT_F0, constval, JIT_F0);  break;
            case Bang::kOpMult:  jit_muli_d(JIT_F0, constval, JIT_F0);  break;
            case Bang::kOpMinus:
                jit_movi_d( JIT_F1, constval );
                jit_subr_d(JIT_F0, JIT_F1, JIT_F0);
                break;
            case Bang::kOpDiv:
                jit_movi_d( JIT_F1, constval );
                jit_divr_d(JIT_F0, JIT_F1, JIT_F0);
                break;
        }
    }

    void loadFromUpval( Bang::NthParent uvnumP )
    {
        const size_t parentOffset = offsetof( Bang::Upvalue, parent_ );
        jit_movr( JIT_R2, JIT_R1 );
        for ( int uvnum = uvnumP.toint() ; uvnum > 0; --uvnum)
        {
            jit_ldxi( JIT_R2, JIT_R2, parentOffset );
        }
        
        const size_t BVOffset = offsetof(Bang::Upvalue,v_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
        jit_ldxi_d( JIT_F0, JIT_R2, BVOffset );
    }
    
    void addUpvalOp( Bang::NthParent uvnumP, Bang::EOperators eop )
    {
        const size_t parentOffset = offsetof( Bang::Upvalue, parent_ );
        jit_movr( JIT_R2, JIT_R1 );
        for ( int uvnum = uvnumP.toint() ; uvnum > 0; --uvnum)
        {
            jit_ldxi( JIT_R2, JIT_R2, parentOffset );
        }
        
        const size_t BVOffset = offsetof(Bang::Upvalue,v_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
        jit_ldxi_d( JIT_F1, JIT_R2, BVOffset );
      
        switch( eop ) {
            case Bang::kOpPlus: jit_addr_d(JIT_F0, JIT_F0, JIT_F1);  break;
            case Bang::kOpMinus: jit_subr_d(JIT_F0, JIT_F0, JIT_F1);  break;
            case Bang::kOpMult: jit_mulr_d(JIT_F0, JIT_F0, JIT_F1);  break;
            case Bang::kOpDiv: jit_divr_d(JIT_F0, JIT_F0, JIT_F1);  break;
        }
    }
    void loadFromRegister()
    {
        const size_t BVOffset = offsetof(Bang::Thread,r0_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
        jit_ldxi_d( JIT_F0, JIT_R0, BVOffset );
    }

    // R0 is pthread, R1 is Upvalue*
    Litnumop2reg()
    {
        jit_node_t  *in;
        jit_node_t  *in2;
        initjitter();

        _jit = jit_new_state();
        jit_prolog();                    /*      prolog              */
        in = jit_arg();                  /*      in = arg            */
        in2 = jit_arg();                  /*      in = arg            */
        jit_getarg(JIT_R0, in);          /*      getarg R0           */
        jit_getarg(JIT_R1, in2);          /*      getarg R0           */

// // //         std::cout << "disassemble PROLOG AND ARGS\n";
// // //         jit_disassemble();
// // //         std::cout << "=================================\n";
// // //         
        //jit_retr_d(JIT_F0);                /*      retr   R0           */
    }

    pifi_up_void closeitup() // store to pThread->r0_.v_.num
    {
        const size_t BVOffset = offsetof(Bang::Thread,r0_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
        jit_stxi_d( BVOffset, JIT_R0, JIT_F0 );

        jit_ret();

        pifi_up_void incr = reinterpret_cast<pifi_up_void>(jit_emit());

// // //         std::cout << "disassemble; reg=" << isreg << " uvnum=" << uvnum << "\n";
// // //         jit_disassemble();
// // //         std::cout << "=================================\n";
// // //   
        jit_clear_state();

        /* call the generated code, passing 5 as an argument */
        //printf("%d + 1 = %d\n", 5, incr(5));

//  jit_destroy_state();
//  finish_jit();
        return incr;
        {
            const size_t BVOffset = offsetof(Bang::Thread,r0_.v_.num); //  ( &(((Bang::Upvalue*)0)->v_.v_.num) - &(*((Bang::Upvalue*)0)) );
            jit_stxi_d( BVOffset, JIT_R0, JIT_F0 );
        }

        jit_ret();

        incr = reinterpret_cast<pifi_up_void>(jit_emit());

// // //   std::cout << "disassemble; reg=" << isreg << " uvnum=" << uvnum << "\n";
// // //   jit_disassemble();
// // //   std::cout << "=================================\n";
  
        jit_clear_state();

        /* call the generated code, passing 5 as an argument */
        //printf("%d + 1 = %d\n", 5, incr(5));

//  jit_destroy_state();
//  finish_jit();
        return incr;
    }
};



pifi_up_void jitit_increment_uv0_2reg( double constval, Bang::EOperators eop, int uvnum, bool isreg )
{
}
#endif 

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

void OptimizeAst( std::vector<Bang::Ast::Base*>& ast, const Bang::Ast::CloseValue* upvalueChain );


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

#if LCFG_MT_SAFEISH    
template <class Tp>
struct SimplestAllocator
{
    typedef Tp value_type;
    SimplestAllocator(/*ctor args*/) {}
    template <class T> SimplestAllocator(const SimplestAllocator<T>& other){}
    template <class U> struct rebind { typedef SimplestAllocator<U> other; };
    
    Tp* allocate(std::size_t n)
    {
        Tp* mem = reinterpret_cast<Tp*>( ::operator new(n*sizeof(Tp)) );
        return mem;
    }
    void deallocate(Tp* p, std::size_t n)
    {
        ::operator delete(p);
    }
    void construct( Tp* p, const Tp& val ) { new (p) Tp(val); }
    void destroy(Tp* p) { p->~Tp(); }
};
#endif

#if !USE_GC    
#if LCFG_MT_SAFEISH    
SimplestAllocator< Upvalue > gUpvalAlloc;
#else
SimpleAllocator< Upvalue > gUpvalAlloc;
#endif 
#endif
    

        
#if USE_GC
# define NEW_UPVAL new Upvalue
#elif LCFG_GCPTR_STD    
# define NEW_UPVAL(c,p,v) std::allocate_shared<Upvalue>( gUpvalAlloc, c, p, v )
#elif LCFG_UPVAL_SIMPLEALLOC
    
# if 1
    # define NEW_UPVAL(c,p,v) gcptrupval( new (gUpvalAlloc.allocate(1)) Upvalue( c, p, v ) )
    DLLEXPORT void GCDellocator<Upvalue>::freemem(Upvalue* p)
    {
    //    ::operator delete(p);
        gUpvalAlloc.deallocate(p, 1);
    }
#endif

#else    
// # define NEW_UPVAL(c,p,v) gcptrupval( new Upvalue( c, p, v ) )
# define NEW_UPVAL(c,p,v,thr) gcptrupval( new (::operator new (sizeof(Upvalue))) Upvalue( c, p, v ) )
DLLEXPORT void GCDellocator<Upvalue>::freemem(Upvalue* p)
{
    ::operator delete(p);
//    gUpvalAlloc.deallocate(p, sizeof(Upvalue));
}
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
#if LCFG_HAVE_TRY_CATCH      
    ,catcher(nullptr)
#endif 
    {}
    
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
    void stacklen( Stack& s, const RunContext& ctx)
    {
        s.push(double(s.size()));
    }

    void isfun( Stack& s, const RunContext&)
    {
        const Value& v = s.loc_top();
        bool rc = v.isfunprim() || v.isfun() || v.isboundfun();
        // s.pop_back();
        s.push( rc );
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
    
    void tostring( Stack& s, const RunContext& ctx )
    {
        const Value& v1 = s.pop();
        std::ostringstream os;
        v1.tostring( os );
        s.push( os.str() );
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

    DLLEXPORT void Operators::invalidOperator(const Value& v, Stack&)
    {
        bangerr() << "Invalid operator applied";
    }

    DLLEXPORT Value Operators::opThingAndValue2Value_noop( const Value& thing, const Value& other )
    {
        bangerr() << "Invalid/unsupported operator applied";
    }
    namespace { using namespace Bang;
        

    struct NumberOps : public Bang::Operators
    {
        static Value gt( const Value& thing, const Value& other )      { return Bang::Value( other.tonum() > thing.tonum() );                   }
        static Value lt( const Value& thing, const Value& other )      { return Bang::Value( other.tonum() < thing.tonum() );                   }
        static Value eq( const Value& thing, const Value& other )      { return Bang::Value( other.tonum() == thing.tonum() );                  }
        static Value plus( const Value& thing, const Value& other )    { return Bang::Value( other.tonum() + thing.tonum() );                   }
        static Value mult( const Value& thing, const Value& other )    { return Bang::Value( other.tonum() * thing.tonum() );                   }
        static Value div( const Value& thing, const Value& other )     { return Bang::Value( other.tonum() / thing.tonum() );                   }
        static Value minus( const Value& thing, const Value& other )   { return Bang::Value( other.tonum() - thing.tonum() );                   }
        static Value modulo( const Value& thing, const Value& other )  { return Bang::Value( double((int)other.tonum() % (int)thing.tonum()) ); }
    
        NumberOps()
        {
            opPlus   = &plus;
            opMult   = &mult;
            opLt     = &lt;
            opGt     = &gt;
            opMinus  = &minus;
            opDiv    = &div;
            opEq     = &eq;
            opModulo = &modulo;
        }
    } gNumberOperators; }

    struct BoolOps : public Bang::Operators
    {
        static Bang::Value and_( const Bang::Value& thing, const Bang::Value& other )  { return Bang::Value( other.tobool() && thing.tobool() ); }
        static Bang::Value or_( const Bang::Value& thing, const Bang::Value& other )   { return Bang::Value( other.tobool() || thing.tobool() ); }
        BoolOps()
        {
            opAnd = &and_;
            opOr = &or_;
        }
    } gBoolOperators;
    
    struct StringOps : public Bang::Operators
    {
        static Bang::Value eq( const Bang::Value& sThing, const Bang::Value& sOther )
        {
            const bangstring& sLt = sOther.tostr();
            return Value ( sLt == sThing.tostr() );
        }
        static Bang::Value gt( const Bang::Value& sThing, const Bang::Value& sOther )
        {
            const bangstring& sLt = sOther.tostr();
            const bangstring& sRt = sThing.tostr();
            return Value( !(sLt == sRt) && !(sLt < sRt) );
        }
        static Bang::Value lt( const Bang::Value& sThing, const Bang::Value& sOther )
        {
            const bangstring& sLt = sOther.tostr();
            return Value ( sLt < sThing.tostr() );
        }
        static Bang::Value plus( const Bang::Value& sThing, const Bang::Value& sOther )
        {
            const bangstring& v2 = sThing.tostr();
            const bangstring& v1 = sOther.tostr();
            return Bang::Value( v1 + v2 );
        }
        StringOps()
        {
            opEq = &eq;
            opLt = &lt;
            opGt = &gt;
            opPlus = &plus;
        }
    } gStringOperators;

    struct FunctionOps : public Bang::Operators
    {
        static Bang::Value eq( const Bang::Value& sThing, const Bang::Value& sOther )
        {
            return Value ( *(sOther.tofun()) == *(sThing.tofun()) );
        }
        FunctionOps()
        {
            opEq = &eq;
        }
    } gFunctionOperators;
    
    DLLEXPORT Bang::Function:: Function()
    //    : operators( &gFunctionOperators )
    {}

#if LCFG_KEEP_PROFILING_STATS    
    unsigned operatorCounts[kOpLAST];
#endif

    DLLEXPORT void dumpProfilingStats()
    {
#if LCFG_KEEP_PROFILING_STATS    
        for (int i = 0; i < kOpLAST; ++i)
        {
            std::cout << "op=" << i << " count=" << operatorCounts[i] << std::endl;
        }
#endif
    }

    static Bang::Operators* opbytype[] = {
        0,
        &gBoolOperators,
        &gNumberOperators,
        &gStringOperators,
        &gFunctionOperators,
        &gFunctionOperators,
        0,
        0, //  @todo: make empty op table for these 
    };


    tfn_opThingAndValue2Value Value::getOperator( EOperators which ) const
    {
#if 0        
        Bang::Operators* op;
        switch (type_)
        {
            case kNum:  op = &gNumberOperators; break;
            case kBool: op = &gBoolOperators;   break;
            case kStr:  op = &gStringOperators; break;
            case kFun: // fall through
            case kBoundFun: op = &gFunctionOperators; break;// fall through
            case kFunPrimitive: // fall through
            case kThread: bangerr() << "thingAndValue2Value operators not supported for type=" << type_;
        }
#else
        const Bang::Operators* const op = opbytype[type_];
#endif
#if 1
        return op->ops[which];
#else
        switch( which )
        {
            case kOpPlus:   return op->opPlus;
            case kOpMult:   return op->opMult;
            case kOpDiv:    return op->opDiv;
            case kOpMinus:  return op->opMinus;
            case kOpGt:     return op->opGt;
            case kOpLt:     return op->opLt;
            case kOpEq:     return op->opEq;
            case kOpModulo: return op->opModulo;
            case kOpOr:     return op->opOr;
            case kOpAnd:    return op->opAnd;
            default: bangerr() << "op=" << which << "not supported for type=" << type_;
        }
#endif 
    }
    

    Value Value::applyAndValue2Value( EOperators which, const Value& other ) const
    {
#if LCFG_KEEP_PROFILING_STATS
        ++operatorCounts[which];
#endif
        return (this->getOperator( which ))( *this, other );
    }
    
    
    void Value::applyCustomOperator( const bangstring& theOperator, Stack& s ) const
    {
        switch (type_)
        {
            case kNum: bangerr() << "no custom num ops"; break;
            case kStr: bangerr() << "no custom string ops"; break;
            case kFun: // fall through
            case kBoundFun: this->tofun()->customOperator( theOperator, s ); break;
            case kFunPrimitive: bangerr() << "custom operators not supported for function primitives"; break; 
            case kThread: bangerr() << "custom operators not supported for threads"; break; 
        }
    }

    void Value::applyIndexOperator( const Value& theIndex, Stack& s, const RunContext& ctx ) const
    {
        switch (type_)
        {
            case kNum: bangerr() << "no index num ops"; break;
            case kStr: bangerr() << "no index string ops"; break;
            case kFun: // fall through
            case kBoundFun: this->tofun()->indexOperator( theIndex, s, ctx ); break;
            case kFunPrimitive:
            {
                s.push( theIndex );
                this->tofunprim()( s, ctx );
            }
            break; 
            case kThread: bangerr() << "index operator not supported for threads"; break; 
        }
    }
    
    


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
    :  c == '\\' ? Primitives::drop
    :  c == '(' ? Primitives::beginStackBound
    :  c == ')' ? Primitives::endStackBound

    :  nullptr
    );
}



namespace Ast
{

    class CloseValue : public Base
    {
        const CloseValue* pUpvalParent_;
        bangstring paramName_;
        unsigned subprogUseCount_;
    public:
        void incUseCount() { ++subprogUseCount_; }
        CloseValue( const CloseValue* parent, const bangstring& name )
        : Base( kCloseValue ),
          pUpvalParent_( parent ),
          paramName_( name ),
          subprogUseCount_(0)
        {}
        const bangstring& valueName() const { return paramName_; }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "CloseValue(" << paramName_ << ") #" << subprogUseCount_ << " " << std::hex << PtrToHash(this) << std::dec << "\n";
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

        CloseValue& nthUpvalue( NthParent n )
        {
            if (n == NthParent(0))
                return *this;
            else
                return const_cast<CloseValue*>(pUpvalParent_)->nthUpvalue( --n );
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
        else if (parent_)
        {
            return parent_->getUpValue( uvName );
        }
        
        bangerr() << "Could not find dynamic upval=\"" << uvName << "\"\n";
    }

    const Value& Upvalue::getUpValue( const bangstring& uvName, const Ast::CloseValue* const upperBound ) const
    {
//         std::cerr << "Searching upvalue=" << uvName
//                   << " level.closer=" << std::hex << PtrToHash(closer_) << std::dec
//                   << " upperBound=" << std::hex << PtrToHash(upperBound) << std::dec << "\n";
            
        if (closer_ == upperBound)
            ;
        else if (uvName == closer_->valueName())
        {
            return v_;
        }
        else if (parent_)
        {
            return parent_->getUpValue( uvName, upperBound );
        }
        
        bangerr() << "Could not find dynamic upval=\"" << uvName << "\"\n";
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
        )
        : parsecontext_(parsecontext), stream_(stream),
          parent_(parent), upvalueChain_( upvalueChain ),
          pRecParsing_( pRecParsing )
        {
        }
        virtual Ast::Base* hitEof( const Ast::CloseValue* uvchain );
    };
    

    enum ESourceDest {
        kSrcStack,
        kSrcAltStack,
        kSrcLiteral,
        kSrcUpval,
        kSrcRegister,
        kSrcRegisterBool,
        kSrcCloseValue
    };
        static const char* sd2str( ESourceDest sd )
        {
            return
            ( sd == kSrcStack ? "$stk"
            : sd == kSrcAltStack ? "$altstk"
            : sd == kSrcLiteral ? "Literal"
            : sd == kSrcUpval ? "Upval"
            : sd == kSrcRegister ? "$reg"
            : sd == kSrcRegisterBool ? "$reg_bool"
            : sd == kSrcCloseValue ? "CloseValue"
            : "Unknown-source/dest"
            );
        }
    
    
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

    static const char* op2str( EOperators which )
    {
        return
        ( which == kOpPlus   ? "+"
        : which == kOpMinus  ? "-"
        : which == kOpLt     ? "<"
        : which == kOpGt     ? ">"
        : which == kOpEq     ? "="
        : which == kOpMult   ? "*"
        : which == kOpDiv    ? "/"
        : which == kOpModulo ? "%"
        : which == kOpOr ? "OR"
        : which == kOpAnd ? "AND"
        : which == kOpCustom ? "custom"
        : "unknown operator"
        );
    }

    class IsApplicable
    {
    protected:
        bool apply_;
    public:
        IsApplicable() : apply_(false) {}
        bool hasApply() const { return apply_; }
        virtual void setApply() { apply_ = true; }
    };

    class StackToAltstack : public Base
    {
    public:
        StackToAltstack()
        {
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "StackToAltstack\n";
        }

        virtual void run( Stack& stack, const RunContext& rc ) const
        {
            rc.thread->altstack.push( stack.pop() );
        }
    };

    class BoolEater
    {
    protected:
        ESourceDest boolsrc_;
    public:
        BoolEater()
        : boolsrc_( kSrcStack )
        {}
        void setSrcRegisterBool() { boolsrc_ = kSrcRegisterBool; }
    };

    class ValueEater
    {
    protected:
    public:
        ESourceDest v1src_;
        NthParent   v1uvnumber_;
        std::string v1uvname_;
        Value       v1literal_;
    public:
        ValueEater()
        : v1src_( kSrcStack ),
          v1uvnumber_( kNoParent )
        {}
        ESourceDest sourceType() const { return v1src_; }
        bool srcIsStack() const        { return v1src_ == kSrcStack; }
        bool srcIsAltStack() const     { return v1src_ == kSrcAltStack; }
        bool srcisstr() const          { return v1src_ == kSrcLiteral && v1literal_.isstr(); }
        const Value& literal() const { return v1literal_; }
        const bangstring& tostr() { return v1literal_.tostr(); }
        void setSrcUpval( const std::string& name, NthParent uvnumber )
        {
            v1uvnumber_ = uvnumber;
            v1uvname_ = name;
            v1src_ = kSrcUpval;
        }
        void setSrcAltstack()
        {
            v1src_ = kSrcAltStack;
        }
        void setSrcOther( const ValueEater& other )
        {
            *this = other;
        }
        void setSrcStack()
        {
            v1src_ = kSrcStack;
        }
        void setSrcLiteral( const Value& v )
        {
            v1literal_ = v;
            v1src_ = kSrcLiteral;
        }
        void setSrcRegister()
        {
            v1src_ = kSrcRegister;
        }
        void dump( std::ostream& o ) const
        {
            if (v1src_ == kSrcLiteral)
            {
                v1literal_.dump(o);
            }
            else if (v1src_ == kSrcUpval)
            {
                o << "upval#"
                  << v1uvnumber_.toint()
                  << ',' << v1uvname_;
            }
            else
            {
                o << sd2str( v1src_ );
            }
        }
    };

    
    

    class DestableOperator
    {
    public: // protected:
        ESourceDest dest_;
    public:
        DestableOperator()
        : dest_( kSrcStack )
        {}
    };
    
    class BoolMaker : public DestableOperator
    {
    public:
        BoolMaker()
        {}
        void setDestRegisterBool() { dest_ = kSrcRegisterBool; }
    };

    class ValueMaker : public BoolMaker
    {
    public: // protected:
        const Ast::CloseValue* cv_; // valid when dest_ is kSrcCloseValue
    public:
        ValueMaker()
        : cv_(0)
        {}
        void setDestRegister() { dest_ = kSrcRegister; }
        void setDestCloseValue( const Ast::CloseValue* cv )
        {
            dest_ = kSrcCloseValue;
            cv_ = cv;
        }
        bool destIsStack() const { return dest_ == kSrcStack; }
        void dump( std::ostream& o ) const
        {
            o << sd2str(dest_) << ",";
            if (dest_ == kSrcCloseValue)
                cv_->dump( 0, o );
        }
    };

    class Move : public Base, public ValueMaker
    {
        FRIENDOF_RUNPROG
        ValueEater src_;
    protected:
    public:
        Move( const std::string& name, NthParent uvnumber )
        : Base( kMove )
        {
            src_.setSrcUpval( name, uvnumber );
        }

        Move( const Value& v )
        : Base( kMove )
        {
            src_.setSrcLiteral(v);
        }

        bool srcIsStr()
        {
            return src_.srcisstr();
        }
        const bangstring& tostr() { return src_.tostr(); }

        
        const ValueEater& source() const { return src_; }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Move( src=";
            src_.dump(o);
            o << " dest=";
            ValueMaker::dump(o);
            o << ")\n";
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };

    class OperatorThrow : public Base
    {
    public:
        OperatorThrow() : Base(kThrow)
        {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "OperatorThrow\n";
        }
        virtual void run( Stack& s, const RunContext& rc ) const {
            bangerr() << "OperatorThrow::run() should not be called";
        }
    };


    class OperatorNot : public Base, public BoolEater, public BoolMaker
    {
    public:
        OperatorNot() {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "OperatorNot( src=" << sd2str(boolsrc_) << " dest=" << sd2str(dest_) << ")\n";
        }
        virtual void run( Stack& s, const RunContext& rc ) const
        {
            if (boolsrc_ == kSrcStack)
            {
                Value& v = s.loc_topMutate();
                if (!v.isbool())
                    throw std::runtime_error("Logical NOT operator incompatible type");
                if (dest_ == kSrcStack)
                    v.mutateNoType( !v.tobool() );
                else // should only be kSrcRegisterBool
                {
                    rc.thread->rb0_ = !v.tobool();
                    s.pop_back();
                }
            }
            else // should only be kSrcRegisterBool
            {
                if (dest_ == kSrcStack)
                    s.push( !rc.thread->rb0_ );
                else
                    rc.thread->rb0_ = !rc.thread->rb0_;
            }
        }
    };

    class OperatorBindings : public Base
    {
        const CloseValue* upperBound_;
    public:
        OperatorBindings( const CloseValue* upperBound )
        : upperBound_( upperBound )
        {}
        OperatorBindings()
        : upperBound_( nullptr )
        {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "OperatorBindings( " << std::hex << PtrToHash(upperBound_) << std::dec << ")\n";
        }
        virtual void run( Stack& s, const RunContext& rc ) const;
    };


    class SecondValueEater
    {
    public:
        ValueEater secondsrc_;
        ValueEater& secondValueSrc() { return secondsrc_; }
        const ValueEater& secondValueSrc() const { return secondsrc_; }
        void setSecondSrcOther( const ValueEater& other ) { secondsrc_.setSrcOther( other ); }
        bool secondValueIsStack() const { return secondsrc_.srcIsStack(); }
        virtual bool firstValueNotStack() = 0;
    };

    class ApplyThingAndValue2ValueOperator : public Base, public ValueEater, public ValueMaker, public SecondValueEater
    {
        FRIENDOF_RUNPROG
    public:
        bool argSwap_;
        EOperators openum_ ; // method
        tfn_opThingAndValue2Value thingliteralop_;
//        NthParent uvnumber_; // when srcthing_ == kSrcLiteral


        ApplyThingAndValue2ValueOperator( EOperators openum )
        : Base( kApplyThingAndValue2ValueOperator ),
          argSwap_( false ),
          openum_( openum )
        {}

        void setArgSwap() { argSwap_ = true; }
        
        //~~~ @todo: revisit use of these and see if generic ValueEater can be used instead
        bool firstValueNotStack() { return v1src_ != kSrcStack; }
        void setThingRegister() { this->setSrcRegister(); }
        void setOtherRegister() { secondsrc_.setSrcRegister(); }

        void setThingVeSrc( const ValueEater& other )
        {
            ValueEater::setSrcOther( other );
            if (other.v1src_ == kSrcLiteral)
            {
                thingliteralop_ = v1literal_.getOperator( openum_ );
            }
        }

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);

            if (argSwap_)
            {
                o << "SWAP ";
            }
            
            o << "ApplyThingAndValue2ValueOperator( ";

            o << " s1=";
            secondsrc_.dump(o);

            o << " s2=";
            ValueEater::dump(o);
            
            o << " op=" << openum_ << "(" << op2str(openum_) << ')';
            o << " dest=";
            ValueMaker::dump( o );
            
            o << ")\n";
        }
        virtual void run( Stack& stack, const RunContext& rc ) const
        {
            bangerr() << "ApplyThingAndValue2ValueOperator::run() should not be called";
        }
    }; // end, class ApplyThingAndValue2ValueOperator


    class Increment : public Base, public ValueEater, public ValueMaker
    {
        FRIENDOF_RUNPROG
        double v_;
        EOperators op_;
    public:
#if LCFG_TRYJIT        
        pifi_up f_op;
        pifi_up_void f_op_void;
#endif
        std::vector<ApplyThingAndValue2ValueOperator*> origOps_;
        Increment( double toAdd, EOperators eop, const ValueEater& ve, const ValueMaker& vm )
        : Base( ve.v1src_ == kSrcUpval ? kIncrement : vm.dest_ == kSrcRegister ? kIncrementReg2Reg : kIncrementReg ),
          ValueEater( ve ),
          ValueMaker( vm ),
          v_( toAdd ), op_( eop )
        { 
#if LCFG_TRYJIT
            switch( v1src_ )
            {
                case kSrcUpval: f_op = jitit_increment_uv0( toAdd, eop, v1uvnumber_.toint(), false ); break;
                case kSrcRegister:
                {
                    f_op = jitit_increment_uv0( toAdd, eop, 0, true );
                }
                break;
            }
        }
        Increment( pifi_up_void jitted, EOperators eop, const ValueEater& ve, const ValueMaker& vm )
        : Base( kIncrementReg2Reg ), // ve.v1src_ == kSrcUpval ? kIncrement : vm.dest_ == kSrcRegister ? kIncrementReg2Reg : kIncrementReg ),
          ValueEater( ve ),
          ValueMaker( vm ),
          v_( -999.0 ),
          op_( eop ),
          f_op_void( jitted )
        { 
#endif 
        }
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Increment/" << instr_ << " ("; //  << v_;

            o << " s2=";
            ValueEater::dump(o);
            o << " op=" << op_ << "[" << op2str(op_) << "]";
            o << " lit="  << v_;
            o << " dest=";
            ValueMaker::dump(o);
            
            o << ")\n";
            std::for_each( origOps_.begin(), origOps_.end(),
                [&]( ApplyThingAndValue2ValueOperator* op ) { op->dump( level + 1, o ); } );
        }
        virtual void run( Stack& stack, const RunContext& rc ) const {
            bangerr() << "Increment should not run";
        }
    };
    
    class ApplyCustomOperator : public Base, public ValueEater
    {
    public:
        bangstring custom_ ; // method
        ApplyCustomOperator( const bangstring& custom )
        : custom_( custom )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyCustomOperator( src=";
            if (v1src_ == kSrcStack)
                o << "stack ";
            else
            {
                o << "upval#"
                  << v1uvnumber_.toint()
                  << ',' << v1uvname_ << ' ';
            }
            o << custom_ << ")\n";
        }

#if 1
 #define FRVE2UV(frame,ve) frame.getUpValue((ve).v1uvnumber_)
 #define FRATAV2VUV(frame,atav2v) frame.getUpValue((atav2v).v1uvnumber_)
#endif 
        
        virtual void run( Stack& stack, const RunContext& rc ) const
        {
            switch (v1src_)
            {
                case kSrcUpval:
                {
                    const Value& owner = FRVE2UV( rc, *this );
                    owner.applyCustomOperator( custom_, stack );
                }
                break;
                default:
                {
                    const Value& owner = stack.pop();
                    owner.applyCustomOperator( custom_, stack );
                }
                break;
            }
        }
    };
    
    class ApplyCustomOperatorDotted : public Base, public ValueEater
    {
    public:
        bangstring custom_ ; // method
        bangstring dotted_ ; // method
        ApplyCustomOperatorDotted( const bangstring& custom, const bangstring& dotted )
        : custom_( custom ),
          dotted_( dotted )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyCustomOperatorDotted( src=" << sd2str(v1src_);
            ValueEater::dump(o);
            o << custom_ << " ." << dotted_ << ")\n";
        }

        virtual void run( Stack& stack, const RunContext& rc ) const
        {
            switch (v1src_)
            {
                case kSrcUpval:
                {
                    const Value& owner = FRVE2UV(rc, (*this)); 
                    stack.push( dotted_ );
                    owner.applyCustomOperator( custom_, stack );
                }
                break;
                
                default:
                {
                    const Value& owner = stack.pop();
                    stack.push( dotted_ );
                    owner.applyCustomOperator( custom_, stack );
                }
                break;
            }
        }
    };
    
    
    class PushPrimitive: public Base, public IsApplicable
    {
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
            if (gFailedAst == this)
                o << "FAIL *** ";
            indentlevel(level, o);
            o << (apply_?"Apply":"Push") << "Primitive op='" << desc_ << "'\n";
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

    
    class Apply : public Base, public ValueEater
    {
    public:
        Apply() : Base( kApply ) {}
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Apply (src=";
            ValueEater::dump(o);
            o << ") " << where_ ;
            if (gFailedAst == this)
                o << " ***                                 <== *** FAILED *** ";
            o << std::endl;
        }
    };


    class Program : public Base, public IsApplicable
    {
        FRIENDOF_OPTIMIZE
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

//         void setAst( const astList_t& newast )
//         {
//             ast_ = newast;
//         }
        
        void setApply() {
            IsApplicable::setApply();
            instr_ = kApplyProgram;
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec << ' ' << (hasApply() ? "Apply" : "") << "Program\n";
            std::for_each
            (   ast_.begin(), ast_.end(),
                [&]( const Ast::Base* ast ) { ast->dump( level+1, o ); }
            );
        }

        const astList_t* getAst() const { return &ast_; }
        astList_t& astRef() { return ast_; }

        // 'run' pushes the program onto the stack as a BoundProgram.
        // 
        void run( Stack& stack, const RunContext& ) const;
    }; // end, class Ast::Program

    class ApplyIndexOperator : public Base, public ValueEater
    {
        FRIENDOF_RUNPROG
        ValueEater indexValue_;
//        void runAst( Stack& stack, const RunContext& ) const;
    public:
        ApplyIndexOperator( const bangstring& msgStr )
        : Base( kApplyIndexOperator )
        {
            indexValue_.setSrcLiteral( Bang::Value(msgStr) );
        }

        ApplyIndexOperator()
        : Base( kApplyIndexOperator )
        {
            // indexValue_.setSrcLiteral( Bang::Value(msgStr) );
        }
        ApplyIndexOperator( const std::vector<Ast::Base*>& ast );

        bool indexValueSrcIsStack() { return indexValue_.srcIsStack(); }
        void setIndexValueSrcRegister() { return indexValue_.setSrcRegister(); }
        void setIndexValueLiteral( const Value& v ) { return indexValue_.setSrcLiteral( v ); }
        
        //void setIndexValueUpval( const Ast::PushUpval* pup ) { return indexValue_.setSrcUpval( pup ); }
        void setIndexValueSource( const ValueEater& other ) {
            indexValue_ = other;
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "ApplyIndexOperator( src=";
            ValueEater::dump(o);
            o << " [";
            indexValue_.dump(o);
            o << "] )\n";
        }
        virtual void run( Stack& stack, const RunContext& ) const
        { bangerr() <<"ApplyIndexOperator::run should not be called"; }
    }; // end, ApplyIndexOperator class
    
    class IfElse : public Base, public BoolEater
    {
        Ast::Program* if_;
        Ast::Program* else_;
    public:
        IfElse( Ast::Program* if__, Ast::Program* else__ )
        : Base( kIfElse ),
          if_(if__),
          else_( else__ )
        {}

        Ast::Program* branchTaken( Thread& thr ) const
        {
            if (boolsrc_ == kSrcStack)
            {
                bool isCond = thr.stack.loc_top().tobool();
                thr.stack.pop_back();
                return isCond ? if_ : else_;
            }
            else
            {
                return thr.rb0_ ? if_ : else_;
            }
        }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "If (" << sd2str(boolsrc_) << ')';
            
            if_->dump( level, o );
            if (else_)
            {
                indentlevel(level, o);
                o << "Else";
                else_->dump( level, o );
            }
        }
    }; // end class IfElse


    class TryCatch : public Base
    {
        FRIENDOF_RUNPROG
        Ast::Program* try_;
        Ast::Program* catch_;
    public:
        TryCatch( Ast::Program* try__, Ast::Program* catch__ )
        : Base( kTryCatch ),
          try_( try__ ),
          catch_( catch__ )
        {}

        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << "Try {\n";
            
            try_->dump( level, o );
            indentlevel(level, o);
            o << "} Catch {\n";
            catch_->dump( level, o );
            indentlevel(level,o);
            o << "}\n";
        }
    }; // end class IfElse
    

    class PushFunctionRec : public Base, public IsApplicable
    {
    protected:
        FRIENDOF_RUNPROG
        FRIENDOF_OPTIMIZE
        Ast::Program* pRecFun_;
        NthParent nthparent_;
    public:
        int skipInstructions;
        PushFunctionRec( Ast::Program* other, NthParent boundAt )
        : pRecFun_( other ),
          nthparent_(boundAt)
        {}

        void setBindingParent( NthParent n ) {
            nthparent_ = n;
//            nthparent_ = (nthparent_ == kNoParent) ? NthParent(0) : --nthparent;
        }

        void setApply() {
            IsApplicable::setApply();
            instr_ = kApplyFunRec;
        }

        const Ast::Program::astList_t* getRecFunAst() const { return pRecFun_->getAst(); }
        
        virtual void dump( int level, std::ostream& o ) const
        {
            indentlevel(level, o);
            o << (hasApply() ? "Apply" : "Push") << "FunctionRec[ " << std::hex << PtrToHash(pRecFun_) << std::dec << "/" << nthparent_ .toint()
              << " / " << instr_ << " ]" << std::endl;
        }

        virtual void run( Stack& stack, const RunContext& ) const;
    };
    
} // end, namespace Ast



class DynamicLookup : public Function
{
    SHAREDUPVALUE upvalues_;
    const Ast::CloseValue* upperBound_;
    void indexOperatorNoCtx( const Value& theIndex, Stack& stack )
    {
        const auto& str = theIndex.tostr();
        if (upperBound_)
            stack.push( upvalues_->getUpValue( str, upperBound_ ) );
        else
            stack.push( upvalues_->getUpValue( str ) );
    }
public:
    DynamicLookup( SHAREDUPVALUE_CREF upvalues )
    : upvalues_( upvalues ),
      upperBound_( nullptr )
    {
    }

    DynamicLookup( SHAREDUPVALUE_CREF upvalues, const Ast::CloseValue* upperBound )
    : upvalues_( upvalues ),
      upperBound_( upperBound )
    {
    }
    
    SHAREDUPVALUE_CREF upvalues() const { return upvalues_; }
    
    DLLEXPORT virtual void indexOperator( const Value& theIndex, Stack& stack, const RunContext& )
    {
        this->indexOperatorNoCtx( theIndex, stack );
    }
    virtual void apply( Stack& s )
    {
        this->indexOperatorNoCtx( s.pop(), s ); // obtain index from stack
    }
};

/*virtual*/ void
Ast::OperatorBindings::run( Stack& s, const RunContext& rc ) const
{
    const auto& bindings = NEW_BANGFUN(DynamicLookup, rc.upvalues(), upperBound_);
    s.push( STATIC_CAST_TO_BANGFUN(bindings) );
}
    
    

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
#if 1
        const auto& restoreFunction = NEW_BANGFUN(FunctionRestoreStack, s );
        s.push( STATIC_CAST_TO_BANGFUN(restoreFunction) );
#endif 
    }

#if LCFG_HAVE_SAVE_BINDINGS                
    void savebindings( Stack& s, const RunContext& rc )
    {
        const auto& bindings = NEW_BANGFUN(DynamicLookup, rc.upvalues() );
        s.push( STATIC_CAST_TO_BANGFUN(bindings) );
    }
#endif

    void rebindvalues( Stack& s, const Value& v, const Value& vbindname, const Value& newval )
    {
        const auto& bindname = vbindname.tostr();
        
        if (v.isboundfun())
        {
            auto bprog = v.toboundfunhold();

            SHAREDUPVALUE newchain = replace_upvalue( bprog->upvalues_, bindname, newval );

            const auto& newfun = NEW_BANGFUN(BoundProgram, bprog->program_, newchain );
            s.push( newfun );
            return;
        }
        else if (v.isfun())
        {
            DynamicLookup* pLookup = dynamic_cast<DynamicLookup*>( v.tofun().get() );
            if (pLookup)
            {
                SHAREDUPVALUE newchain = replace_upvalue( pLookup->upvalues(), bindname, newval );
                const auto& bindings = NEW_BANGFUN( DynamicLookup, newchain );
                s.push( STATIC_CAST_TO_BANGFUN(bindings) );
            }
            else
                throw std::runtime_error("rebind-fun: not a bound function");
        }
        else
            throw std::runtime_error("rebind-fun: not a bound function");

    }
    void rebindOuterFunction( Stack& s, const RunContext& rc )
    {
        const Value& vbindname = s.pop();
        const Value& newval = s.pop();
        const Value& v = s.pop();
        rebindvalues( s, v, vbindname, newval );
    }
    void rebindFunction( Stack& s, const RunContext& rc )
    {
        const Value& vbindname = s.pop();
        const Value& v = s.pop();
        const Value& newval = s.pop();
        rebindvalues( s, v, vbindname, newval );
    }
}



    
    


        BoundProgram::BoundProgram( const Ast::Program* program, SHAREDUPVALUE_CREF upvalues )
        : // Function(true),
          program_( program ), upvalues_( upvalues )
        {
//             for (int i = 0; i < 16; ++i)
//                 uvcache[i].v = nullptr;
        }
    
        void BoundProgram::dump( std::ostream & out )
        {
            program_->dump( 0, std::cerr );
        }
    
    void throwNoFunVal( const Ast::Base* pInstr, const Value& v )
    {
        std::ostringstream oss;
        oss << pInstr->where_;
        oss << ": " << typeid(*pInstr).name() << ": Called apply without function.2; found type=" << v.type() << " V=";
        v.dump(oss);
        throw std::runtime_error(oss.str());
    }

//SimplestAllocator<RunContext> gAllocRc;


    void Thread::setcallin(Thread*caller)
    {
        if (!callframe)
        {
            // maybe this should just be done in constructor.
            
            // start me off pointing at the BoundProgram, ready to roll
            // no dynamic cast for quickness. responsibility of caller to insure passed Function is a BoundFunction.
            BoundProgram* pbound = reinterpret_cast<BoundProgram*>( boundProg_.get() );

            this->callframe =
                new (this->rcAlloc_.allocate(sizeof(RunContext)))
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
#if LCFG_HAVE_TRY_CATCH      
    ,catcher(nullptr)
#endif 
     {}
    
void RunApplyValue( const Ast::Base* pInstr, const Value& v, Stack& stack, const RunContext& frame )
{
    switch (v.type())
    {
        case Value::kFun: { auto f = v.tofun(); f->apply(stack); } break;
        case Value::kFunPrimitive: v.tofunprim()( stack, frame ); break;
        default: throwNoFunVal(pInstr, v);
    }
}
    
void xferstack( Thread* from, Thread* to )
{
    from->stack.giveTo( to->stack );
}

static Thread gNullThread;
DLLEXPORT Thread* pNullThread( &gNullThread );
DLLEXPORT Thread* Thread::nullthread() { return &gNullThread; }

    template <ESourceDest esd> struct DestSet {};
    template <> struct DestSet<kSrcStack>        { static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value& vv ) { stack.push(vv); }
                                                   static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value&& vv ) { stack.push(std::move(vv)); }
    };
    template <> struct DestSet<kSrcRegister>     { static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value& vv ) { pThread->r0_ = vv; }
                                                   static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value&& vv ) { pThread->r0_ = std::move(vv); }
    }; 
    template <> struct DestSet<kSrcRegisterBool> { static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value& vv ) { pThread->rb0_ = vv.tobool(); } };
    template <> struct DestSet<kSrcCloseValue>   { static inline void set( const Ast::ValueMaker& pa, Thread* pThread, RunContext& frame, Stack& stack, const Value& vv ) {
        frame.upvalues_ = NEW_UPVAL( pa.cv_, frame.upvalues_, vv );
    } };

    template <ESourceDest esd> struct SrcGet {};
    template <> struct SrcGet<kSrcLiteral>  { static inline const Bang::Value& get( const Ast::ValueEater& pa, Thread* pThread, RunContext& frame ) { return pa.v1literal_; } };
    template <> struct SrcGet<kSrcRegister> { static inline const Bang::Value& get( const Ast::ValueEater& pa, Thread* pThread, RunContext& frame ) { return pThread->r0_; }  };
    template <> struct SrcGet<kSrcUpval>    { static inline const Bang::Value& get( const Ast::ValueEater& pa, Thread* pThread, RunContext& frame ) { return FRVE2UV(frame, pa); } }; // frame.getUpValue( pa.v1uvnumber_ ); } };
    template <> struct SrcGet<kSrcStack>    { static inline Bang::Value get( const Ast::ValueEater& pa, Thread* pThread, RunContext& frame ) { return pThread->stack.pop(); } };
    
    template <ESourceDest esd> struct Invoke2n2OperatorWithThingFrom {
        static inline Bang::Value getAndCall( const Ast::ApplyThingAndValue2ValueOperator& pa, Thread* pThread, RunContext& frame, Stack& stack ) {
            switch( pa.secondsrc_.v1src_ ) {
                case kSrcUpval:    return SrcGet<esd>::get(pa,pThread,frame).applyAndValue2Value( pa.openum_, SrcGet<kSrcUpval>   ::get( pa.secondsrc_, pThread, frame ) );
                case kSrcRegister: return SrcGet<esd>::get(pa,pThread,frame).applyAndValue2Value( pa.openum_, SrcGet<kSrcRegister>::get( pa.secondsrc_, pThread, frame ) );
                case kSrcLiteral:  return SrcGet<esd>::get(pa,pThread,frame).applyAndValue2Value( pa.openum_, SrcGet<kSrcLiteral> ::get( pa.secondsrc_, pThread, frame ) );
                default:           return SrcGet<esd>::get(pa,pThread,frame).applyAndValue2Value( pa.openum_, SrcGet<kSrcStack>   ::get( pa.secondsrc_, pThread, frame ) );
            }
        }
    };
    
    template <> struct Invoke2n2OperatorWithThingFrom<kSrcStack> {
        static inline Bang::Value getAndCall( const Ast::ApplyThingAndValue2ValueOperator& pa, Thread* pThread, RunContext& frame, Stack& stack ) {
            const Bang::Value& owner = stack.pop();
            switch( pa.secondsrc_.v1src_ ) {
                case kSrcUpval:    return owner.applyAndValue2Value( pa.openum_, SrcGet<kSrcUpval>   ::get( pa.secondsrc_, pThread, frame ) );
                case kSrcRegister: return owner.applyAndValue2Value( pa.openum_, SrcGet<kSrcRegister>::get( pa.secondsrc_, pThread, frame ) );
                case kSrcLiteral:  return owner.applyAndValue2Value( pa.openum_, SrcGet<kSrcLiteral> ::get( pa.secondsrc_, pThread, frame ) );
                default:           return owner.applyAndValue2Value( pa.openum_, SrcGet<kSrcStack>   ::get( pa.secondsrc_, pThread, frame ) );
            }
        }
    };
    
    template <> struct Invoke2n2OperatorWithThingFrom<kSrcLiteral> {
        static inline Bang::Value getAndCall( const Ast::ApplyThingAndValue2ValueOperator& pa, Thread* pThread, RunContext& frame, Stack& stack ) {
            switch( pa.secondsrc_.v1src_ ) {
                case kSrcUpval:    return pa.thingliteralop_( pa.v1literal_, SrcGet<kSrcUpval>   ::get( pa.secondsrc_, pThread, frame ) );
                case kSrcRegister: return pa.thingliteralop_( pa.v1literal_, SrcGet<kSrcRegister>::get( pa.secondsrc_, pThread, frame ) );
                case kSrcLiteral:  return pa.thingliteralop_( pa.v1literal_, SrcGet<kSrcLiteral> ::get( pa.secondsrc_, pThread, frame ) );
                default:           return pa.thingliteralop_( pa.v1literal_, SrcGet<kSrcStack>   ::get( pa.secondsrc_, pThread, frame ) );
            }
        }
    };

#if LCFG_HAVE_TAV_SWAP
            // i think a better implementation would be to have a distinct Ast::Swap()
            // instruction, then the optimizer could remove it when followed by a dual ValueEater where
            // both Values are not coming from the stack.  if both values come from the stack
            // then the Ast::Swap() instruction is left in place.
            if (pa.argSwap_)
            {
                switch( pa.secondsrc_.v1src_ ) {
                    case kSrcUpval:    return FRVE2UV(frame, pa.secondsrc_) .applyAndValue2Value( pa.openum_, FRATAV2VUV(frame,pa) );
                    case kSrcRegister: return pThread->r0_                  .applyAndValue2Value( pa.openum_, FRATAV2VUV(frame,pa)  );
                    case kSrcLiteral:  return pa.secondsrc_.v1literal_      .applyAndValue2Value( pa.openum_, FRATAV2VUV(frame,pa) );
                    default:           return stack.pop()                   .applyAndValue2Value( pa.openum_, FRATAV2VUV(frame,pa) );
                }
            }
            else
#endif
    
    template< ESourceDest eThingSource, ESourceDest eValueDest >
    struct ThingValueOperatorFetchFromSaveTo
    {
        static inline void apply(  Stack& stack, Thread* pThread, RunContext& frame, const Ast::ApplyThingAndValue2ValueOperator& pa )
        {
            DestSet<eValueDest>::set( pa, pThread, frame, stack, Invoke2n2OperatorWithThingFrom<eThingSource>::getAndCall( pa, pThread, frame, stack ) );
        }
    };

    /* this is pretty horribly worse than the plain inline version with MSVC; it's *nearly* as good with GCC, but
     * still not quite.  I guess I should be happy that it comes anywhere near close. As much as I like the
     * theory of "use templates, not preprocessor" I guess compilers still aren't quite as smart as human
     * crafted syntax expansions. */
    template< ESourceDest edst >
    struct ApplyThingValueCall
    {
        static inline void docall( Stack& stack, Thread* pThread, RunContext& frame, const Ast::ApplyThingAndValue2ValueOperator& pa )
        {
            switch (pa.v1src_)
            {
                case kSrcUpval:    ThingValueOperatorFetchFromSaveTo<kSrcUpval,edst>   ::apply( stack, pThread, frame, pa ); break;
                case kSrcStack:    ThingValueOperatorFetchFromSaveTo<kSrcStack,edst>   ::apply( stack, pThread, frame, pa ); break;
                case kSrcLiteral:  ThingValueOperatorFetchFromSaveTo<kSrcLiteral,edst> ::apply( stack, pThread, frame, pa ); break;
                case kSrcRegister: ThingValueOperatorFetchFromSaveTo<kSrcRegister,edst>::apply( stack, pThread, frame, pa ); break;
            }
        }
    };

    
    template <ESourceDest source, ESourceDest dest> struct Mover {
        static inline void domove( const Ast::ValueMaker& vm, const Ast::ValueEater& ve, Thread* pThread, RunContext& frame, Stack& stack ) { 
            DestSet<dest>::set( vm, pThread, frame, stack, SrcGet<source>::get( ve, pThread, frame ) );
        }
    };

//     template <ESourceDest source, ESourceDest dest> struct Incrementer {
//         static void domove( const Ast::ValueMaker& vm, const Ast::ValueEater& ve, Thread* pThread, RunContext& frame, Stack& stack ) { 
//             DestSet<dest>::set( vm, pThread, frame, stack, SrcGet<source>::get( ve, pThread, frame ).tonum() + 1 );
//         }
//     };
    

#if __GNUC__
# define LCFG_COMPUTED_GOTO 1
#else
# define LCFG_COMPUTED_GOTO 0
#endif 


    
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
        new (pThread->rcAlloc_.allocate(sizeof(RunContext)))
        RunContext( pThread, TMPFACT_PROG_TO_RUNPROG(inprog), inupvalues );
restartThread:
    pThread->callframe->thread = pThread;
    Stack& stack = pThread->stack;
restartReturn:
    RunContext& frame = *(pThread->callframe);
restartTco:

#if LCFG_COMPUTED_GOTO     
# define OPCODE_LOC(op) OPCODE_START_##op
# define OPCODE_2LOC(pre,op) OPCODE_START_##pre##op
# define OPCODE_END() \
            pInstr = *(frame.ppInstr++); \
            goto *dispatch_table[pInstr->instr_];
#else
# define OPCODE_LOC(op) case Ast::Base::op
# define OPCODE_2LOC(pre,op) OPCODE_START_##pre##op
# define OPCODE_END() break
#endif


#if LCFG_COMPUTED_GOTO

    static void* const dispatch_table[] = {
        &&OPCODE_LOC(kUnk),
        &&OPCODE_LOC(kBreakProg),
        &&OPCODE_LOC(kCloseValue),
        &&OPCODE_LOC(kApply),
        &&OPCODE_LOC(kMove),
        &&OPCODE_LOC(kApplyProgram),
        &&OPCODE_LOC(kApplyFunRec),
        &&OPCODE_LOC(kApplyThingAndValue2ValueOperator),
        &&OPCODE_LOC(kApplyIndexOperator),
        &&OPCODE_LOC(kIncrement),
        &&OPCODE_LOC(kIncrementReg),
        &&OPCODE_LOC(kIncrementReg2Reg),
        &&OPCODE_LOC(kIfElse),
#if LCFG_HAVE_TRY_CATCH        
        &&OPCODE_LOC(kTryCatch),
        &&OPCODE_LOC(kThrow),
#else
        0,
        0,
#endif 
        &&OPCODE_LOC(kTCOApply),
        &&OPCODE_LOC(kTCOApplyProgram),
        &&OPCODE_LOC(kTCOApplyFunRec),
        &&OPCODE_LOC(kTCOIfElse),
        &&OPCODE_LOC(kMakeCoroutine),
        &&OPCODE_LOC(kYieldCoroutine),
        &&OPCODE_LOC(kEofMarker)
    };
#endif 
    
    const Ast::Base* pInstr;
    try
    {
        //const Ast::Base* const ** pppInstr = &(frame.ppInstr);
        //for ( ;true; ++*pppInstr)
        while (true)
        {
            pInstr = *(frame.ppInstr++);
#if LCFG_COMPUTED_GOTO
            goto *dispatch_table[pInstr->instr_];
            do {
#else
            switch (pInstr->instr_) {
#endif 
            OPCODE_LOC(kUnk):
                    pInstr->run( stack, frame );
            OPCODE_END();

//                dobreakprog:
                OPCODE_LOC(kBreakProg):
                {
                    RunContext* prev = frame.prev;
                    frame.~RunContext();
                    pThread->rcAlloc_.deallocate( &frame, sizeof(RunContext) );
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

            OPCODE_LOC(kYieldCoroutine):
                    if (pThread->pCaller)
                    {
                        if (reinterpret_cast<const Ast::YieldCoroutine*>(pInstr)->shouldXferstack())
                            xferstack( pThread, pThread->pCaller );
                        pThread = pThread->pCaller;
                        goto restartThread;
                    }
                    else
                        return; // nowhere to go

                

        OPCODE_LOC(kEofMarker):
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
                        catch (const std::exception& ) // parse error
                        {
                            //~~~ @todo: shouldn't I dump this here?  reportError or something?
                            pEof->repl_prompt(stack);
                        }
                    }
                }
        OPCODE_END();

            OPCODE_LOC(kMakeCoroutine):
                    Ast::MakeCoroutine::go( stack, pThread );
            OPCODE_END();

                OPCODE_LOC(kCloseValue):
                    frame.upvalues_ =
                        NEW_UPVAL
                        (   reinterpret_cast<const Ast::CloseValue*>(pInstr),
                            frame.upvalues_,
                            stack.pop()
                        );
                OPCODE_END();
            
                OPCODE_LOC(kTCOApplyFunRec):
                {
                    // no dynamic cast, should be safe since we know the type from instr_
//                    std::cerr << "got kTCOApplyFunRec" << std::endl;
                    const Ast::PushFunctionRec* afn = reinterpret_cast<const Ast::PushFunctionRec*>(pInstr);
                    frame.rebind
                    (  TMPFACT_PROG_TO_RUNPROG(afn->pRecFun_),
                        (afn->nthparent_ == kNoParent) ? SHAREDUPVALUE() : frame.nthBindingParent( afn->nthparent_ )
                    );
                    goto restartTco;
                }
                OPCODE_END();

            OPCODE_LOC(kApplyFunRec):
                {
                    const Ast::PushFunctionRec* afn = reinterpret_cast<const Ast::PushFunctionRec*>(pInstr);
                    inprog = afn->pRecFun_;
                    inupvalues = (afn->nthparent_ == kNoParent) ? SHAREDUPVALUE() : frame.nthBindingParent( afn->nthparent_ );
                    goto restartNonTail;
                }
                OPCODE_END();
                
            OPCODE_LOC(kTCOIfElse):
                {
                    const Ast::IfElse* ifelse = reinterpret_cast<const Ast::IfElse*>(pInstr);
                    const Ast::Program* p = ifelse->branchTaken(*pThread);
                    if (p)
                    {
                        frame.rebind( TMPFACT_PROG_TO_RUNPROG(p) );
                        goto restartTco;
                    }
                }
                OPCODE_END();

            OPCODE_LOC(kIfElse):
                {
                    const Ast::IfElse* ifelse = reinterpret_cast<const Ast::IfElse*>(pInstr);
                    const Ast::Program* p = ifelse->branchTaken(*pThread);
                    if (p)
                    {
                        inprog = p;
                        inupvalues = frame.upvalues_;
                        goto restartNonTail;
                    }
                }
                OPCODE_END();

#if LCFG_HAVE_TRY_CATCH                
            OPCODE_LOC(kTryCatch):
                {
                    const Ast::TryCatch* trycatch = reinterpret_cast<const Ast::TryCatch*>(pInstr);
                    const Ast::Program* p = trycatch->try_; 
                    inprog = p;
                    inupvalues = frame.upvalues_;
                    
                    pThread->callframe =
                        new (pThread->rcAlloc_.allocate(sizeof(RunContext)))
                        RunContext( pThread, TMPFACT_PROG_TO_RUNPROG(inprog), inupvalues );

                    pThread->callframe->catcher = trycatch->catch_;
                    
                    goto restartThread;
                }
                OPCODE_END();

            OPCODE_LOC(kThrow):
                {
                    RunContext *pframe = &frame;

                    while (pframe && !pframe->catcher)
                    {
                        RunContext* prev = pframe->prev;
//                        std::cerr << "tossing frame, frame=" << pframe << " prev=" << prev << std::endl;
                        pframe->~RunContext();
                        pThread->rcAlloc_.deallocate( pframe, sizeof(RunContext) );
                        pframe = prev;
                    }
                    
                    if (pframe && pframe->catcher)
                    {
                        pThread->callframe = pframe;
//                        std::cerr << "running catch block frame, frame=" << pframe << std::endl;
                        pframe->rebind( TMPFACT_PROG_TO_RUNPROG(pframe->catcher), inupvalues );
                        goto restartReturn;
                    }
                    else
                    {
                        throw AstExecFail( *(frame.ppInstr), "Unhandled exception" ); // ;e.what() );
//                        bangerr() << 
                    }
                }
                OPCODE_END();
#endif

            OPCODE_LOC(kIncrement):
                {
#if LCFG_TRYJIT
                    const Ast::Increment& move = *reinterpret_cast<const Ast::Increment*>(pInstr);
                    double v2 = move.f_op( (void*)frame.upvalues_.get() ); // , (void*)pThread ); //  + 1;
                    switch (move.dest_)
                    {
                        case kSrcStack:      DestSet<kSrcStack>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                        case kSrcRegister:   DestSet<kSrcRegister>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                        case kSrcCloseValue: DestSet<kSrcCloseValue>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                    }
#endif 
                }
                OPCODE_END();

            OPCODE_LOC(kIncrementReg):
                {
#if LCFG_TRYJIT
                    const Ast::Increment& move = *reinterpret_cast<const Ast::Increment*>(pInstr);
                    double v2 = move.f_op( pThread ); // , (void*)pThread ); //  + 1;v = SrcGet<kSrcUpval>  ::get( move, pThread, frame ).tonum(); break;
                    switch (move.dest_)
                    {
                        case kSrcStack:      DestSet<kSrcStack>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                        case kSrcRegister:   DestSet<kSrcRegister>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                        case kSrcCloseValue: DestSet<kSrcCloseValue>::set( move, pThread, frame, stack, Value( v2 ) ); break;
                    }
#endif 
                }
                OPCODE_END();

            OPCODE_LOC(kIncrementReg2Reg):
                {
#if LCFG_TRYJIT
                    const Ast::Increment& move = *reinterpret_cast<const Ast::Increment*>(pInstr);
                    move.f_op_void( pThread, frame.upvalues_.get() ); // , (void*)pThread ); //  + 1;v = SrcGet<kSrcUpval>  ::get( move, pThread, frame ).tonum(); break;
//                     switch (move.dest_)
//                     {
//                         case kSrcStack:      DestSet<kSrcStack>::set( move, pThread, frame, stack, Value( v2 ) ); break;
//                         case kSrcRegister:   DestSet<kSrcRegister>::set( move, pThread, frame, stack, Value( v2 ) ); break;
//                         case kSrcCloseValue: DestSet<kSrcCloseValue>::set( move, pThread, frame, stack, Value( v2 ) ); break;
//                     }
#endif 
                }
                OPCODE_END();

            OPCODE_LOC(kMove):
                {
                    const Ast::Move& move = *reinterpret_cast<const Ast::Move*>(pInstr);
                    
                    switch (move.dest_)
                    {
                        case kSrcStack: { switch (move.src_.v1src_) {
                                case kSrcUpval:   Mover<kSrcUpval,kSrcStack>  ::domove( move, move.src_, pThread, frame, stack ); break;
                                case kSrcLiteral: Mover<kSrcLiteral,kSrcStack>::domove( move, move.src_, pThread, frame, stack ); break;
                            } break; } break;
                        case kSrcRegister: { switch (move.src_.v1src_) {
                                case kSrcUpval:   Mover<kSrcUpval,kSrcRegister>  ::domove( move, move.src_, pThread, frame, stack ); break;
                                case kSrcLiteral: Mover<kSrcLiteral,kSrcRegister>::domove( move, move.src_, pThread, frame, stack ); break;
                            } break; } break;
                        case kSrcRegisterBool: { switch (move.src_.v1src_) {
                                case kSrcUpval:   Mover<kSrcUpval,kSrcRegisterBool>  ::domove( move, move.src_, pThread, frame, stack ); break;
                                case kSrcLiteral: Mover<kSrcLiteral,kSrcRegisterBool>::domove( move, move.src_, pThread, frame, stack ); break;
                            } break; } break;
                        case kSrcCloseValue: { switch (move.src_.v1src_) {
                                case kSrcUpval:   Mover<kSrcUpval,kSrcCloseValue>  ::domove( move, move.src_, pThread, frame, stack ); break;
                                case kSrcLiteral: Mover<kSrcLiteral,kSrcCloseValue>::domove( move, move.src_, pThread, frame, stack ); break;
                            } break; } break;
                    }
            }
            OPCODE_END();
                
        OPCODE_LOC(kTCOApplyProgram):
                {
                    const Ast::Program* afn = reinterpret_cast<const Ast::Program*>(pInstr); 
                    frame.rebind( TMPFACT_PROG_TO_RUNPROG(afn) );
                    goto restartTco;
                }
            OPCODE_END();

            OPCODE_LOC(kApplyProgram):
                {
                    const Ast::Program* afn = reinterpret_cast<const Ast::Program*>(pInstr);
                    inprog = afn;
                    inupvalues = frame.upvalues_;
                    goto restartNonTail;
                }
            OPCODE_END();

#define KTHREAD_CASE \
                case Value::kThread: { auto other = v.tothread().get(); other->setcallin(pThread); xferstack(pThread,other); pThread = other; goto restartThread; }
                
                /* 150629 Coroutine issue:  Currently, coroutine yield returns to creating thread, not calling thread. */

            OPCODE_LOC(kApplyThingAndValue2ValueOperator):
                {
//                     static void* dispatch_table_tav2v = { &&OPCODE_2LOC(tav2v,kSrcStack), 0, 0, 0,
//                                          &&OPCODE_2LOC(tav2v,kSrcRegister),
//                                          &&OPCODE_2LOC(tav2v,kSrcRegisterBool),
//                                          &&OPCODE_2LOC(tav2v,kSrcCloseValue) };
                    const Ast::ApplyThingAndValue2ValueOperator& pa = *reinterpret_cast<const Ast::ApplyThingAndValue2ValueOperator*>(pInstr);
//                    goto *dispatch_table_tav2v[pa.dest_];
                    switch (pa.dest_)
                    {
                        case kSrcStack:        ApplyThingValueCall<kSrcStack>       ::docall( stack, pThread, frame, pa ); break;
                        case kSrcRegister:     ApplyThingValueCall<kSrcRegister>    ::docall( stack, pThread, frame, pa ); break;
                        case kSrcRegisterBool: ApplyThingValueCall<kSrcRegisterBool>::docall( stack, pThread, frame, pa ); break;
                        case kSrcCloseValue:   ApplyThingValueCall<kSrcCloseValue>  ::docall( stack, pThread, frame, pa ); break;
                    }
                }
            OPCODE_END();

#if DOT_OPERATOR_INLINE
            OPCODE_LOC(kApplyIndexOperator):
                {
                    auto op = reinterpret_cast<const Ast::ApplyIndexOperator*>(pInstr);
                    auto rc = frame;
                    
                    // now apply
                    switch (op->v1src_)
                    {
                        case kSrcUpval:
                        {
                            const Value& owner = FRVE2UV(frame, *op); 
//                             std::cerr << "kApplyIndexOperator, owner=";
//                             owner.dump(std::cerr);
//                             std::cerr << " indexV st=" << op->indexValue_.sourceType() << std::endl;
                            switch (op->indexValue_.sourceType())
                            {
                                case kSrcLiteral:  owner.applyIndexOperator( op->indexValue_.v1literal_, stack, frame ); break;
                                case kSrcStack:    owner.applyIndexOperator( stack.pop(), stack, frame ); break;
                                case kSrcUpval:    owner.applyIndexOperator( FRVE2UV(frame, op->indexValue_), stack, frame ); break;
                                case kSrcRegister: owner.applyIndexOperator( frame.thread->r0_, stack, frame ); break;
                                default: break;
                            }
                        }
                        break;
                        
                        case kSrcStack:
                        {
                            const Value& owner = stack.pop();
                            switch (op->indexValue_.sourceType())
                            {
                                case kSrcLiteral:  owner.applyIndexOperator( op->indexValue_.v1literal_, stack, frame ); break;
                                case kSrcStack:    owner.applyIndexOperator( stack.pop(), stack, frame ); break;
                                case kSrcUpval:    owner.applyIndexOperator( FRVE2UV(frame, op->indexValue_ ), stack, frame ); break;
                                case kSrcRegister: owner.applyIndexOperator( frame.thread->r0_, stack, frame ); break;
                                default: break;
                            }
                        }
                        break;
                        
                        case kSrcAltStack:
                        {
                            const Value& owner = pThread->altstack.pop();
                            switch (op->indexValue_.sourceType())
                            {
                                case kSrcLiteral:  owner.applyIndexOperator( op->indexValue_.v1literal_, stack, frame ); break;
                                case kSrcStack:    owner.applyIndexOperator( stack.pop(), stack, frame ); break;
                                case kSrcUpval:    owner.applyIndexOperator( FRVE2UV(frame, op->indexValue_), stack, frame ); break;
                                case kSrcRegister: owner.applyIndexOperator( frame.thread->r0_, stack, frame ); break;
                                default: break;
                            }
                        }
                        break;
                    }
                }
            OPCODE_END();
#endif 
                
            OPCODE_LOC(kTCOApply):
                {
                    auto op = reinterpret_cast<const Ast::Apply*>(pInstr);
                    switch( op->v1src_ )
                    {
                        case kSrcStack:
                        {
                            const Value& v = stack.pop();
                            switch (v.type())
                            {
                                default: RunApplyValue( pInstr, v, stack, frame ); break;
                                    //~~~ should this setcallin? as with KTHREAD_CASE? or is it intentionally different
                                case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                                case Value::kBoundFun:
                                    auto pbound = v.toboundfunhold();
                                    frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                                    goto restartTco;
                            }
                        }
                        break;
                        case kSrcUpval:
                        {
                            const Value& v = FRVE2UV(frame, *op);
                            switch (v.type())
                            {
                                default: RunApplyValue( pInstr, v, stack, frame ); break;
                                    //~~~ should this setcallin? as with KTHREAD_CASE? or is it intentionally different
                                case Value::kThread: { auto other = v.tothread().get(); xferstack(pThread,other); pThread = other; goto restartThread; }
                                case Value::kBoundFun:
                                    auto pbound = v.toboundfunhold();
                                    frame.rebind( TMPFACT_PROG_TO_RUNPROG(pbound->program_), pbound->upvalues_ );
                                    goto restartTco;
                            }
                        }
                        break;
                    }
                }
            OPCODE_END();

            OPCODE_LOC(kApply):
                {
                    auto op = reinterpret_cast<const Ast::Apply*>(pInstr);
                    switch( op->v1src_ )
                    {
                        case kSrcStack:
                        {
                            const Value& v = stack.pop();
                            switch (v.type())
                            {
                                default: RunApplyValue( pInstr, v, stack, frame ); break;
                                    KTHREAD_CASE    
                                case Value::kBoundFun:
                                auto pbound = v.toboundfun();
                                inprog = pbound->program_;
                                inupvalues = pbound->upvalues_;
                                goto restartNonTail;
                            }
                        }
                        break;
                        case kSrcUpval:
                        {
                            const Value& v = FRVE2UV(frame, *op); //
                            switch (v.type())
                            {
                                default: RunApplyValue( pInstr, v, stack, frame ); break;
                                    KTHREAD_CASE    
                                case Value::kBoundFun:
                                auto pbound = v.toboundfun();
                                inprog = pbound->program_;
                                inupvalues = pbound->upvalues_;
                                goto restartNonTail;
                            }
                        }
                        break;
                    }
                }
            OPCODE_END();
#if LCFG_COMPUTED_GOTO
            } while( 0 );
#else
            } // end,instr switch
#endif 
        } // end, while loop incrementing PC

    }
    catch (const std::runtime_error& e)
    {
        // duplicate code from kThrow
#if LCFG_HAVE_TRY_CATCH        
                    RunContext *pframe = &frame;

                    while (pframe && !pframe->catcher)
                    {
                        RunContext* prev = pframe->prev;
//                        std::cerr << "tossing frame, frame=" << pframe << " prev=" << prev << std::endl;
                        pframe->~RunContext();
                        pThread->rcAlloc_.deallocate( pframe, sizeof(RunContext) );
                        pframe = prev;
                    }
                    
                    if (pframe && pframe->catcher)
                    {
                        pThread->callframe = pframe;
//                        std::cerr << "running catch block frame, frame=" << pframe << std::endl;
                        pframe->rebind( TMPFACT_PROG_TO_RUNPROG(pframe->catcher), inupvalues );
                        goto restartReturn;
                    }
                    else
#endif 
                    {
        throw AstExecFail( *(frame.ppInstr), e.what() );
//                        bangerr() << "Unhandled exception";
                    }
        
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


//     DLLEXPORT void FromCStackRunProgramInCurrentContext( const RunContext& rc, const Ast::Program* prog )
//     {
//         Bang::Thread* bthread = rc.thread;
//         // this is an awful mess.
//         auto prevcaller = bthread->pCaller;
//         auto prevcf = bthread->callframe;
//         bthread->pCaller = nullptr;
//         bthread->callframe = nullptr;
//         // auto bprog = v->toboundfun(); 
//         Bang::RunProgram( bthread, prog, rc.upvalues_ );
//         bthread->pCaller = prevcaller;
//         bthread->callframe = prevcf;
//     }
    
    
    void Function::customOperator( const bangstring& theOperator, Stack& s)
    {
        bangerr() << "Vanilla Function does not support customOperator=" << theOperator;
    }

    void Function::indexOperator( const Value& theIndex, Stack& stack, const RunContext& ctx )
    {
        stack.push( theIndex );
        this->apply( stack );
    }

    void BoundProgram::indexOperator( const Value& theIndex, Stack& stack, const RunContext& ctx )
    {
        stack.push( theIndex );
        CallIntoSuspendedCoroutine( ctx.thread, this );
    }
    
    
    void BoundProgram::apply( Stack& s )
    {
        // this is handled in main RunProgram machine
        throw std::runtime_error("BoundProgram::Apply should not be called");
    }

    
    void Ast::Program::run( Stack& stack, const RunContext& rc ) const
    {
        stack.push( NEW_BANGFUN(BoundProgram, this, rc.upvalues() ) );
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
    stack.push( NEW_BANGFUN(BoundProgram, pRecFun_, uv ) ); //STATIC_CAST_TO_BANGFUN(newfun) );
}

void Ast::PushPrimitive::run( Stack& stack, const RunContext& rc ) const
{
    if (apply_)
        primitive_( stack, rc );
    else
        stack.push( primitive_ );
};

void Ast::Move::run( Stack& stack, const RunContext& rc) const
{
    bangerr() << "Move::run() should not be called";
//     if (hasApply())
//         bangerr() << "should not be calling run for ApplyUpval";
//     stack.push( rc.getUpValue( uvnumber_ ) );
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

    virtual std::string sayWhere() const {
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
        {
            std::cerr << "last char=" << lastc << " regurged=" << c << std::endl;
            throw std::runtime_error("parser regurged char != last");
        }
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
        if (!f_)
            bangerr() << "Cannot open file=" << filename;
    }
    ~RegurgeFile()
    {
        fclose(f_);
    }
    virtual std::string sayWhere() const
    {
        std::ostringstream oss;
        oss << filename_ << ":" << loc_.lineno_ << " C" << loc_.linecol_;
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
        Ast::Program::astList_t& ast_;
    public:
        Program( ParsingContext& pc, StreamMark&, const Ast::Program* parent,
            const Ast::CloseValue* upvalueChain, const Ast::CloseValue* const entryUvChain,
            ParsingRecursiveFunStack* pRecParsing, Ast::Program::astList_t& );

        // const Ast::Program::astList_t& ast() { return ast_; }
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

    static bool isoperatorchar( char c )
    {
        auto found = strchr( "+-<>=/*@$%^&~", c );
        return found ? true : false;
    }

    static bool operatorterminator( char c )
    {
        if (isspace(c))
            return true;
        
        auto found = strchr( "];}).!", c );
        return found ? true : false;
    }
    
    class OperatorToken
    {
        std::string name_;
    public:
        OperatorToken( StreamMark& stream )
        {
            StreamMark mark(stream);
            char c = mark.getc();
            
            if (!isoperatorchar(c))
                throw ErrorNoMatch();

            do
            {
                name_.push_back(c);
                mark.accept();
                c = mark.getc();
            }
            while (!operatorterminator(c));
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
//         Ast::Program::astList_t& parentProgramAst() {
//             return *(parentProgram->getAst());
//         }
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

//            Ast::Program::astList_t functionAst;
            pNewProgram_ = new Ast::Program( nullptr ); // , functionAst );
            const Ast::CloseValue* const entryUvChain = upvalueChain;
            upvalueChain = getParamBindings( mark, pNewProgram_->astRef(), upvalueChain );

            //~~~ programParent??
            Program program( parsectx, mark, nullptr, upvalueChain, entryUvChain, pRecParsing, pNewProgram_->astRef() );
//             const auto& subast = program.ast();
//             std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );

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

//            Ast::Program::astList_t functionAst;
            pDefProg_ = new Ast::Program(nullptr);
            const Ast::CloseValue* const entryUvChain = upvalueChain;
            upvalueChain = getParamBindings( mark, pDefProg_->astRef(), upvalueChain );

            // std::string defFunName = pWithDefFun_->getParamName();
            ParsingRecursiveFunStack recursiveStack( pRecParsing, pDefProg_, lastParentUpvalue, *defname_ ); // , pWithDefFun_);
            Program progdef( parsectx, mark, nullptr, upvalueChain, entryUvChain, &recursiveStack,  pDefProg_->astRef() ); // pDefFun_, &recursiveStack );
//            const auto& subast = progdef.ast();
//            std::copy( subast.begin(), subast.end(), std::back_inserter(functionAst) );
//            pDefProg_->setAst( functionAst );
            mark.accept();
        }

        bool hasPostApply() const { return postApply_; }
        Ast::Program* stealDefProg() { auto rc = pDefProg_; pDefProg_ = nullptr; return rc; }
        const std::string& getDefName() { return *defname_; }
    }; // end, Defdef class
    
    Program* program_;
    Ast::Program::astList_t programAst_;
public:
    Parser( ParsingContext& ctx, StreamMark& mark, const Ast::Program* parent )
    {
        program_ = new Program( ctx, mark, parent, nullptr, nullptr, nullptr, programAst_ ); // 0,0,0 = no upvalues, no recursive chain
    }

    Parser( ParsingContext& ctx, StreamMark& mark, const Ast::CloseValue* upvalchain )
    {
        program_ = new Program( ctx, mark, nullptr /*parent*/, upvalchain, upvalchain, nullptr, programAst_ ); // 0,0,0 = no upvalues, no recursive chain
    }
    
    const Ast::Program::astList_t& programAst()
    {
        return programAst_;
    }
};

    Ast::Base* ImportParsingContext::hitEof( const Ast::CloseValue* uvchain )
    {
        Parser p( parsecontext_, stream_, uvchain );

        auto prog = new Ast::Program( parent_, p.programAst() );  //~~~todo: refactor me up
        prog->setApply();
        return prog;
    }
    

void OptimizeAst( std::vector<Ast::Base*>& ast, const Ast::CloseValue* upvalueChain )
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
    
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        Ast::Base* first = ast[i];
        Ast::Base* second = ast[i+1];
        
        Ast::IsApplicable* pup = dynamic_cast<Ast::IsApplicable*>( first );
        
        if (pup && !pup->hasApply() && dynamic_cast<const Ast::Apply*>( second ))
        {
            pup->setApply();
            first->where_ = second->where_;
            ast[i+1] = &noop;
            ++i; // increment i an additional time to skip the incorporated Apply
        }
    }
    
    delNoops();

    // fix up index operator by moving literals into ApplyIndexOperator
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        Ast::ApplyIndexOperator* pIndex  = dynamic_cast<Ast::ApplyIndexOperator*>(ast[i+1]);
        if (pIndex)
        {
            const Ast::Move* pmove = dynamic_cast<const Ast::Move*>(ast[i]);
            if (pmove && pmove->destIsStack() && !pIndex->srcIsStack() && pIndex->indexValueSrcIsStack())
            {
                pIndex->setIndexValueSource( pmove->source() );
                ast[i] = &noop;
                ++i;
            }
            continue;
        }
    }
    delNoops();

    // fix up index operators by removing use of altstack where possible
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        const Ast::StackToAltstack* pAlt = dynamic_cast<const Ast::StackToAltstack*>(ast[i]);
        if (pAlt)
        {
            Ast::ApplyIndexOperator* pIndex  = dynamic_cast<Ast::ApplyIndexOperator*>(ast[i+1]);
            if (pIndex && pIndex->srcIsAltStack())
            {
                pIndex->setSrcStack();
                ast[i] = &noop;
                ++i;
            }
            continue;
        }
    }
    delNoops();
    
    

#if LCFG_OPTIMIZE_OPVV2V_WITHLIT
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        const Ast::Move* pmove = dynamic_cast<const Ast::Move*>(ast[i]);
        if (pmove && pmove->destIsStack())
        {
            Ast::ApplyThingAndValue2ValueOperator* pTav2v = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[i+1]);
            if (pTav2v && pTav2v->v1src_ == kSrcStack)
            {
                pTav2v->setThingVeSrc( pmove->source() );
                ast[i] = &noop;
                ++i;
                continue;
            }
            
            Ast::ValueEater* ve = dynamic_cast<Ast::ValueEater*>(ast[i+1]);
            if (ve && ve->srcIsStack())
            {
                ve->setSrcOther( pmove->source() );
                ast[i] = &noop;
                ++i;
                continue;
            }
        }
    }
    delNoops();


    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        const Ast::Move* pmove = dynamic_cast<const Ast::Move*>(ast[i]);
        if (pmove && pmove->destIsStack())
        {
            Ast::SecondValueEater* sve = dynamic_cast<Ast::SecondValueEater*>(ast[i+1]);
            if (sve && sve->firstValueNotStack() && sve->secondValueIsStack())
            {
                sve->setSecondSrcOther( pmove->source() );
                ast[i] = &noop;
                ++i;
            }
            continue;
        }
    }

    delNoops();

    
    /* IF first is a Bool maker and second is a Bool eater, then set src/dest to Bool register
     * If first is a Value maker and second is a Value eater, set src/dest to Value register */
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        Ast::ValueMaker* pfirst = dynamic_cast<Ast::ValueMaker*>(ast[i]);
        if (pfirst && pfirst->dest_ == kSrcStack)
        {
            Ast::ApplyThingAndValue2ValueOperator* psecond = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[i+1]);
            if (psecond)
            {
                if (psecond->v1src_ == kSrcStack)
                {
                    pfirst->setDestRegister();
                    psecond->setThingRegister();
                }
            }
            else
            {
                Ast::CloseValue* psecond = dynamic_cast<Ast::CloseValue*>(ast[i+1]);
                if (psecond)
                {
                    pfirst->setDestCloseValue( psecond );
                    ast[i+1] = &noop;
                    ++i;
                }
                else
                {
                    Ast::BoolEater* psecond = dynamic_cast<Ast::BoolEater*>(ast[i+1]);
                    if (psecond)
                    {
                        pfirst->setDestRegisterBool();
                        psecond->setSrcRegisterBool();
                    }
                    else
                    {
                        Ast::ApplyIndexOperator* psecond = dynamic_cast<Ast::ApplyIndexOperator*>(ast[i+1]);
                        if (psecond)
                        {
                            if (psecond->indexValueSrcIsStack() && !psecond->srcIsStack())
                            {
                                pfirst->setDestRegister();
                                psecond->setIndexValueSrcRegister();
                            }
                        }
                    }
                }
            }
        }
        else
        {
            Ast::BoolMaker* pfirst  = dynamic_cast<Ast::BoolMaker*>(ast[i]);
            Ast::BoolEater* psecond = dynamic_cast<Ast::BoolEater*>(ast[i+1]);
            if (pfirst && psecond)
            {
                pfirst->setDestRegisterBool();
                psecond->setSrcRegisterBool();
            }
        }
    }
    delNoops();

#if 1
    for (unsigned i = 0; i < ast.size() - 1; ++i)
    {
        Ast::ValueMaker* pfirst = dynamic_cast<Ast::ValueMaker*>(ast[i]);
        if (pfirst && pfirst->dest_ == kSrcStack)
        {
            Ast::ApplyThingAndValue2ValueOperator* psecond = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[i+1]);
            if (psecond && psecond->secondValueIsStack() && psecond->v1src_ != kSrcStack)
            {
                pfirst->setDestRegister();
                psecond->setOtherRegister();
            }
        }
    }
#endif

    

    for (unsigned i = 2; i < ast.size(); ++i)
    {
        Ast::ApplyThingAndValue2ValueOperator* pfirst = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[i]);
        if (pfirst && pfirst->secondValueIsStack() && pfirst->firstValueNotStack())
        {
            unsigned j = i - 1;
            for (; j > 0; --j)
            {
                bool otherstackuser = false;
                Ast::ApplyThingAndValue2ValueOperator* psecond = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[j]);
//                std::cout << "checking j=" << j << " psecond=" << psecond << " srcother_=" << psecond->srcother_ << " srcthing_=" << psecond->srcthing_ << '\n';
                if (psecond && (!psecond->secondValueIsStack() && psecond->firstValueNotStack() && psecond->dest_ != kSrcStack))
                    continue;
                else
                    break;
            }
            if (j < i - 1)
            {
                Ast::Move* pmove = dynamic_cast<Ast::Move*>(ast[j]);
                if (pmove && pmove->destIsStack())
                {
                    pfirst->setSecondSrcOther( pmove->source() );
                    ast[j] = &noop;
                    delete pmove;
                    continue;
                }
            }
        }
    }
    delNoops();



#if LCFG_INTLITERAL_OPTIMIZATION
    for (unsigned i = 0; i < ast.size(); ++i)
    {
        Ast::ApplyThingAndValue2ValueOperator* op = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[i]);
        if
        (op && op->v1src_ == kSrcLiteral && op->v1literal_.type() == Value::kNum
        &&  (op->secondValueSrc().v1src_ == kSrcUpval
            || (op->secondValueSrc().v1src_ == kSrcRegister && op->dest_ == kSrcRegister)
            )
        )
        {
            if (( op->openum_ == kOpPlus || op->openum_ == kOpMult || op->openum_ == kOpDiv || op->openum_ == kOpMinus ) )
            {
                if (op->dest_ == kSrcRegister)
                {
                    std::vector<Ast::ApplyThingAndValue2ValueOperator*> ops_;
                    ops_.push_back( op );
                    Litnumop2reg jitme;
                
                    if (op->secondValueSrc().v1src_ == kSrcRegister)
                        jitme.loadFromRegister();
                    else // upval
                        jitme.loadFromUpval( op->secondValueSrc().v1uvnumber_ );
                    
                    const double vv = op->v1literal_.tonum();

                    jitme.addOperation( vv, op->openum_ );
                    
                    // combine multiple subsequent register + literal -> register operations
                    // into a single jitted function.  This is quite the thing.
                    for (unsigned j = i + 1; j < ast.size(); ++j)
                    {
                        Ast::ApplyThingAndValue2ValueOperator* op = dynamic_cast<Ast::ApplyThingAndValue2ValueOperator*>(ast[j]);
                        if (!op)
                            break;
                        const bool isCommuteOp = ( op->openum_ == kOpPlus || op->openum_ == kOpMult );
                        const bool isMathOp = ( isCommuteOp || op->openum_ == kOpDiv || op->openum_ == kOpMinus );
                        const bool isOtherDestReg = op->secondValueSrc().v1src_ == kSrcRegister && op->dest_ == kSrcRegister;
//                        std::cerr << "commute="  << isCommuteOp << " math=" << isMathOp << " otherreg=" << isOtherDestReg << "\n"; op->dump(9,std::cerr);
                        if
                        (op && op->v1src_ == kSrcLiteral && op->v1literal_.type() == Value::kNum
                        &&  isOtherDestReg && isMathOp
                        )
                        {
                            jitme.addOperation( op->v1literal_.tonum(), op->openum_ );
                            ops_.push_back( op );
                            ast[j] = &noop;
                        }
                        else if
                        (op && op->v1src_ == kSrcUpval
                        &&  isOtherDestReg && isCommuteOp
                        )
                        {
                            jitme.addUpvalOp( op->v1uvnumber_, op->openum_ );
                            ops_.push_back( op );
                            ast[j] = &noop;
                        }
                        else if
                        (op && op->v1src_ == kSrcRegister && op->dest_ == kSrcRegister
                        &&  op->secondValueSrc().v1src_ == kSrcLiteral && op->secondValueSrc().v1literal_.type() == Value::kNum
                        )
                        {
                            jitme.addReverseOperation( op->secondValueSrc().v1literal_.tonum(), op->openum_ );
                            ops_.push_back( op );
                            ast[j] = &noop;
                        }
                        else
                            break;
                    }
                
                    auto jitfun = jitme.closeitup();
                    auto lno = new Ast::Increment( jitfun, op->openum_, op->secondsrc_, *op ); // == kOpMinus) ? -vv :
                                                                                             // vv
                    lno->origOps_ = ops_;
                    ast[i] = lno;
                }
//                *static_cast<Ast::ValueMaker*>(lno) = *static_cast<Ast::ValueMaker*>(op);
//                *static_cast<Ast::ValueEater*>(lno) = op->secondsrc_;
            }
        }
    }
    delNoops();
#endif 


    
#endif 

    
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

//        { static bool isinit = false; if (!isinit) { initPrimitiveOperators(); isinit = true; } }

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

    DLLEXPORT Ast::Program* RequireKeyword::parseToProgramWithUpvals( ParsingContext& ctx, const Ast::CloseValue* uvchain, bool bDump )
    {
        Ast::Program* fun;

        RegurgeFile strmFile( fileName_ );
        try
        {
            fun = ParseToProgram( ctx, strmFile, bDump, uvchain );
        }
        catch( const ParseFail& e )
        {
            bangerr(ParseFail) << "e01a Parsing: " <<  e.what();
        }

        return fun;
    }
    
    DLLEXPORT Ast::Program* RequireKeyword::parseToProgramNoUpvals( ParsingContext& ctx, bool bDump )
    {
        return parseToProgramWithUpvals( ctx, nullptr, bDump );
    }


    EOperators optoken2enum( const std::string& op )
    {
        if (op.size() != 1)
            return kOpCustom;

        switch (op.front())
        {
            case '+': return kOpPlus;
            case '-': return kOpMinus;
            case '<': return kOpLt;
            case '>': return kOpGt;
            case '=': return kOpEq;
            case '*': return kOpMult;
            case '/': return kOpDiv;
            case '%': return kOpModulo;
            default: return kOpCustom;
        };
    }
    
    Ast::Base* setwhere( Ast::Base* ast, const RegurgeStream& s )
    {
        ast->where_ = s.sayWhere();
        return ast;
    }
    Ast::Base* newapplywhere( const RegurgeStream& s )
    {
        return setwhere( new Ast::Apply(), s );
    }

Parser::Program::Program
(   ParsingContext& parsecontext,
    StreamMark& stream,
    const Ast::Program* parent,
    const Ast::CloseValue* upvalueChain, const Ast::CloseValue* const entryUvChain,
    ParsingRecursiveFunStack* pRecParsing,
    Ast::Program::astList_t& programAst
)
: ast_( programAst )
{
//    std::cerr << "enter Program parent=" << parent << " uvchain=" << upvalueChain << "\n";
//    const Ast::CloseValue* const entryUvChain = upvalueChain;
    try
    {
        bool bHasOpenBracket = false;
        bool bHasOpenIndex = false;
        bool bHasOpenTryCatch = false;
        
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
                ast_.push_back( new Ast::Move(aLiteral.value()) );
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
                    ast_.push_back( newapplywhere(stream) );

                continue;
            } catch ( const ErrorNoMatch& ) {}

            try
            {
                Defdef fun( parsecontext, stream, upvalueChain, pRecParsing );
                
                ast_.push_back( fun.stealDefProg() );
                
                if (fun.hasPostApply())
                    ast_.push_back( newapplywhere(stream) );
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
                ast_.push_back( newapplywhere(mark) );
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
            else if (c == ']') // "Object Method / Message / Dereference / Index" operator
            {
                if (bHasOpenIndex)
                {
                    mark.accept();
                    continue;
                }
                else
                    break;
            }
            else if (c == '[') // "Object Method / Message / Dereference / Index" operator
            {
                bHasOpenIndex = true;
                mark.accept();
                Ast::Program::astList_t indexProgAst;
                Program indexProgram( parsecontext, stream, nullptr, upvalueChain, upvalueChain, pRecParsing, indexProgAst );
                mark.accept();
#if 0
                ast_.push_back( new Ast::ApplyIndexOperator( indexProgram.ast() ) );
                // ast_.push_back( new Ast::Apply() );
#else
                ast_.push_back( new Ast::StackToAltstack() );
                
                const auto astop = indexProgAst;
                for (auto el = astop.begin(); el != astop.end(); ++el)
                {
                    if ((*el)->instr_ != Ast::Base::kBreakProg)
                        ast_.push_back( *el );
                }
//                 auto pp = new Ast::PushPrimitive( &Primitives::swap, "swap" );
//                 pp->setApply();
//                 ast_.push_back( pp );
//                 ast_.push_back( newapplywhere(mark) );
                auto aio = new Ast::ApplyIndexOperator();
                aio->setSrcAltstack();
                ast_.push_back( aio );
                
#endif 

                continue;
            }
            else if (c == '?')
            {
//                std::cerr << "found if-else\n";
                // ast_.push_back( new Ast::ConditionalApply() );
                mark.accept();
                try
                {
                    Ast::Program* ifProg = new Ast::Program( nullptr ); // , ifBranchAst );
                    Program ifBranch( parsecontext, stream, nullptr, upvalueChain, upvalueChain, pRecParsing, ifProg->astRef() );
                    Ast::Program* elseProg = nullptr;
                    eatwhitespace(stream);
                    try
                    {
                        c = mark.getc();
                        if (c != ':')
                        {
                            mark.regurg(c); // no single char operator found
                        }
                        else
                        {
                            mark.accept();
//                        std::cerr << "  found else clause\n";
                            elseProg = new Ast::Program( nullptr ); // , elseBranch.ast() );
                            Program elseBranch( parsecontext, stream, nullptr, upvalueChain, upvalueChain, pRecParsing, elseProg->astRef() );
                        }
                    }
                    catch (const ErrorEof& ) // no else clause found before EOF
                    {
                    }
                    
                    ast_.push_back( new Ast::IfElse( ifProg, elseProg ) );
                }
                catch ( const std::exception& e )
                {
                    std::cerr << "?? error on ifelse: " << e.what() << std::endl;
                }
                catch ( ... )
                {
                    bangerr(ParseFail) << "Parse error in if/else block at " << mark.sayWhere();
                    // bangerr() << "Parse error on ifelse" << std::endl;
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
                    ast_.push_back( new Ast::ApplyIndexOperator( internstring(methodName.name())) );
#else
                    ast_.push_back( new Ast::PushLiteral( Value(methodName.name())) );                    
                    ast_.push_back( new Ast::PushPrimitive( &Primitives::swap, "swap" ) );
                    ast_.push_back( newapplywhere(mark) ); // haveto apply the swap
                    ast_.push_back( newapplywhere(mark) ); // now apply to the object!!
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
                    ast_.push_back( newapplywhere(mark) );
                    continue;
                }
            }
                
            mark.regurg(c); // no single char operator found

            try
            {
                OperatorToken op( mark );
                const std::string& token = op.name();

                // std::cerr << "got operator token=" << op.name() << std::endl;
                mark.accept();
                
                if (token == "~" || token == "/not")
                {
                    ast_.push_back( new Ast::OperatorNot() );
                    continue;
                }
                else if (token == "<~" || token == ">=")
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( kOpLt ) ); 
                    ast_.push_back( new Ast::OperatorNot() );
                    continue;
                }
                else if (token == ">~" || token == "<=")
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( kOpGt ) ); 
                    ast_.push_back( new Ast::OperatorNot() );
                    continue;
                }
                else if (token == "=~" || token == "<>")
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( kOpEq ) ); 
                    ast_.push_back( new Ast::OperatorNot() );
                    continue;
                }
                else if (token == "/and")
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( kOpAnd ) ); 
                    continue;
                }
                else if (token == "/or")
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( kOpOr ) ); 
                    continue;
                }
                else if (token == "^bind")
                {
                    ast_.push_back( new Ast::OperatorBindings( entryUvChain ) );
                    continue;
                }
                else if (token == "^^bind") // not limited to current program scope
                {
                    ast_.push_back( new Ast::OperatorBindings() );
                    continue;
                }
                else if (token == "/throw")
                {
                    ast_.push_back( new Ast::OperatorThrow() );
                    continue;
                }
#if LCFG_HAVE_TAV_SWAP
                else if (token == "~-")
                {
                    auto atav = new Ast::ApplyThingAndValue2ValueOperator( kOpMinus );
                    atav->setArgSwap();
                    ast_.push_back( atav );
                    continue;
                }
                else if (token == "~/")
                {
                    auto atav = new Ast::ApplyThingAndValue2ValueOperator( kOpDiv );
                    atav->setArgSwap();
                    ast_.push_back( atav );
                    continue;
                }
#endif 
                
                EOperators openum = optoken2enum( token );
                if (openum != kOpCustom)
                {
                    ast_.push_back( new Ast::ApplyThingAndValue2ValueOperator( openum ) ); 
                }
                else
                {
                    eatwhitespace(mark);
                    char c = mark.getc();
                    if (c != '.') // "Object Method / Message / Dereference / Index" operator
                    {
                        mark.regurg(c);
                        ast_.push_back( new Ast::ApplyCustomOperator( internstring(token) ) );
                    }
                    else
                    {
                        mark.accept();
                        try
                        {
                            Identifier methodName( mark );
                            mark.accept();
                            ast_.push_back( new Ast::ApplyCustomOperatorDotted( internstring(token), internstring(methodName.name()) ) );
                        }
                        catch( const ErrorNoMatch& )
                        {
                            bangerr(ParseFail) << "valid identifier must follow .operator in " << mark.sayWhere();
                        }
                    }
                }
                continue;
            }
            catch( const ErrorNoMatch& )
            {
            }
            

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
                    Ast::Move* who = dynamic_cast<Ast::Move*>(prev);
                    if (!who || !who->srcIsStr())
                        throw std::runtime_error("import requires a preceding string");
                    ImportParsingContext importContext
                    (   parsecontext, stream,
                        parent, upvalueChain,
                        pRecParsing
                    );
                    RequireKeyword requireImport( who->tostr().c_str() );
                    delete prev;
                    // bah, really need to pass in parent's upvalue chain here
                    auto prog = requireImport.parseToProgramWithUpvals( importContext, upvalueChain, gDumpMode ); // DUMP
                    //~~~ bah, a mess
                    auto importAst = prog->getAst();
                    std::copy( importAst->begin(), importAst->end(), std::back_inserter(ast_));
                    // ast_.push_back( new Ast::Apply() );
                    continue;
                }

                if (ident.name() == "try")
                {
                    mark.accept();
                    try
                    {
                        Ast::Program* ifProg = new Ast::Program( nullptr ); // , ifBranch.ast() );
                        Program ifBranch( parsecontext, stream, nullptr, upvalueChain, upvalueChain, pRecParsing, ifProg->astRef() );
                        
                        eatwhitespace(stream);
                        Identifier idcatch( mark );
                        if (idcatch.name() != "catch")
                            bangerr() << "'catch' must follow 'try'";
                        
                        mark.accept();
                        Ast::Program* elseProg = new Ast::Program( nullptr ); // , elseBranch.ast() );
                        Program elseBranch( parsecontext, stream, nullptr, upvalueChain, upvalueChain, pRecParsing, elseProg->astRef() );
                        
                        ast_.push_back( new Ast::TryCatch( ifProg, elseProg ) );

                        bHasOpenTryCatch = true;
                    }
                    catch (...)
                    {
                        bangerr(ParseFail) << "Parse error in try/catch block at " << mark.sayWhere();
                    }
                    continue;
                }
                
                if (ident.name() == "catch")
                {
                    if (!bHasOpenTryCatch)
                        break;

                    bHasOpenTryCatch = false;
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
                
                if (rwPrimitive( "drop",   &Primitives::drop   ) ) continue;
                if (rwPrimitive( "swap",   &Primitives::swap   ) ) continue;
                if (rwPrimitive( "isafun",   &Primitives::isfun   ) ) continue;
                if (rwPrimitive( "dup",    &Primitives::dup    ) ) continue;
                if (rwPrimitive( "nth",    &Primitives::nth    ) ) continue;
                if (rwPrimitive( "print",   &Primitives::print   ) ) continue;
                if (rwPrimitive( "format",   &Primitives::format   ) ) continue;
                if (rwPrimitive( "save-stack",    &Primitives::savestack    ) ) continue;
#if LCFG_HAVE_SAVE_BINDINGS                
                if (rwPrimitive( "save-bindings",    &Primitives::savebindings    ) ) continue;
#endif 
                if (rwPrimitive( "rebind",    &Primitives::rebindFunction    ) ) continue;
                if (rwPrimitive( "rebind-outer",    &Primitives::rebindOuterFunction    ) ) continue;
                if (rwPrimitive( "crequire",    &Primitives::crequire    ) ) continue;
                if (rwPrimitive( "tostring", &Primitives::tostring    ) ) continue;
            
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

                    const NthParent entryUvNumber = upvalueChain->FindBinding( entryUvChain );
                    if (! (upvalNumber < entryUvNumber) )
                    {
                        auto& cvForUpval = const_cast<Ast::CloseValue*>(upvalueChain)->nthUpvalue( upvalNumber );
                        cvForUpval.incUseCount();
                    }
                    ast_.push_back( new Ast::Move(ident.name(), upvalNumber) );
                }
                
                mark.accept();
            }
            catch( const ErrorNoMatch& )
            {
                bangerr(ParseFail) << " in " << mark.sayWhere() << " unparseable token [" << c << "]";
            }
        } // end, while true

        stream.accept();
        ast_.push_back( new Ast::BreakProg() );
        OptimizeAst( ast_, upvalueChain );
    }
    catch (const ErrorEof&)
    {
        stream.accept();
        ast_.push_back( parsecontext.hitEof( upvalueChain ) );
        OptimizeAst( ast_, upvalueChain );
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
    const auto& closure = NEW_BANGFUN(BoundProgram, fun, noUpvals );

    stack.push( closure ); // STATIC_CAST_TO_BANGFUN(closure) );
    // auto newfun = std::make_shared<FunctionRequire>( s.tostr() );
}

} // end namespace Bang





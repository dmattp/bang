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
#include <vector>
#include <list>
#include <string>
#include <algorithm>
#include <iterator>
#include <memory>

#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sstream>

const char* const BANG_VERSION = "0.001";
const char* const kDefaultScript = "c:\\m\\n2proj\\bang\\tmp\\gurger.bang";

namespace Bang {

class Function;
class FunctionClosure;

#if USE_GC
#include  "gc_cpp.h"
#include "gc_allocator.h"
extern "C" {
# include "private/gcconfig.h"

# ifndef GC_API_PRIV
#   define GC_API_PRIV GC_API
# endif
  GC_API_PRIV void GC_printf(const char * format, ...);
  /* Use GC private output to reach the same log file.  */
  /* Don't include gc_priv.h, since that may include Windows system     */
  /* header files that don't take kindly to this context.               */
}

typedef Function* bangfunptr_t;
typedef FunctionClosure* bangclosure_t;
  #define BANGFUN_CREF bangfunptr_t
  #define CLOSURE_CREF bangclosure_t
  #define STATIC_CAST_TO_BANGFUN(f)  static_cast<Function*>( f )
  #define DYNAMIC_CAST_TO_CLOSURE(f) dynamic_cast<FunctionClosure*>( f )
  #define NEW_CLOSURE(a,b)           new FunctionClosure( a, b )
  #define NEW_BANGFUN(A)             new A
#else
typedef std::shared_ptr<Function> bangfunptr_t;
typedef std::shared_ptr<FunctionClosure> bangclosure_t;
  #define BANGFUN_CREF const bangfunptr_t&
  #define CLOSURE_CREF const bangclosure_t&
  #define STATIC_CAST_TO_BANGFUN(f)  std::static_pointer_cast<Function>(f)
  #define DYNAMIC_CAST_TO_CLOSURE(f) std::dynamic_pointer_cast<FunctionClosure,Function>( f )
  #define NEW_CLOSURE(a,b)           std::make_shared<FunctionClosure>( a, b )
  #define NEW_BANGFUN(A)             std::make_shared<A>
#endif 

#define BANGFUNPTR bangfunptr_t
#define BANGCLOSURE bangclosure_t




namespace {
    // numword - debugging
    // 4 vowels, 16 consonats = 6 bits = CvCvC = 30bits
    unsigned jenkins_one_at_a_time_hash(const char *key, size_t len)
    {
        unsigned hash, i;
        for(hash = i = 0; i < len; ++i)
        {
            hash += key[i];
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }
        hash += (hash << 3);
        hash ^= (hash >> 11);
        hash += (hash << 15);
        return hash;
    }
    // numwords would be great for debugging pointers
    template<class T>
    unsigned PtrToHash( const T& ptr )
    {
        return (reinterpret_cast<unsigned>(ptr)&0xFFF0) >> 4;
//            jenkins_one_at_a_time_hash( reinterpret_cast<const char*>(&ptr), sizeof(T)) & 0xFFF;
    }
}

class Stack;
class RunContext;
class Function;
typedef void (*tfn_primitive)( Stack&, const RunContext& );
namespace Ast {
    class PushPrimitive;
}

class Value
{
    inline void copyme( const Value& rhs )
    {
        switch (rhs.type_)
        {
#if USE_GC            
            case kFun: v_.funptr = rhs.v_.funptr; return;
#else
            case kFun:  new (v_.cfun) std::shared_ptr<Function>( rhs.tofun() ); return;
#endif
            case kStr:  new (v_.cstr) std::string( rhs.tostr() ); return;
            case kBool: v_.b = rhs.v_.b; return;
            case kNum:  v_.num = rhs.v_.num; return;
            case kFunPrimitive: v_.funprim = rhs.v_.funprim; return;
            default: return;
        }
    }
        
    inline void free_manual_storage()
    {
        using namespace std;
#if !USE_GC            
        if (type_ == kFun)
            reinterpret_cast<shared_ptr<Function>*>(v_.cfun)->~shared_ptr<Function>();
        else
#endif 
            if (type_ == kStr)
            reinterpret_cast<string*>(v_.cstr)->~string();
    }
public:
    enum EValueType
    {
        kInvalidUnitialized,
        kBool,
        kNum,
        kStr,
        kFun,
        kFunPrimitive
    };

    union Union {
        bool     b;
        double   num;
        char     cstr[sizeof(std::string)];
#if USE_GC
        bangfunptr_t funptr;
#else
        char     cfun[sizeof(std::shared_ptr<Function>)];
#endif
        tfn_primitive funprim;
    } v_;

    Value( const Value& rhs )
    : type_( rhs.type_ )
    {
        copyme( rhs );
    }

    const Value& operator=( const Value& rhs )
    {
        free_manual_storage();
        type_ = rhs.type_;
        copyme( rhs );
        return *this;
    }

    Value() : type_( kInvalidUnitialized ) {}
    Value( bool b )     : type_( kBool ) { v_.b = b; }
    Value( double num ) : type_( kNum ) { v_.num = num; }

    Value( const std::string& str )
    : type_( kStr )
    {
        new (v_.cstr) std::string(str);
    }

    Value( BANGFUN_CREF f )
    : type_( kFun )
    {
#if USE_GC
        v_.funptr = f;
#else
        new (v_.cfun) std::shared_ptr<Function>(f);
#endif 
    }

    Value( tfn_primitive pprim )
    : type_( kFunPrimitive )
    {
        v_.funprim = pprim;
    }
    
    ~Value()
    {
        free_manual_storage();
    }

    void mutateNoType( double num ) { v_.num = num; } // precondition: previous type was double
    void mutateNoType( bool b ) { v_.b = b; } // precondition: previous type was double
    void mutatePrimitiveToBool( bool b ) // precondition: previous type doesn't require ~destructor
    {
        type_ = kBool;
        v_.b = b;
    }

    BANGFUN_CREF tofun() const
    {
#if USE_GC
        return v_.funptr;
#else
        return *reinterpret_cast<const std::shared_ptr<Function>* >(v_.cfun);
#endif 
    }

    EValueType type_;


    bool isnum()  const { return type_ == kNum; }
    bool isbool() const { return type_ == kBool; }
    bool isfun()  const { return type_ == kFun; }
    bool isstr()  const { return type_ == kStr; }
    bool isfunprim()  const { return type_ == kFunPrimitive; }
    EValueType type() const { return type_; }

    double tonum()  const { return v_.num; }
    bool   tobool() const { return v_.b; }
    const std::string& tostr() const { return *reinterpret_cast<const std::string*>(v_.cstr); }
    tfn_primitive tofunprim() const { return v_.funprim; }
    
    void dump( std::ostream& o ) const
    {
        if (type_ == kNum)
            o << v_.num;
        else if (type_ == kBool)
            o << (v_.b ? "true" : "false");
        else if (type_ == kStr)
            o << '"' << this->tostr() << '"';
        else if (type_ == kFun)
            o << "(function)";
    }
}; // end, class Value

class Stack
{
    Stack( const Stack& ); // uncopyable
    struct Bound {
        int mark;
        Bound* prev;
        Bound( int m, Bound *p ) : mark(m), prev(p) {}
    };
    friend class FunctionRestoreStack;
    friend class FunctionStackToArray;
    std::vector<Value> stack_;
    Bound* bound_;

    void giveTo( std::vector<Value>& other )
    {
        if (!bound_)
            stack_.swap( other );
        else
        {
            for (auto it = stack_.begin() + bound_->mark; it < stack_.end(); ++it )
                other.push_back( *it );
            stack_.erase( stack_.begin() + bound_->mark, stack_.end() );
        }
    }
    
public:
    Stack()
    : bound_(nullptr)
    {}

    void beginBound()
    {
        bound_ = new Bound(stack_.size(), bound_);
    }
    
    void endBound()
    {
        if (bound_)
        {
            Bound* curr = bound_;
            bound_ = curr->prev;
            delete curr;
        }
    }

//     void push( const Value& v )
//     {
//         stack_.emplace_back(v);
//     }
//     template <class T>
//     void push( T v )
//     {
//         stack_.emplace_back( std::forward(v) );
//     }

    void push( const Value& v ) { stack_.push_back( v ); }
    void push( BANGFUN_CREF fun ) { stack_.emplace_back( fun ); }
    void push( double num ) { stack_.emplace_back(num); }
    void push( bool b ) { stack_.emplace_back(b); }
    void push( tfn_primitive fn ) { stack_.emplace_back(fn); }

    const Value& loc_top() const { return stack_.back(); }
    Value& loc_topMutate() { return stack_.back(); }
    
    Value& loc_topMinus1Mutate()
    {
        return *(&stack_[stack_.size()-2]);
    }
    
    Value pop()
    {
        Value v( stack_.back() );
        stack_.pop_back();
        return v;
    }
    void pop_back() { stack_.pop_back(); };
    
    const Value& nth( int ndx ) const
    {
        return stack_[stack_.size()-1-ndx];
    }

    int size() const
    {
        const int totsz = stack_.size();
        return bound_ ? totsz - bound_->mark : totsz;
    }

    void dump( std::ostream& o ) const
    {
        std::for_each
        (   stack_.begin(), stack_.end(),
            [&]( const Value& v ) { v.dump( o ); o << "\n"; }
        );
    }
}; // end, class Stack

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
    // Function& runfun_;
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
            throw std::runtime_error("Binary operator incompatible types");
    }
    
    void plus ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 + v2; } ); }
    void minus( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 - v2; } ); }
    void mult ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 * v2; } ); }
    void div  ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return v1 / v2; } ); }
    void lt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 < v2; } ); }
    void gt   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 > v2; } ); }
    void eq   ( Stack& s, const RunContext& ) { infix2to1bool( s, [](double v1, double v2) { return v1 == v2; } ); }
    void modulo ( Stack& s, const RunContext& ) { infix2to1( s, [](double v1, double v2) { return double(int(v1) % int(v2)); } ); }
    
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
    
    void printone( Stack& s, const RunContext& ctx )
    {
        const Value& v1 = s.pop();

        std::cout << "V=";
        v1.dump( std::cout );
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
        virtual void dump( int, std::ostream& o ) = 0;
        virtual void run( Stack& stack, const RunContext& ) = 0;
        enum EAstInstr {
            kUnk,
            kApply,
            kConditionalApply,
            kApplyUpval
        };
        Base() : instr_(kUnk) {}
        Base( EAstInstr i ) : instr_( i ) {}
        bool isTailable() { return instr_ != kUnk; }
            // return instr_ == kApply || instr_ == kConditionalApply || instr_ == kApplyUpval; }

        EAstInstr instr_;
    private:
    };
}

Ast::Base* gFailedAst = nullptr;
struct AstExecFail {
    Ast::Base* pStep;
    std::runtime_error e;
    AstExecFail( Ast::Base* step, const std::runtime_error& e )
    : pStep(step), e(e)
    {}
};
    

namespace Ast
{
    class PushLiteral : public Base
    {
        Value v_;
    public:
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "PushLiteral v=";
            v_.dump( o );
            o << "\n";
        }
        
        PushLiteral( const Value& v )
        : v_(v)
        {}

        virtual void run( Stack& stack, const RunContext& )
        {
            stack.push( v_ );
        }
    };

    class NoOp: public Base
    {
    public:
        NoOp() {}
        
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "NoOp\n";
        }

        virtual void run( Stack& stack, const RunContext& ) {}
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

        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "PushPrimitive op='" << desc_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& );
    };

    class ApplyPrimitive: public PushPrimitive
    {
    public:
        ApplyPrimitive( PushPrimitive* pp )
        : PushPrimitive( *pp )
        {}

        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "ApplyPrimitive op='" << desc_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& );
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

        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "PushUpval #" << uvnumber_.toint() << " name='" << name_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& );
    };

    class ApplyUpval : public PushUpval
    {
    public:
        ApplyUpval( PushUpval* puv )
        :  PushUpval( *puv )
        {
            instr_ = kApplyUpval;
        }
          
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "ApplyUpval #" << uvnumber_.toint() << " name='" << name_ << "'\n";
        }

        virtual void run( Stack& stack, const RunContext& );

        const Value& getUpValue( const RunContext& rc ) const
        {
            return rc.getUpValue( uvnumber_ );
        }
    };

    class PushUpvalByName : public Base
    {
    public:
        PushUpvalByName() {}
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "PushUpvalByName\n";
        }

        virtual void run( Stack& stack, const RunContext& );
    };

    class Require : public Base
    {
    public:
        Require() {}
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "Require\n";
        }
        virtual void run( Stack& stack, const RunContext& );
    };

    
    class Apply : public Base
    {
    public:
        Apply() : Base( kApply ) {}
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "Apply";
            if (gFailedAst == this)
                o << " ***                                 <== *** FAILED *** ";
            o << std::endl;
        }
        
        virtual void run( Stack& stack, const RunContext& );
        static bool applyOrIsClosure( const Value& v, Stack& stack, const RunContext& );
    };


    class ConditionalApply : public Base
    {
    public:
        ConditionalApply()
        : Base( kConditionalApply )
        {}
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "ConditionalApply\n";
        }
        
        virtual void run( Stack& stack, const RunContext& );

        static bool foundTrue( Stack& stack )
        {
            const Value& vTest = stack.pop();
    
            if (!vTest.isbool())
                throw std::runtime_error("Called conditional apply without boolean");

            return vTest.tobool();
        }
    };

    class PushFun;

    // never loaded directly into program tree - always a member of PushFun
    class Program //  : public Base
    {
    public:
        typedef std::vector<Ast::Base*> astList_t;
    private:
        std::vector<Ast::Base*> ast_;

    public:
        Program( const astList_t& ast )
        : ast_( ast )
        {}

        Program() {} // empty program

        Program& add( Ast::Base* action ) {
            ast_.push_back( action );
            return *this;
        }
        Program& add( Ast::Base& action ) {
            ast_.push_back( &action );
            return *this;
        }
        
        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "Program\n";
            std::for_each
            (   ast_.begin(), ast_.end(),
                [&]( Ast::Base* ast ) { ast->dump( level+1, o ); }
            );
        }

        const std::vector<Ast::Base*>* getAst() { return &ast_; }

        // void run( Stack& stack, std::shared_ptr<Function> pFunction ); 
    }; // end, class Ast::Program
        

    class PushFun : public Base
    {
        PushFun* pParentFun_;
        Ast::Program*  astProgram_;
        std::string* pParam_;

    public:
        Ast::PushFun* parent() { return pParentFun_; }
        
        Ast::Program* getProgram() const { return astProgram_; }

        PushFun( Ast::PushFun* pParentFun )
        : pParentFun_(pParentFun),
          astProgram_( nullptr ),
          pParam_(nullptr)
        {
        }

        void setAst( const Ast::Program& astProgram ) { astProgram_ = new Ast::Program(astProgram); }
        PushFun& setParamName( const std::string& param ) { pParam_ = new std::string(param); return *this; }
        bool hasParam() { return pParam_ ? true : false; }
        std::string getParamName() const { return *pParam_; }
        bool DoesBind( const std::string& varName ) const { return pParam_ && *pParam_ == varName; }

        const NthParent FindBinding( const std::string& varName )
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
        
        virtual void dumpshort( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << std::hex << PtrToHash(this) << std::dec <<
                " PushFun(" << (pParam_ ? *pParam_ : "--") << ")";
//             if (upvalues_.size() > 0)
//                 o << " " << upvalues_.size() << " upvalues";
        }


        virtual void dump( int level, std::ostream& o )
        {
            this->dumpshort( level, o );
            o << ":";  
            astProgram_->dump( level, o );
        }

        virtual void run( Stack& stack, const RunContext& );

        const std::vector<Ast::Base*>* getProgramAst()
        {
            return astProgram_->getAst();
        }
        
    }; // end, class Ast::PushFun


    class PushFunctionRec : public Base
    {
        Ast::PushFun* pRecFun_;
    public:
        PushFunctionRec( Ast::PushFun* other )
        : pRecFun_( other )
        {}

        virtual void dump( int level, std::ostream& o )
        {
            indentlevel(level, o);
            o << "PushFunctionRec[" << std::hex << PtrToHash(pRecFun_) << std::dec << "]" << std::endl;
        }

        virtual void run( Stack& stack, const RunContext& );
    };
    
    
} // end, namespace Ast



        


// Function should be Constructed when the PushFun is pushed,
// thereby binding to the pushing context and preserving upvalues.
// ParamValue will be set when it is invoked, which could happen
// multiple times in theory.
class FunctionClosure;
class Function
#if USE_GC
    : public gc
#endif
{
    bool isClosure_;
    Function( const Function& ); // uncopyable
public:
    Function() : isClosure_(false) {}
    Function( bool isc ) : isClosure_(isc) {};
    virtual ~Function() {}
    bool isClosure() { return isClosure_; }
    virtual void apply( Stack& s, CLOSURE_CREF runningOrMyself ) = 0;
};

// class FunctionPrimitive : public Function
// {
//     tfn_primitive pfn_;
// public:
//     FunctionPrimitive( tfn_primitive pfn ) // for primitives / built-in C functions ***AND MAIN BODY***
//     : pfn_( pfn )
//     {}

//     void apply( Stack& s, const std::shared_ptr<FunctionClosure>& running )
//     {
//         RunContext ctx( running );
//         pfn_( s, ctx );
//     }
// };


//~~~ @todo: maybe separate out closure which binds a variable from
// one which doesn't?  If nothing else this could make upvalue lookup chain
// shorter
class FunctionClosure : public Function
{
    Ast::PushFun* pushfun_;                      // permanent
    BANGCLOSURE pParent_;  // set when pushed??
    Value paramValue_;                          // set when applied??

public:
    static BANGCLOSURE lexicalMatch(BANGCLOSURE start, Ast::PushFun* target)
    {
        
        while (start && start->pushfun_ != target)
            start = start->pParent_;

        // move to highest matching parent to avoid deeply nesting inner loops!
        while (start && start->pParent_ && start->pParent_->pushfun_ == target)
            start = start->pParent_;
        
        return start;
    }
    
    FunctionClosure( Ast::PushFun& pushfun, BANGCLOSURE pParent )
    : Function(true),
      pushfun_( &pushfun ),
      pParent_( FunctionClosure::lexicalMatch( pParent, pushfun.parent() ) ) // pParent_ ) // this is the running Closure which "pushed" this function
//      pParent_( pushfun.activeParentFunction() ) // pParent_ ) // this is the running Closure which "pushed" this function
//      pParent_( pParent_ ) 
    {
        PBIND(std::cerr << "Create closure, Function=" << this << " pushfun=" << std::hex << PtrToHash(pushfun_) << std::dec << "\n";
        if (!pParent)
            std::cerr << " *** no running parent ***\n";
        else        
        {
            std::cerr << "\n  Active/Running/Parent function binds pushfun=";
            pParent->pushfun_->dumpshort( 2, std::cerr );
        };
        if (!pParent_)
            std::cerr << " *** no active pushfun parent ***\n";
        else        
        {
            std::cerr << "\n  Last Active PUSHFUN Parent function=";
                pParent_->pushfun_->dumpshort( 2, std::cerr );
        }
        std::cerr << std::endl);
    }

//    std::shared_ptr<FunctionClosure> parent() { return pParent_; }

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
            std::cerr << "Error Looking for dynamic upval=\"" << uvName << '"';
            throw std::runtime_error("Could not find dynamic upval");
        }
        return getUpValue( uvnum );
    }

    // the pedant in me says "bind" should return a new type, a "BoundFunction" or something;
    // that honestly may clear some of the confusion up... what color is the bike under your desk,
    // object lifetimes and all that, no mutation...
    void bindParams( Stack& s ) 
    {
        if (pushfun_->hasParam()) // bind parameter
            paramValue_ = s.pop();
    }

    const std::vector<Ast::Base*>* getProgramAst()
    {
        return pushfun_->getProgramAst();
    }

    Ast::PushFun* pushfun() { return pushfun_; }

    void apply( Stack& s, CLOSURE_CREF myself );
    
}; // end, class FunctionClosure

class FunctionRestoreStack : public Function
{
    std::shared_ptr< std::vector<Value> > pStack_;
public:
    FunctionRestoreStack( Stack& s )
    : pStack_( std::make_shared<std::vector<Value>>() )
    {
        s.giveTo( *pStack_ );
    }
    FunctionRestoreStack( const std::shared_ptr< std::vector<Value> >& pStack )
    : pStack_( pStack )
    {
    }
    virtual void apply( Stack& s, CLOSURE_CREF running )
    {
        std::copy( pStack_->begin(), pStack_->end(), std::back_inserter( s.stack_ ) );
    }
};

class FunctionStackToArray : public Function
{
    std::shared_ptr< std::vector<Value> > pStack_;
public:
    FunctionStackToArray( Stack& s )
    : pStack_( std::make_shared<std::vector<Value>>() )
    {
        s.giveTo( *pStack_ );
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
            s.push( (*pStack_)[int(msg.tonum())] );
        }
        else if (msg.isstr())
        {
            const auto& str = msg.tostr();
            if (str == "#")
                s.push( double(s.size()) );
            else if (str == "push")
            {
                auto pushit = NEW_BANGFUN(FunctionRestoreStack)( pStack_ );
                s.push( STATIC_CAST_TO_BANGFUN(pushit) );
            }
        }
    }
};


    
// class FunctionRequire : public Function
// {
//     std::string v_;
// public:
//     FunctionRequire( const std::string& v ) : v_(v)
//     {
//         s.giveTo( stack_ );
//     }
//     virtual void apply( Stack& s, const std::shared_ptr<FunctionClosure>& running )
//     {
//     }
// };


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
    BANGCLOSURE pFunction
)
{
restartTco:
    pFunction->bindParams( stack );

    const std::vector<Ast::Base*>* const pAst = pFunction->getProgramAst();

    const int astLen = pAst->size();

    if (astLen < 1)
        return;

    const bool isTailable = ((*pAst)[astLen-1]->isTailable()); //  && pFunction;

    const int lastInstr = isTailable ? astLen - 1 : astLen;

    RunContext rc( pFunction );

    int i = 0;
    try
    {
        for (; i < lastInstr; ++i)
            (*pAst)[i]->run( stack, rc );
    }
    catch (const std::runtime_error& e)
    {
        throw AstExecFail( (*pAst)[i], e );
    }
            
    if (isTailable)
    {
        const Ast::Base* const astApply = (*pAst)[astLen-1];
        bool iscond = (astApply->instr_ == Ast::Base::kConditionalApply);
        bool shouldExec = !iscond || Ast::ConditionalApply::foundTrue( stack );

        const Value& calledFun = (astApply->instr_ == Ast::Base::kApplyUpval)
            ? dynamic_cast<const Ast::ApplyUpval*>(astApply)->getUpValue( rc )
            : stack.pop();
        
        if (shouldExec && Ast::Apply::applyOrIsClosure( calledFun, stack, rc ))
        {
            // "rebind" RunProgram() parameters
            pFunction = DYNAMIC_CAST_TO_CLOSURE(calledFun.tofun());
            goto restartTco;
        } // end , closure
    }
}
    
void FunctionClosure::apply( Stack& s, CLOSURE_CREF myself )
{
    RunProgram( s, myself );
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
void Ast::PushFun::run( Stack& stack, const RunContext& rc )
{
    const auto& pfr = NEW_CLOSURE(*this, rc.running());
    stack.push( STATIC_CAST_TO_BANGFUN(pfr) );
}

class FunctionPossibleTailCall : public Function
{
public:
};

void Ast::PushFunctionRec::run( Stack& stack, const RunContext& rc )
{
#if 0
    // this is weird okay, if we're making a tail call, we *should* reuse the thing; if we're 
    // _not_ making a tail call, you sort of need to re-bind?  It's odd... but I think
    // that for TCO to work properly, we need to know at this point whether to smash
    // over the parent bindings or whether to create new bindings.  Grr, that's ugly but
    // here, i think, is where it finally falls out.  And the only place, I guess, that
    // you really are able to smash the parent bindings is on tail call on the function
    // to itself, _or_ on a tail call on an inner program when that program is itself
    // in tail call position in the function which called it.  That will be fun to
    // get right for sure.  Until then, no true tail calls, sorry!  But at least this
    // memory comes off the heap rather than the stack, so that's something.

    //  maybe do something like walk up the runcontext and as long as everybody
    // through the parent is in tail call position it's okay? ie, set a flag
    // in the runcontext structure when we're in tail call position or something
    // and that way I can check here in this function

    //  But I guess the easy / "scala" TCO is, if the runcontext PushFun _is_
    //  the same as the PushFunctionRec pushFun (ie, I am calling back from
    //  myself) _and_ the runContext says hey, you're in the tail call position
    //  buddy-o, then I can say okay, it's hereby safe to blow away the
    //  previous running thingamajoogie and over-bind his values; there's some
    //  obvious optimization there also that should handle functions with
    //  multiple parameters... 

    //~~~ TODO: This can probably be replaced with Function::lexicalMatch()
//     std::cerr << "TCO inTail=" << rc.inTailMode() << std::hex 
//               << " recfun=" << PtrToHash(pRecFun_) << " RUNfun=" << PtrToHash(rc.running()->pushfun()) << std::dec << "\n";
/// /// ///    if (rc.inTailMode() && (rc.running()->pushfun() == pRecFun_))
/// /// ///    {
/// /// ///        std::cerr << "TCO OPT!!!!!!\n";
/// /// ///        std::shared_ptr<FunctionClosure> parent = FunctionClosure::lexicalMatch(rc.running(), pRecFun_);
/// /// ///        stack.push( Value(std::static_pointer_cast<Function>(parent) ) );
/// /// ///    } 
/// /// ///    else
/// /// ///    {
/// /// ///        const auto& otherfun = std::make_shared<FunctionClosure>( *pRecFun_, rc.running() );
/// /// ///        stack.push( Value(std::static_pointer_cast<Function>(otherfun)) );
/// /// ///    }
#else
    {
        const auto& otherfun = NEW_CLOSURE( *pRecFun_, rc.running() );
        stack.push( STATIC_CAST_TO_BANGFUN(otherfun) );
    }
#endif 
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

void Ast::ApplyUpval::run( Stack& stack, const RunContext& rc)
{
    const Value& v = this->getUpValue( rc );
    if (Ast::Apply::applyOrIsClosure( v, stack, rc ))
        v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
};


void Ast::Apply::run( Stack& stack, const RunContext& rc )
{
    const Value& v = stack.pop();

    if (Ast::Apply::applyOrIsClosure( v, stack, rc ))
        v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
}

void Ast::ConditionalApply::run( Stack& stack, const RunContext& rc)
{
    const bool shouldExec = Ast::ConditionalApply::foundTrue(stack);

    // always pop the function, even if not taken, for consistency's sake
    const Value& v = stack.pop();
    
    if (shouldExec && Ast::Apply::applyOrIsClosure( v, stack, rc ))
        v.tofun()->apply( stack, DYNAMIC_CAST_TO_CLOSURE( v.tofun() ) );
}


void Ast::PushPrimitive::run( Stack& stack, const RunContext& rc )
{
    stack.push( primitive_ );
};

void Ast::ApplyPrimitive::run( Stack& stack, const RunContext& rc )
{
    primitive_( stack, rc );
};


void Ast::PushUpval::run( Stack& stack, const RunContext& rc)
{
    stack.push( rc.getUpValue( uvnumber_ ) );
};

// this nearly lets us create a sort of hash table / record / object system!
//
// This is really sort of 'experiment' code to make a quick&dirty object system /
// immutable record sort of thing
void Ast::PushUpvalByName::run( Stack& stack, const RunContext& rc)
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

    void dump( std::ostream& out, const std::string& context )
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


class RegurgeFile : public RegurgeStream
{
    std::list<char> regurg_;
    FILE* f_;
public:
    RegurgeFile( FILE* f )
    : f_(f)
    {}

    void regurg( char c )
    {
        regurg_.push_back(c);
    }

    void accept() {}

    char getc()
    {
        char rv;

        if (regurg_.empty())
        {
            int i = fgetc(f_);
            if (i==EOF)
                throw ErrorEof();
            rv = i;
        }
        else
        {
            rv = regurg_.back();
            regurg_.pop_back();
        }
        return rv;
    }
};



bool eatwhitespace( StreamMark& stream )
{
    bool bGotAny = false;
    StreamMark mark( stream );
    while (isspace(mark.getc()))
    {
        bGotAny = true;
        mark.accept();
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
            double val = 0;
            double fractional_adj = 0.1;
            char c = mark.getc();

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
            value_ = val;
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
        Program( StreamMark&, Ast::PushFun* pCurrentFun, ParsingRecursiveFunStack* pRecParsing );

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
        
        Fundef( StreamMark& stream, Ast::PushFun* pParentFun, ParsingRecursiveFunStack* pRecParsing )
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
        Defdef( StreamMark& stream, Ast::PushFun* pParentFun, ParsingRecursiveFunStack* pRecParsing )
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
    Parser( StreamMark& mark )
    {
        program_ = new Program( mark, nullptr, nullptr ); // 0,0,0 = no current function, no self-name
    }

    Ast::Program astProgram()
    {
        return Ast::Program( program_->ast() );
    }
};

void OptimizeAst( Ast::Program::astList_t& ast )
{
    static Ast::NoOp noop;
    const int end = ast.size() - 1;
    for (int i = 0; i < end; ++i)
    {
        Ast::Base* step = ast[i];
        if (!dynamic_cast<Ast::ApplyPrimitive*>(step))
        {
            Ast::PushPrimitive* pp = dynamic_cast<Ast::PushPrimitive*>(step);
            if (pp && dynamic_cast<Ast::Apply*>(ast[i+1]))
            {
                ast[i] = new Ast::ApplyPrimitive( pp );
                ast[i+1] = &noop;
                delete step;
                continue;
            }
        }

        if (!dynamic_cast<Ast::ApplyUpval*>(step))
        {
            Ast::PushUpval* pup = dynamic_cast<Ast::PushUpval*>(step);
            if (pup && dynamic_cast<Ast::Apply*>(ast[i+1]))
            {
                 ast[i] = new Ast::ApplyUpval( pup );
                 ast[i+1] = &noop;
                 delete pup;
            }
        }
    }

    for (int i = ast.size() - 1; i >= 0; --i)
    {
        if (ast[i] == &noop)
            ast.erase( ast.begin() + i );
    }
}

namespace Primitives {
    #include "mathlib.cpp"
}


    


Parser::Program::Program( StreamMark& stream, Ast::PushFun* pCurrentFun, ParsingRecursiveFunStack* pRecParsing )
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
                mark.accept();
                ast_.push_back( new Ast::ConditionalApply() );
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

//                    char c = mark.getc();
                    
                    // std::cerr << "Char-after Message='" << c << "'\n";
                    
//                     if (c != '!')
//                         std::runtime_error("Message name must be followed by '!'");

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
                if (rwPrimitive( "save-stack",    &Primitives::savestack    ) ) continue;
                if (rwPrimitive( "stack-to-array",    &Primitives::stackToArray    ) ) continue;
                if (rwPrimitive( "require_math",    &Primitives::require_math    ) ) continue;
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
    }
    catch (const ErrorEof&)
    {
        // std::cerr << "EOF found" << std::endl;
        stream.accept();
    }

    OptimizeAst( ast_ );
}


class RequireKeyword
{
    const std::string fileName_;
public:
    RequireKeyword( const std::string& fn )
    : fileName_(fn)
    {}
    
    Ast::PushFun* parse( RegurgeFile& stream, bool bDump )
    {
        StreamMark mark(stream);

        try
        {
            Parser parser( mark );

            Ast::Program* pProgram = new Ast::Program( parser.astProgram() );

            Ast::PushFun* fun = new Ast::PushFun(0);

            fun->setAst( *pProgram );

            if (bDump)
                pProgram->dump( 0, std::cerr );

            return fun;
        }
        catch (const ErrorEof& )
        {
            std::cerr << "Found EOF without valid program!" << std::endl;
        }
        catch (const std::runtime_error& e )
        {
            std::cerr << "Runtime parse error: " << e.what() << std::endl;
        }

        return nullptr;
    }

    BANGCLOSURE parseToClosure(Stack& stack, CLOSURE_CREF parent, bool bDump )
    {
        FILE* fScript = fopen( fileName_.c_str(), "r");
        RegurgeFile filegetter( fScript );
        Ast::PushFun* fun = this->parse( filegetter, bDump );
        fclose( fScript );
        
        auto closure = NEW_CLOSURE(*fun, parent);

        return closure;
    }
    
    void parseAndRun(Stack& stack, bool bDump)
    {
        try
        {
            BANGCLOSURE noFunction(nullptr);
            auto closure = parseToClosure( stack, noFunction, bDump );
            RunContext rc( noFunction );
            closure->apply( stack, closure );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Runtime exec Error: " << e.what() << std::endl;
        }
        catch( AstExecFail ast )
        {
            gFailedAst = ast.pStep;
            std::cerr << "Runtime AST exec Error: " << ast.e.what() << std::endl;
            //~~~  //~~~  //~~~
            // fun->getProgramAst()->dump( 0, std::cerr );
        }
    }
}; // end, class RequireKeyword


void Ast::Require::run( Stack& stack, const RunContext& rc )
{
    const auto& v = stack.pop(); // get file name
    if (!v.isstr())
        throw std::runtime_error("no filename found for require??");

    RequireKeyword me( v.tostr() );
    BANGCLOSURE noFunction(nullptr);
    const auto& closure = me.parseToClosure( stack, noFunction, false );
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
    std::cerr << "BANG v" << BANG_VERSION << " - hello!" << std::endl;

    bool bDump = false;
    if (argc > 1)
    {
        if (std::string("-dump") == argv[1])
        {
            bDump = true;
            argv[1] = argv[2];
            --argc;
        }
    }
    const char* fname = argv[1];
    if (argc < 2)
    {
        bDump = true;
        fname = kDefaultScript;
    }
    

    Bang::RequireKeyword requireMain( fname );

    Bang::Stack stack;
    requireMain.parseAndRun( stack, bDump );
    stack.dump( std::cout );
    
    std::cerr << "toodaloo!" << std::endl;
}


#include <memory>
#include <vector>
#include <iterator>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <list>
#include <string>
#include <iostream>
#include <string.h>

#define LCFG_STD_STRING 0
#define LCFG_GCPTR_STD 1
#define LCFG_UPVAL_SIMPLEALLOC 1

static const char* const BANG_VERSION = "0.004";

#if defined(WIN32)
# define DLLEXPORT _declspec(dllexport)
#else
# define DLLEXPORT
#endif

#if USE_GC
# include "gc_cpp.h"
# include "gc_allocator.h"
#endif


namespace Bang
{
    class Stack;
    class Function;
    class Upvalue;
    class BoundProgram;
    class RunContext;
    class ParsingContext;
   
    typedef void (*tfn_primitive)( Stack&, const RunContext& );

    namespace Ast { class CloseValue; class Program; class Base; }

    struct Empty {};

    class Thread;


    struct Uncopyable
    {
    private:
        Uncopyable( const Uncopyable& );
    public:
        Uncopyable( Uncopyable&& other ) {}
        Uncopyable() {}
    };

    template <class T>
    class GCDellocator;
    template<>
    class GCDellocator<Upvalue>
    {
    public:
        static DLLEXPORT void freemem(Upvalue*thingmem);
    };

    template <class T>
    class GCDeleter
    {
    public:
        static void deleter( T* thing )
        {
            delete thing;
        }
    };

#if LCFG_UPVAL_SIMPLEALLOC
    template<>
    class GCDeleter<Upvalue>
    {
    public:
        static void deleter( Upvalue* thing );
    };
#endif 

#if LCFG_GCPTR_STD
    typedef std::shared_ptr<Function> gcptrfun;
    typedef std::shared_ptr<Upvalue>  gcptrupval;
# define gcptr std::shared_ptr
    template <class T>
    struct gcbase {};
#else
    template <class T>
    class gcbase : private Uncopyable
    {
        int refcount_;
        void (*deleter_)( T* );
        //~~~ wait, what?  I don't know that this makes sense either, unless the other guy's
        // refcount is 1 (and is about to be decremented); otherwise people are hanging on to a
        // a moved object, and nobody is necessarily referencing the new object, so what should
        // his refcount be?? .  Maybe it makes sense to allow the rest of the objects content to 
        // be moved while just assuming a 'first initialization' refcount of 1 here.
        // But Let's start with it disabled and see how that goes.
        gcbase( gcbase&& other );
        gcbase& operator=( gcbase& other );
//         {
//             refcount_ = other.refcount_;
//         }
    public:
        gcbase()
        : refcount_(0),
          deleter_( &GCDeleter<T>::deleter )
        {}
        void ref()
        {
            ++refcount_;
        }
        void unref()
        {
            if (refcount_ > 1)
                --refcount_;
            else
                deleter_(static_cast<T*>(this));
        }
    };

    
    template <class T> // T must derive from gcbase
    class gcptr
    {
    private:
        T* ptr_;
    public:
        gcptr() : ptr_(nullptr) {}
        
        gcptr( T* ptr )
        : ptr_( ptr )
        {
            if (ptr_) ptr_->ref();
        }
        
        gcptr( const gcptr& other )
        : ptr_( other.ptr_ )
        {
            if (ptr_) ptr_->ref();
        }
        
        ~gcptr()
        {
            if (ptr_) ptr_->unref();
        }

#if 1
        gcptr( gcptr&& other )
        : ptr_( other.ptr_ )
        {
            other.ptr_ = nullptr;
        }
        
        gcptr& operator=( gcptr&& other )
        {
            gcptr( static_cast<gcptr&&>( other ) ).swap(*this);
            return *this;
        }
#endif
        
        gcptr& operator=( const gcptr& other )
        {
            gcptr(other).swap(*this);
            return *this;
        }
        
        gcptr& operator=( T* other )
        {
            gcptr(other).swap(*this);
            return *this;
        }

        T* get() const       { return ptr_; }
        T& operator*() const { return *ptr_; }
        T* operator->() const { return ptr_; }
        
        operator bool () const { return ptr_ ? true : false; }

        void reset()
        {
            gcptr().swap( *this );
        }
        
        void swap(gcptr & other)
        {
            T * tmp = ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = tmp;
        }
    };
    typedef gcptr<Function> gcptrfun;
    typedef gcptr<Upvalue>  gcptrupval;
#endif 

typedef std::shared_ptr<Thread> bangthreadptr_t;
    
#if USE_GC
  typedef Function* bangfunptr_t;
  #define BANGFUN_CREF Bang::bangfunptr_t
  #define STATIC_CAST_TO_BANGFUN(f)  static_cast<Bang::Function*>( f )
  #define NEW_BANGFUN(A)             new A
#else
  typedef gcptrfun bangfunptr_t;
  #define BANGFUN_CREF const Bang::bangfunptr_t&
  #define STATIC_CAST_TO_BANGFUN(f) gcptrfun( reinterpret_cast<const gcptrfun&>(f) )
  #define NEW_BANGFUN(A,...)  gcptr<A>( new A( __VA_ARGS__ ) )
#endif 

#define BANGFUNPTR bangfunptr_t

#define NEW_BANGTHREAD  std::make_shared<Thread>
#define BANGTHREAD_CREF const Bang::bangthreadptr_t&
#define BANGTHREADPTR   bangthreadptr_t


    class Except : public std::runtime_error
    {
    public:
        Except( const std::string& m )
        : std::runtime_error(m)
        {}
    };

    
    struct ParseFail : public Except
    {
        ParseFail( const std::string& msg )
        : Except( msg )
        {}
    };

    struct AstExecFail : public Except
    {
        const Ast::Base* pStep;
        AstExecFail( const Ast::Base* step, const std::string& msg ) //, const std::runtime_error& e )
        : Except(msg),
          pStep(step)
        {}
    };

#if LCFG_STD_STRING
    typedef std::string bangstring;
#else
    class bangstring
    {
        static unsigned jenkins_one_at_a_time_hash(const char *key, size_t len)
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
        static unsigned calchash( const char* str, int len )
        {
            return jenkins_one_at_a_time_hash( str, len > 32 ? 32 : len );
        }
        struct bangstringstore
        {
            int refcount;
            int len;
            unsigned hash;
            char* str;
            bangstringstore( const std::string& stdstr )
            :  refcount(1),
               len( stdstr.size() ),
               hash( calchash( &stdstr.front(), len ) )
            {
                str = new char[len+1];
                std::copy( stdstr.begin(), stdstr.end(), str );
                str[len] = 0;
            }
            ~bangstringstore()
            {
                delete[] str;
            }
            void unref()
            {
                if (refcount > 1)
                    --refcount;
                else
                {
//                    std::cerr << "delete bangstring, v=" << str << std::endl;
                    delete this;
                }
            }
            void ref()
            {
                ++refcount;
            }
            bool operator==( const bangstringstore& rhs ) const
            {
//                 if (this == &rhs)
//                 {
//                     std::cerr << "Got matching bangstringstore, str=" <<  str << "\n";
//                 }
                return
                (  (this == &rhs) ||
                   (  (hash == rhs.hash)
                   && (len == rhs.len)
                   && !memcmp(str, rhs.str, len)
                   )
                );
            }
            bool operator==( const std::string& rhs ) const
            {
                return (len == rhs.size()) && (rhs == str);
            }
            bool operator==( const char* psz ) const
            {
                return !strcmp( str, psz );
            }
        }; // end, bangstringstore class

        bangstringstore* store_;
        
    public:
        ~bangstring()
        {
            if (store_)
                store_->unref();
        }
        unsigned gethash() const { return store_->hash; }
        bangstring( const std::string& other )
        : store_( new bangstringstore(other) )
        {
        }
        bangstring( const bangstring& other )
        : store_( other.store_ )
        {
            store_->ref();
        }
        bangstring( bangstring&& other )
        : store_( other.store_ )
        {
            other.store_ = nullptr;
        }
        const bangstring& operator=( const bangstring& rhs )
        {
            store_->unref();
            store_ = rhs.store_;
            store_->ref();
            return *this;
        }
        const bangstring& operator=( bangstring&& rhs )
        {
            store_->unref();
            store_ = rhs.store_;
            rhs.store_ = nullptr;
            return *this;
        }
        bool operator==( const bangstring& rhs ) const
        {
            return *store_ == *(rhs.store_);
        }
        bool operator==( const char* rhs ) const
        {
            return *store_ == rhs;
        }
        bool operator==( const std::string& rhs ) const
        {
            return *store_ == rhs; // static_cast<const std::string&>(*this) == rhs;
        }

        operator std::string() const {
            //std::cerr << "casting to str" << store_->str << std::endl;
            return std::string( store_->str, store_->len );
        }

        size_t size() const { return store_->len; }
        size_t length() const { return store_->len; }

        const char* c_str() const { return store_->str; }

        bangstring operator+( const bangstring& rhs ) const
        {
            return static_cast<std::string>(*this) + static_cast<std::string>(rhs);
        }
        char operator[]( size_t ndx ) const
        {
            return store_->str[ndx];
        }
        char& front() { return store_->str[0]; }
        const char& front() const { return store_->str[0]; }
    }; // end, bangstring class
#endif 


    class Value
    {
        inline void copyme( const Value& rhs )
        {
            switch (rhs.type_)
            {
#if USE_GC            
                case kFun: v_.funptr = rhs.v_.funptr; return;
#else
                case kBoundFun: // fall through
                case kFun:  new (v_.cfun) gcptrfun( rhs.tofun() ); return;
#endif
                case kStr:  new (v_.cstr) bangstring( rhs.tostr() ); return;
                case kBool: v_.b = rhs.v_.b; return;
                case kNum:  v_.num = rhs.v_.num; return;
                case kFunPrimitive: v_.funprim = rhs.v_.funprim; return;
                case kThread:  new (v_.cthread) std::shared_ptr<Thread>( rhs.tothread() ); return;
                default: return;
            }
        }

        inline void moveme( Value&& rhs )
        {
            switch (rhs.type_)
            {
#if USE_GC            
                case kFun: v_.funptr = rhs.v_.funptr; return;
#else
                case kBoundFun: // fall through
                case kFun: new (v_.cfun) gcptrfun( std::move(rhs.tofun()) ); return;
#endif
                case kStr:  new (v_.cstr) bangstring( std::move(rhs.tostr()) ); return;
                case kBool: v_.b = rhs.v_.b; return;
                case kNum:  v_.num = rhs.v_.num; return;
                case kFunPrimitive: v_.funprim = rhs.v_.funprim; return;
                case kThread:  new (v_.cthread) std::shared_ptr<Thread>( rhs.tothread() ); return;
                default: return;
            }
        }
        
        inline void free_manual_storage()
        {
            using namespace std;
            if (type_ == kStr)
                reinterpret_cast<bangstring*>(v_.cstr)->~bangstring();
#if !USE_GC            
            else if (type_ == kBoundFun || type_ == kFun)
                reinterpret_cast<gcptrfun*>(v_.cfun)->~gcptrfun();
#endif 
            else if (type_ == kThread)
                reinterpret_cast<BANGTHREADPTR*>(v_.cthread)->~shared_ptr<Thread>();
        }
    public:
        enum EValueType
        {
            kInvalidUnitialized,
            kBool,
            kNum,
            kStr,
            kFun,
            kBoundFun,
            kFunPrimitive,
            kThread
        };

        union Union {
            bool     b;
            double   num;
            char     cstr[sizeof(bangstring)];
#if USE_GC
            bangfunptr_t funptr;
#else
            char     cfun[sizeof(gcptrfun)];
#endif
            char cthread[sizeof(BANGTHREADPTR)];
            tfn_primitive funprim;
        } v_;

        Value( const Value& rhs )
        : type_( rhs.type_ )
        {
            copyme( rhs );
        }

        Value( Value&& rhs )
        : type_( rhs.type_ )
        {
            //std::cerr << "got move cons\n";
            moveme( std::move(rhs) );
        }
        

        const Value& operator=( const Value& rhs )
        {
//            std::cerr << "const& operator="; rhs.tostring(std::cerr); std::cerr << "\n";
            free_manual_storage();
            type_ = rhs.type_;
            copyme( rhs );
            return *this;
        }

        const Value& operator=( Value&& rhs )
        {
//            std::cerr << "&& operator="; rhs.tostring(std::cerr); std::cerr << "\n";
            free_manual_storage();
            type_ = rhs.type_;
            moveme( std::move(rhs) );
            return *this;
        }

        Value() : type_( kInvalidUnitialized ) {}
        Value( bool b )     : type_( kBool ) { v_.b = b; }
        Value( double num ) : type_( kNum ) { v_.num = num; }

        Value( const char* str )
        : type_( kStr )
        {
            bangstring* pstr = reinterpret_cast<bangstring*>(v_.cstr);
            new (pstr) bangstring(str);
        }
        
        Value( const bangstring& str )
        : type_( kStr )
        {
//            std::cerr << "Value(const&) str=" << str << "\n";
            // new (v_.cstr) bangstring(str);
            bangstring* pstr = reinterpret_cast<bangstring*>(v_.cstr);
            new (pstr) bangstring(str);
//             std::cerr << "VALUE=[" << *pstr << "]\n";
//             std::cerr << "VALUE2=[" << this->tostr() << "]\n";
        }

        
        Value( bangstring&& str )
        : type_( kStr )
        {
//            std::cerr << "Value(&&) str=" << str << "\n";
            bangstring* pstr = reinterpret_cast<bangstring*>(v_.cstr);
            new (pstr) bangstring(std::move(str));
        }
        
        Value( BANGFUN_CREF f )
        : type_( kFun )
        {
#if USE_GC
            v_.funptr = f;
#else
            new (v_.cfun) gcptrfun(f);
#endif 
        }

        Value( const gcptr<BoundProgram>& bp )
        : type_( kBoundFun )
        {
            new (v_.cfun) gcptr<BoundProgram>(bp);
        }
        
        // these really dont seem to help much, probably for reasons
        // i don't quite understand.  not fully grokking move semantics yet
        Value( BANGFUNPTR&& f )
        : type_( kFun )
        {
#if USE_GC
            v_.funptr = f;
#else
            new (v_.cfun) gcptrfun(std::move(f));
#endif 
        }
        
        Value( tfn_primitive pprim )
        : type_( kFunPrimitive )
        {
            v_.funprim = pprim;
        }

        Value( BANGTHREAD_CREF thread )
        : type_( kThread )
        {
            new (v_.cthread) std::shared_ptr<Thread>(thread);
        }

        ~Value()
        {
            free_manual_storage();
        }

        void mutateNoType( double num ) { v_.num = num; } // precondition: previous type was double
        void mutateNoType( bool b ) { v_.b = b; } // precondition: previous type was bool
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
            return *reinterpret_cast<const gcptrfun* >(v_.cfun);
#endif 
        }

        EValueType type_;

        BoundProgram* toboundfun() const
        {
            return
            reinterpret_cast<BoundProgram*>
            (   reinterpret_cast<const gcptr<BoundProgram>* >(v_.cfun)->get()
            );
        }

        gcptr<BoundProgram> toboundfunhold() const
        {
            return
                //reinterpret_cast<BoundProgram*>
            (   *reinterpret_cast<const gcptr<BoundProgram>* >(v_.cfun)
            );
        }
        
        bool isnum()  const { return type_ == kNum; }
        bool isbool() const { return type_ == kBool; }
        bool isfun()  const { return type_ == kFun; }
        bool isboundfun()  const { return type_ == kBoundFun; }
        bool isstr()  const { return type_ == kStr; }
        bool isfunprim()  const { return type_ == kFunPrimitive; }
        bool isthread()  const { return type_ == kThread; }
        EValueType type() const { return type_; }

        double tonum()  const { return v_.num; }
        bool   tobool() const { return v_.b; }
        const bangstring& tostr() const { return *reinterpret_cast<const bangstring*>(v_.cstr); }
        tfn_primitive tofunprim() const { return v_.funprim; }
        BANGTHREAD_CREF tothread() const {
            return *reinterpret_cast<const std::shared_ptr<Thread>* >(v_.cthread);
        }
        void tostring( std::ostream& ) const;
    
        void dump( std::ostream& o ) const;
    }; // end, class Value

    class NthParent {
        int nth_;
    public:
        explicit NthParent( int n ) : nth_(n) {}
        NthParent operator++() const { return NthParent(nth_+1); }
        NthParent operator--() const { return NthParent(nth_-1); }
        bool operator==( NthParent other ) const { return nth_ == other.nth_; }
        bool operator!=( NthParent other ) const { return nth_ != other.nth_; }
        int toint() const { return nth_; } // use judiciously, please
    };

    class Upvalue;
    typedef gcptrupval SHAREDUPVALUE;
    typedef const gcptrupval& SHAREDUPVALUE_CREF; // avoids ref/unref

    class Upvalue : public gcbase<Upvalue>
    {
    private:
        const Ast::CloseValue* closer_; // contains the symbolic name to which the upvalue is bound
        SHAREDUPVALUE parent_;  // the upvalue chain
        Value v_; // the value itself
    public:
//         ~Upvalue()
//         {
//             parent_.reset();
//         }
        Upvalue( const Ast::CloseValue* closer, SHAREDUPVALUE_CREF parent, const Value& v )
        : closer_( closer ), parent_( parent ), v_( v )
        {
        }

        Upvalue( const Ast::CloseValue* closer, SHAREDUPVALUE_CREF parent, Value&& v )
        : closer_( closer ), parent_( parent ), v_( v )
        {
        }
        
        bool binds( const bangstring& name ) const;

        const Ast::CloseValue* upvalParseChain() const { return closer_; } // needed for REPL/EofMarker::run()

        const Value& getUpValue( NthParent uvnumber ) const
        {
#if 1
            const Upvalue* uv = this;
            while ( uvnumber != NthParent(0) )
            {
                uv = uv->parent_.get();
                uvnumber = --uvnumber;
            }
            return uv->v_;
#else
            return (uvnumber == NthParent(0)) ? v_ : parent_->getUpValue( --uvnumber );
#endif 
        }

        // lookup by string / name is expensive; used for experimental object
        // system.  obviously if it's "a hit" you can optimize out much of this cost
        const Value& getUpValue( const bangstring& uvName ) const;

        SHAREDUPVALUE_CREF nthParent( const NthParent uvnumber ) const
        {
            return (uvnumber == NthParent(1)) ? parent_ : parent_->nthParent( --uvnumber );
        }
    }; // end, Upvalue class


#if LCFG_UPVAL_SIMPLEALLOC
    inline void GCDeleter<Upvalue>::deleter( Upvalue* thing )
    {
//        std::cerr << "delete upvalue\n";
        thing->~Upvalue();
        GCDellocator<Upvalue>::freemem(thing);
    }
#endif 
    
    class Stack
    {
        Stack( const Stack& ); // uncopyable
        struct Bound {
            int mark;
            Bound* prev;
            Bound( int m, Bound *p ) : mark(m), prev(p) {}
        };
         friend class FunctionRestoreStack;
//         friend class FunctionStackToArray;
        std::vector<Value> stack_;
        Bound* bound_;


    public:
        std::back_insert_iterator<std::vector<Value> > back_inserter()
        {
            return std::back_inserter( stack_ );
        }
        void giveTo( std::vector<Value>& other )
        {
            if (stack_.size() < 1)
                return;
            
            if (!bound_)
            {
                for (auto it = stack_.begin(); it < stack_.end(); ++it )
                    other.push_back( *it );
                // other = stack_;
                 stack_.clear();
//                stack_.swap( other );
            }
            else
            {
                for (auto it = stack_.begin() + bound_->mark; it < stack_.end(); ++it )
                    other.push_back( *it );
                stack_.erase( stack_.begin() + bound_->mark, stack_.end() );
            }
        }
        
        void giveTo( Stack& other )
        {
            this->giveTo( other.stack_ );
        }
    
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
        void push( const gcptr<BoundProgram>& bp ) { stack_.emplace_back( bp ); }
//        void push( BANGCLOSURE&& fun ) { stack_.emplace_back( fun ); }
        void push( double num ) { stack_.emplace_back(num); }
        void push( bool b ) { stack_.emplace_back(b); }
        void push( int i ) { stack_.emplace_back(double(i)); }
        void push( tfn_primitive fn ) { stack_.emplace_back(fn); }
        
        void push( const std::string& s ) { stack_.emplace_back(s); }
        void push( std::string&& s ) { stack_.emplace_back(std::move(s)); }
        void push( const char* s ) { stack_.emplace_back(s); }

#if !LCFG_STD_STRING
        void push( const bangstring& s ) { stack_.emplace_back(s); }
        void push( bangstring&& s ) { stack_.emplace_back(std::move(s)); }
        void push_bs( const bangstring& s ) { stack_.push_back(s); }
#endif 
        
        void push( BANGFUN_CREF fun ) { stack_.emplace_back( fun ); }
        void push( BANGFUNPTR&& f ) { stack_.emplace_back(std::move(f)); }

        const Value& loc_top() const { return stack_.back(); }
        Value& loc_topMutate() { return stack_.back(); }
    
        Value& loc_topMinus1Mutate()
        {
            return *(&stack_[stack_.size()-2]);
        }
    
        Value pop()
        {
            Value v( std::move(stack_.back()) );
            stack_.pop_back();
            return v;
        }
        void pop_back() { stack_.pop_back(); };

        bangstring poptostr() {
            bangstring b( std::move(stack_.back().tostr()) );
            stack_.pop_back();
            return b;
        }
    
        const Value& nth( int ndx ) const
        {
            return stack_[stack_.size()-1-ndx];
        }

        int size() const
        {
            const int totsz = stack_.size();
            return bound_ ? totsz - bound_->mark : totsz;
        }

        DLLEXPORT void dump( std::ostream& o ) const;
    }; // end, class Stack


    class Function
#if USE_GC
        : public gc
#else
        : public gcbase<Function>
#endif
    {
        Function( const Function& ); // uncopyable
        Function& operator=(const Function&);
    private:
    public:
//        std::weak_ptr<Function> self_;
        Function() {} // : isClosure_(false) {}
        virtual ~Function() {}
        virtual void apply( Stack& s ) = 0; // CLOSURE_CREF runningOrMyself ) = 0;
    };

    
    namespace Ast {
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
#if DOT_OPERATOR_INLINE            
                kApplyDotOperator,
                kApplyDotOperatorUpval,
#endif 
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

        class BreakProg : public Base
        {
        public:
            BreakProg() : Base( kBreakProg ) {}
            DLLEXPORT virtual void dump( int level, std::ostream& o ) const;
        };
        class EofMarker : public Base
        {
        public:
            ParsingContext& parsectx_;
            DLLEXPORT EofMarker( ParsingContext& ctx );
            DLLEXPORT virtual void repl_prompt( Stack& ) const;
            virtual void report_error( const std::exception& ) const {}
            DLLEXPORT virtual Ast::Program* getNextProgram( SHAREDUPVALUE uv ) const = 0;
            DLLEXPORT virtual void dump( int level, std::ostream& o ) const;
        };
    } // end, Ast namespace

    
    
    // if RunProgram is called outside of an active thread, use pNullThread;
    // this should cause the C-call to RunProgram to return when kBreakProg is found.
DLLEXPORT void RunProgram
(   
    Thread* pThread,
    const Ast::Program* inprog,
    SHAREDUPVALUE inupvalues
);

    DLLEXPORT void CallIntoSuspendedCoroutine( Bang::Thread *bthread, const BoundProgram* bprog );
    

    class RunContext
    {
    public:
        Thread* thread;
        RunContext* prev;
        const Ast::Base* const *ppInstr;
        SHAREDUPVALUE    upvalues_;
    public:    
        SHAREDUPVALUE_CREF upvalues() const;
        SHAREDUPVALUE_CREF nthBindingParent( const NthParent n ) const;
        const Value& getUpValue( NthParent up ) const;
        const Value& getUpValue( const bangstring& uvName ) const;


        RunContext( Thread* inthread, const Ast::Base* const *inppInstr, SHAREDUPVALUE_CREF uv );
        RunContext();
        void rebind( const Ast::Base* const * inppInstr, SHAREDUPVALUE_CREF uv );
        void rebind( const Ast::Base* const * inppInstr );
    };
    
    class BoundProgram : public Function
    {
    public:
        const Ast::Program* program_;
        SHAREDUPVALUE upvalues_;
    public:
        BoundProgram( const Ast::Program* program, SHAREDUPVALUE_CREF upvalues );
        void dump( std::ostream & out );
        virtual void apply( Stack& s );
    };


    class Thread
    {
    public:
        Bang::Stack stack;
        RunContext* callframe;
        Thread* pCaller;
        BANGFUNPTR boundProg_; // probably should have distinct type, BANGBOUNDPROGPTR or something
        Thread()
        : // pInteract( nullptr ),
        callframe( nullptr ),
        pCaller( nullptr )
        {}
        Thread( BANGFUNPTR boundProg )
        : callframe( nullptr ),
          pCaller( nullptr ),
          boundProg_( boundProg  )
        {}
        static DLLEXPORT Thread* nullthread();

        void setcallin(Thread*caller);
    };

    class InteractiveEnvironment
    {
        static void norepl_prompt() {}
    public:
///        bool bEof;
        std::function<void(void)> repl_prompt;
        InteractiveEnvironment()
        : repl_prompt( &InteractiveEnvironment::norepl_prompt )
          //       bEof(false)
        {
        }
    };

    class ParsingContext
    {
    public:
        InteractiveEnvironment interact;
        ParsingContext( InteractiveEnvironment& env )
        : interact( env )
        {}
        ParsingContext() 
        {}
        DLLEXPORT virtual Ast::Base* hitEof( const Ast::CloseValue* uvchain ) = 0;
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

    struct ErrorEof
    {
        ErrorEof() {}
    };

    class RegurgeIo  : public RegurgeStream
    {
        std::list<char> regurg_;
    protected:
        DLLEXPORT int getcud();
    public:
        virtual void regurg( char c )
        {
            regurg_.push_back(c);
        }

        void accept() {}
    };
    
    DLLEXPORT Ast::Program* ParseToProgram( ParsingContext& parsectx, RegurgeStream& stream, bool bDump, const Ast::CloseValue* upvalchain );

    class RequireKeyword
    {
        const std::string fileName_;
        // bool stdin_;
    public:
        RequireKeyword( const char* fname )
        :  fileName_( fname ) // ? fname : "" ),
           // stdin_( !fname )
        {
            // assert(fname);
        }

        DLLEXPORT Ast::Program* parseToProgramNoUpvals( ParsingContext& ctx, bool bDump );
//        std::shared_ptr<BoundProgram> parseToBoundProgramNoUpvals( ParsingContext& ctx, bool bDump );
//        DLLEXPORT void parseAndRun( ParsingContext& ctx, Thread& thread, bool bDump);
    }; // end, class RequireKeyword
    
    
} // end, namespace Bang

#if !LCFG_STD_STRING
inline std::ostream& operator<<( std::ostream& o, const Bang::bangstring& bs )
{
    o << static_cast<const std::string&>(bs);
    return o ;
}

inline Bang::bangstring operator+( const char* s, const Bang::bangstring& bs )
{
    return Bang::bangstring( std::string(s) + static_cast<std::string>(bs) );
}

inline Bang::bangstring operator+( const std::string& s, const Bang::bangstring& bs )
{
    return Bang::bangstring( s + static_cast<std::string>(bs) );
}
#endif


// template <class T>
// static const std::shared_ptr<T>&
// operator^ (bool,const std::shared_ptr<T>& this1 )
// {
// //    this1->self_ = std::static_pointer_cast<Bang::Function>(this1);
//     return this1;
// }




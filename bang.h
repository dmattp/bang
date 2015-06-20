
#include <memory>
#include <vector>
#include <ostream>

#if USE_GC
# include "gc_cpp.h"
# include "gc_allocator.h"
#endif

namespace Bang
{
    class Stack;
    class Function;
    class RunContext;
   
    typedef void (*tfn_primitive)( Stack&, const RunContext& );


    class FunctionClosure;
    
#if USE_GC
typedef Function* bangfunptr_t;
typedef FunctionClosure* bangclosure_t;
  #define BANGFUN_CREF Bang::bangfunptr_t
  #define CLOSURE_CREF Bang::bangclosure_t
  #define STATIC_CAST_TO_BANGFUN(f)  static_cast<Bang::Function*>( f )
  #define DYNAMIC_CAST_TO_CLOSURE(f) dynamic_cast<FunctionClosure*>( f )
  #define NEW_CLOSURE(a,b)           new FunctionClosure( a, b )
  #define NEW_BANGFUN(A)             new A
#else
typedef std::shared_ptr<Function> bangfunptr_t;
typedef std::shared_ptr<FunctionClosure> bangclosure_t;
  #define BANGFUN_CREF const Bang::bangfunptr_t&
  #define CLOSURE_CREF const Bang::bangclosure_t&
  #define STATIC_CAST_TO_BANGFUN(f)  std::static_pointer_cast<Bang::Function>(f)
  #define DYNAMIC_CAST_TO_CLOSURE(f) std::dynamic_pointer_cast<FunctionClosure,Function>( f )
  #define NEW_CLOSURE(a,b)           std::make_shared<FunctionClosure>( a, b )
  #define NEW_BANGFUN(A)             std::make_shared<A>
#endif 

#define BANGFUNPTR bangfunptr_t
#define BANGCLOSURE bangclosure_t


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

        inline void moveme(  Value&& rhs )
        {
            switch (rhs.type_)
            {
#if USE_GC            
                case kFun: v_.funptr = rhs.v_.funptr; return;
#else
                case kFun:  new (v_.cfun) std::shared_ptr<Function>( std::move(rhs.tofun()) ); return;
#endif
                case kStr:  new (v_.cstr) std::string( std::move(rhs.tostr()) ); return;
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

        Value( Value&& rhs )
        : type_( rhs.type_ )
        {
            // std::cerr << "got move cons\n";
            moveme( std::move(rhs) );
        }
        

        const Value& operator=( const Value& rhs )
        {
            free_manual_storage();
            type_ = rhs.type_;
            copyme( rhs );
            return *this;
        }

        const Value& operator=( Value&& rhs )
        {
            free_manual_storage();
            type_ = rhs.type_;
            moveme( std::move(rhs) );
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

        void tostring( std::ostream& ) const;
    
        void dump( std::ostream& o ) const;
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
            {
                 other = stack_;
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
        void push( int i ) { stack_.emplace_back(double(i)); }
        void push( tfn_primitive fn ) { stack_.emplace_back(fn); }
        void push( const std::string& s ) { stack_.emplace_back(s); }

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
    
        const Value& nth( int ndx ) const
        {
            return stack_[stack_.size()-1-ndx];
        }

        int size() const
        {
            const int totsz = stack_.size();
            return bound_ ? totsz - bound_->mark : totsz;
        }

        void dump( std::ostream& o ) const;
    }; // end, class Stack

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
    
} // end, namespace Bang

#include "bang.h"

#include <string>
#include <unordered_map>
#include <iterator>
#include <math.h>
#include <stdlib.h>

#define HAVE_HASH_MUTATION 1

namespace 
{
    using namespace Bang;

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

//     unsigned int luaS_hash (const char *str, size_t l ) { // , unsigned int seed) {
//         unsigned int h = 0xdeadbeef ^ ((unsigned int)(l));
//         size_t l1;
//         size_t step = (l >> 5) + 1;
//         for (l1 = l; l1 >= step; l1 -= step)
//             h = h ^ ((h<<5) + (h>>2) + ((unsigned char)(str[l1 - 1])));
//         return h;
//     }

    class StringHashFunction {
    public:
        ::std::size_t operator ()(const ::std::string& str) const
        {
            const int len = str.length();
            return jenkins_one_at_a_time_hash( &str.front(), len );
//            return luaS_hash( &str.front(), len );
        }
    };

    class BangHash;

    class SetBangHash : public Function
    {
        std::shared_ptr<BangHash> hash_;
    public:
        SetBangHash( const std::shared_ptr<BangHash>& hash )
        : hash_( hash )
        {}
        
        virtual void apply( Stack& s ); // , CLOSURE_CREF running )
    }; // end, SetBangHash
    
    class BangHash : public Function
    {
        std::unordered_map<const std::string,Value,StringHashFunction> hash_;
    public:
        std::weak_ptr<BangHash> myself_;

        BangHash()
        {
        }

        void set( const std::string& key, const Value& v )
        {
            hash_[key] = v;
        }

        virtual void apply( Stack& s ) // , CLOSURE_CREF running )
        {
            const Value& msg = s.pop();
            
            if (msg.isstr())
            {
                const auto& str = msg.tostr();
                
#if HAVE_HASH_MUTATION
                // I'm a little more willing to accept mutating arrays (vs upvals) just
                // because I don't know why.  Because arraylib is currently a library, and libraries
                // are even more tentative and easier to replace than the core language, does not
                // imply new syntax, and can always be retained as a deprecated library alongside
                // mutation free alternatives or something.
                if (str == "set")
                {
                    const auto& sethash = NEW_BANGFUN(SetBangHash)(myself_.lock());
                    s.push( STATIC_CAST_TO_BANGFUN(sethash) );
                }
#endif 
                else
                {
                    const Value& v = hash_[ str ];
                    s.push( v );
                }
            }
        }
    }; // end, BangHash class

    void SetBangHash::apply( Stack& s ) // , CLOSURE_CREF running )
    {
        const Value& msg = s.pop();
            
        if (msg.isstr())
        {
            const auto& str = msg.tostr();
            const Value& v = s.pop();
            hash_->set( str, v );
        }
    }
    

//     void stackToArray( Stack& s, const RunContext& rc )
//     {
//         const auto& toArrayFun = NEW_BANGFUN(FunctionStackToArray)( s );
//         toArrayFun->appendStack(s);
//         s.push( STATIC_CAST_TO_BANGFUN(toArrayFun) );
//     }

    void hashNew( Stack& s, const RunContext& rc )
    {
        const auto& hash = NEW_BANGFUN(BangHash)();
        hash->myself_ = hash;
        s.push( STATIC_CAST_TO_BANGFUN(hash) );
    }
    

    void banghash_lookup( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Bang::Value& v = s.pop();
        if (!v.isstr())
            throw std::runtime_error("Array library . operator expects string");
        const auto& str = v.tostr();
        
        const Bang::tfn_primitive p =
            (  // str == "from-stack" ? &stackToArray
               str == "new"        ? &hashNew // test
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("Hash library does not implement: " + str);
    }
    
} // end namespace Math


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &banghash_lookup );
}

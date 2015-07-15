#include "hashlib.h"

#include <string>
#include <iterator>
#include <math.h>
#include <stdlib.h>

#define HAVE_HASH_MUTATION 1

namespace Hashlib
{
    using namespace Bang;

//     unsigned jenkins_one_at_a_time_hash(const char *key, size_t len)
//     {
//         unsigned hash, i;
//         for(hash = i = 0; i < len; ++i)
//         {
//             hash += key[i];
//             hash += (hash << 10);
//             hash ^= (hash >> 6);
//         }
//         hash += (hash << 3);
//         hash ^= (hash >> 11);
//         hash += (hash << 15);
//         return hash;
//     }

//     unsigned jenkins_one_at_a_time_hash(const std::string& s )
//     {
//         return jenkins_one_at_a_time_hash( &s.front(), s.length() );
//     }
    
//     unsigned int luaS_hash (const char *str, size_t l ) { // , unsigned int seed) {
//         unsigned int h = 0xdeadbeef ^ ((unsigned int)(l));
//         size_t l1;
//         size_t step = (l >> 5) + 1;
//         for (l1 = l; l1 >= step; l1 -= step)
//             h = h ^ ((h<<5) + (h>>2) + ((unsigned char)(str[l1 - 1])));
//         return h;
//     }

#if 0    
    ::std::size_t StringHashFunction::operator ()(const Bang::bangstring& str) const
    {
        return str.gethash();
//             const int len = str.length();
//             return jenkins_one_at_a_time_hash( &str.front(), len );
//            return luaS_hash( &str.front(), len );
    }
#endif 


//     class SetBangHash : public Function
//     {
//         gcptr<BangHash> hash_;
//     public:
//         SetBangHash( const gcptr<BangHash>& hash )
//         : hash_( hash )
//         {}
        
//         virtual void apply( Stack& s ); // , CLOSURE_CREF running )
//     }; // end, SetBangHash


    void BangHash::set( Stack& s ) // , CLOSURE_CREF running )
    {
        const Value& vkey = s.pop();
            
        if (vkey.isstr())
        {
            const auto& key = vkey.tostr();
            const Value& v = s.pop();
            this->set( key, v );
        }
    }
    
    
    DLLEXPORT void BangHash::set( const bangstring& key, const Value& v )
    {
        auto loc = hash_.find( key );
        if (loc == hash_.end())
            hash_.insert( std::pair<bangstring,Value>( key, v ) );
        else
            loc->second = v;
    }


    template <class T>
    class BangMemFun : public Bang::Function
    {
        gcptr<T> cppfun_;
        void (T::* memfun_)(Bang::Stack&);
        
    public:
        BangMemFun( T* bangfun, void (T::* memfun)(Bang::Stack&) )
        : cppfun_( bangfun ),
          memfun_( memfun )
        {
        }
        void apply( Bang::Stack& s ) // , CLOSURE_CREF rc )
        {
            ((*cppfun_).*memfun_)( s );
        }
    };

    typedef BangMemFun<BangHash> BangHashMemFun;

    void BangHash::has( Stack& s ) // , CLOSURE_CREF running )
    {
        const Value& key = s.pop();
            
        if (key.isstr())
        {
            const auto& keystr = key.tostr(); // .gethash();
            auto loc = hash_.find(keystr);
            s.push( loc != hash_.end() );
        }
        else
            s.push( false );
            
    }

    void BangHash::keys( Stack& s ) // , CLOSURE_CREF running )
    {
        for ( auto hashel = hash_.begin(); hashel != hash_.end(); ++hashel ) // int i = 0; i < hash_.size(); ++i )
        {
            s.push( hashel->first ); // need to fix hashlib to store actual strings
        }
    }

    DLLEXPORT void BangHash::apply( Stack& s ) // , CLOSURE_CREF running )
    {
        const Value& msg = s.pop();
            
        if (msg.isstr())
        {
            const auto& key = msg.tostr();
                
            if (key == "has")
            {
                const auto& has = NEW_BANGFUN(BangHashMemFun, this, &BangHash::has);
                s.push( STATIC_CAST_TO_BANGFUN(has) );
            }
            else if (key == "keys")
            {
                const auto& sethash = NEW_BANGFUN(BangMemFun<BangHash>, this, &BangHash::keys);
                s.push( STATIC_CAST_TO_BANGFUN(sethash) );
            }
            else
#if HAVE_HASH_MUTATION
            // I'm a little more willing to accept mutating arrays/hashes (vs upvals/program variable bindings) just
            // because I don't know why.  Because arraylib is currently a library, and libraries
            // are even more tentative and easier to replace than the core language, does not
            // imply new syntax, and can always be retained as a deprecated library alongside
            // mutation free alternatives or something.
            // i don't like not having a distinction between hash keys and function names,
            // that's awkward for sure.  Maybe the operator id's would help or something,
            // and we could use #set, #has, etc.
            // i also don't like gcnewing all these Function objects that are generally
            // going to be immediately applied anyway.
            if (key == "set")
            {
                const auto& sethash = NEW_BANGFUN(BangMemFun<BangHash>, this, &BangHash::set);
                s.push( STATIC_CAST_TO_BANGFUN(sethash) );
            }
#endif 
            {
                // auto hash = key.gethash();
                auto loc = hash_.find( key );
                if (loc != hash_.end())
                    s.push( loc->second );
            }
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
        auto hash = NEW_BANGFUN(BangHash);
#if LCFG_GCPTR_STD        
        hash->myself_ = hash;
#endif 
        s.push( STATIC_CAST_TO_BANGFUN(hash) );
    }
    
    void hashOfString( Stack& s, const RunContext& rc )
    {
        const Value& msg = s.pop();
            
        if (msg.isstr())
        {
            const auto& key = msg.tostr();
            s.push( (double)key.gethash() ); // ( StringHashFunction()( key ) ) );
        }
    }
    
    void banghash_lookup( Bang::Stack& s, const Bang::RunContext& ctx )
    {
        const Bang::Value& v = s.pop();
        if (!v.isstr())
            throw std::runtime_error("Array library . operator expects string");
        const auto& str = v.tostr();
        
        const Bang::tfn_primitive p =
            (  str == "of-string" ? &hashOfString
            :  str == "new"      ? &hashNew // test
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("Hash library does not implement: " + std::string(str));
    }
    
} // end namespace Hashlib


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &Hashlib::banghash_lookup );
}

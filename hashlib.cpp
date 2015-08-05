#include "hashlib.h"

#include <string>
#include <iterator>
#include <math.h>
#include <stdlib.h>

#define HAVE_HASH_MUTATION 1

namespace Hashlib
{
    using namespace Bang;

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
        auto loc = this->find( key );
        if (loc == hash_.end())
#if LCFG_HASHLIB_SIMPLEVEC
            hash_.push_back( kvp_t( key, v ) );
#else
            hash_.insert( kvp_t( key, v ) );
#endif 
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

#if LCFG_HASHLIB_SIMPLEVEC
    std::vector< BangHash::kvp_t >::iterator BangHash::find( const bangstring& key )
    {
        for (auto it = hash_.begin(); it != hash_.end(); ++it)
        {
            if (it->first == key)
                return it;
        }
        return hash_.end();
    }
#endif 

    void BangHash::has( Stack& s ) // , CLOSURE_CREF running )
    {
        const Value& key = s.pop();
            
        if (key.isstr())
        {
            const auto& keystr = key.tostr(); // .gethash();
            auto loc = this->find( keystr );
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


    void BangHash::indexOperator( const Bang::Value& msg, Stack& stack, const RunContext& )
    {
        if (msg.isstr())
        {
            const auto& key = msg.tostr();
                
            {
                // auto hash = key.gethash();
                auto loc = this->find( key );
                if (loc != hash_.end())
                    stack.push( loc->second );
            }
        }
    }
    
    DLLEXPORT void BangHash::apply( Stack& s ) // , CLOSURE_CREF running )
    {
        const Bang::Value& msg = s.pop();
        Bang::RunContext* pctx = nullptr;
        this->indexOperator( msg, s, *pctx );
    }

    void BangHash::customOperator( const Value& v, const bangstring& theOperator, Stack& s)
    {
        const static Bang::bangstring opHas("/has");
        const static Bang::bangstring opKeys("/keys");
        const static Bang::bangstring opSet("/set");

        BangHash& self = reinterpret_cast<BangHash&>(*v.tofun());

        if (theOperator == opSet)
        {
            self.set(s);
        }
        else if (theOperator == opHas)
        {
            self.has(s);
        }
        else if (theOperator == opKeys)
        {
            self.keys(s);
        }
        else if (theOperator[0] == '>' && theOperator[1] == '>')
        {
            bangstring key( &theOperator.front(), theOperator.size()-2 );
            const Value& v = s.pop();
            self.set( key, v );
        }
    }

    struct HashOps : public Bang::Operators
    {
        HashOps()
        {
            customOperator = &BangHash::customOperator;
        }
    } gHashOperators;
    
    DLLEXPORT BangHash::BangHash()
    {
        operators = &gHashOperators;
    }

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

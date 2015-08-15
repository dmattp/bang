
#include "bang.h"
//#include <map>
#define LCFG_HASHLIB_SIMPLEVEC 0

#if LCFG_HASHLIB_SIMPLEVEC
# include <vector>
#else
# include <unordered_map>
#endif 


namespace Hashlib
{
    struct StringHashFunction {
        ::std::size_t operator ()(const Bang::bangstring& str) const
        {
            return str.gethash();
        }
    };

    class HashOps;
    
    class BangHash : public Bang::Function
    {
        friend class HashOps;

        typedef std::pair<Bang::bangstring, Bang::Value> kvp_t;
#if LCFG_HASHLIB_SIMPLEVEC
        std::vector< kvp_t > hash_;
        std::vector< kvp_t >::iterator find( const Bang::bangstring& key );
#else
        std::unordered_map< Bang::bangstring, Bang::Value, StringHashFunction> hash_;
        std::unordered_map< Bang::bangstring, Bang::Value, StringHashFunction>::iterator find( const Bang::bangstring& key ) { return hash_.find(key); }
#endif 

        //std::map< Bang::bangstring, Bang::Value> hash_;
        void has( Bang::Stack& s );
        void set( Bang::Stack& s );
        void keys( Bang::Stack& s );
        virtual void customOperator( const Bang::bangstring& theOperator, Bang::Stack& s);
        virtual void indexOperator( const Bang::Value& theIndex, Bang::Stack&, const Bang::RunContext& );
        
    public:
        DLLEXPORT BangHash();
        DLLEXPORT void set( const Bang::bangstring& key, const Bang::Value& v );
        DLLEXPORT virtual void apply( Bang::Stack& s ); // , CLOSURE_CREF running )
    };
}

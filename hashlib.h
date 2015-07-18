
#include <vector>
#include "bang.h"
//#include <map>
#include <unordered_map>

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
        std::unordered_map< Bang::bangstring, Bang::Value, StringHashFunction> hash_;
        //std::map< Bang::bangstring, Bang::Value> hash_;
        void has( Bang::Stack& s );
        void set( Bang::Stack& s );
        void keys( Bang::Stack& s );
        static void customOperator( const Bang::Value& v, const Bang::bangstring& theOperator, Bang::Stack& s);
        
    public:
        DLLEXPORT BangHash();
        DLLEXPORT void set( const Bang::bangstring& key, const Bang::Value& v );
        DLLEXPORT virtual void apply( Bang::Stack& s ); // , CLOSURE_CREF running )
    };
}


#include "bang.h"

#include <string>
#include <stdlib.h>
#include <algorithm>

namespace String
{

    
    using namespace Bang;
    void checkstrtype( const Bang::Value& v )
    {
        if (!v.isstr())
            throw std::runtime_error("String lib incompatible type");
    }

    void len( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const auto& v = s.pop();
        checkstrtype(v);
        s.push( double(v.tostr().size()) );
    }

    void sub( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Value& sEnd = s.pop();
        if (!sEnd.isnum())
            throw std::runtime_error("String lib incompatible type");
        const Value& sBeg = s.pop();
        if (!sBeg.isnum())
            throw std::runtime_error("String lib incompatible type");
        const Value& vStr = s.pop();
        checkstrtype( vStr );
        s.push( std::string(vStr.tostr()).substr( sBeg.tonum(), sEnd.tonum()-sBeg.tonum()+1 ) );
    }

    void lt( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Value& sRt = s.pop();
        const Value& sLt = s.pop();
        checkstrtype( sLt );
        checkstrtype( sRt );
        s.push( std::string(sLt.tostr()) < std::string(sRt.tostr()) );
    }

    void eq( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Value& sRt = s.pop();
        const Value& sLt = s.pop();
        checkstrtype( sLt );
        checkstrtype( sRt );
        s.push( sLt.tostr() == sRt.tostr() );
    }

    void byte( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        checkstrtype(s.loc_top());
        const auto& str = s.pop().tostr();
        int ndx = s.pop().tonum();
        s.push( (double)str[ndx] );
        // s.push( sLt.tostr() < sRt.tostr() );
    }
    
    void to_bytes( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        checkstrtype(s.loc_top());
        const Value& v = s.pop();
        std::string str = v.tostr();
        std::for_each( str.begin(), str.end(),
            [&]( char c ) { s.push( double((unsigned char)c) ); } );
    }
    
    void from_bytes( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        std::string created;
        const int stacklen = s.size();
        if (stacklen > 0)
        {
            for (int i = stacklen - 1; i >= 0; --i )
                created.push_back( (char)s.nth(i).tonum() );
            for (int i = stacklen - 1; i >= 0; --i )
                s.pop_back();
        }
        s.push( created );
    }
    
    void lookup( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Bang::Value& v = s.pop();
        if (!v.isstr())
            throw std::runtime_error("String library '.' operator expects string");
        const auto& str = v.tostr();

        const Bang::tfn_primitive p =
            (  str == "len"    ? &len
            :  str == "sub"    ? &sub
            :  str == "lt"     ? &lt
            :  str == "eq"     ? &eq
            :  str == "byte"     ? &byte
            :  str == "to-bytes"   ? &to_bytes
            :  str == "from-bytes"   ? &from_bytes
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("String library does not implement" + std::string(str));
    }
    
} // end namespace Math


extern "C" 
#if _WINDOWS
__declspec(dllexport)
#endif
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &String::lookup );
}

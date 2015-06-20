
#include "bang.h"

#include <string>
#include <stdlib.h>
#include <algorithm>

namespace String
{

#if 0    
    /* str_format modified heavily from Lua */
    int str_format (Stack& S)
    {
        int top = lua_gettop(L);
        int arg = 1;
        size_t sfl;
        const char *strfrmt = luaL_checklstring(L, arg, &sfl);
        const char *strfrmt_end = strfrmt+sfl;
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        while (strfrmt < strfrmt_end) {
            if (*strfrmt != L_ESC)
                luaL_addchar(&b, *strfrmt++);
            else if (*++strfrmt == L_ESC)
                luaL_addchar(&b, *strfrmt++);  /* %% */
            else { /* format item */
                char form[MAX_FORMAT];  /* to store the format (`%...') */
                char *buff = luaL_prepbuffsize(&b, MAX_ITEM);  /* to put formatted item */
                int nb = 0;  /* number of bytes in added item */
                if (++arg > top)
                    luaL_argerror(L, arg, "no value");
                strfrmt = scanformat(L, strfrmt, form);
                switch (*strfrmt++) {
                    case 'c': {
                        nb = sprintf(buff, form, luaL_checkint(L, arg));
                        break;
                    }
                    case 'd': case 'i': {
                        lua_Number n = luaL_checknumber(L, arg);
                        LUA_INTFRM_T ni = (LUA_INTFRM_T)n;
                        lua_Number diff = n - (lua_Number)ni;
                        luaL_argcheck(L, -1 < diff && diff < 1, arg,
                            "not a number in proper range");
                        addlenmod(form, LUA_INTFRMLEN);
                        nb = sprintf(buff, form, ni);
                        break;
                    }
                    case 'o': case 'u': case 'x': case 'X': {
                        lua_Number n = luaL_checknumber(L, arg);
                        unsigned LUA_INTFRM_T ni = (unsigned LUA_INTFRM_T)n;
                        lua_Number diff = n - (lua_Number)ni;
                        luaL_argcheck(L, -1 < diff && diff < 1, arg,
                            "not a non-negative number in proper range");
                        addlenmod(form, LUA_INTFRMLEN);
                        nb = sprintf(buff, form, ni);
                        break;
                    }
                    case 'e': case 'E': case 'f':
#if defined(LUA_USE_AFORMAT)
                    case 'a': case 'A':
#endif
                    case 'g': case 'G': {
                        addlenmod(form, LUA_FLTFRMLEN);
                        nb = sprintf(buff, form, (LUA_FLTFRM_T)luaL_checknumber(L, arg));
                        break;
                    }
                    case 'q': {
                        addquoted(L, &b, arg);
                        break;
                    }
                    case 's': {
                        size_t l;
                        const char *s = luaL_tolstring(L, arg, &l);
                        if (!strchr(form, '.') && l >= 100) {
                            /* no precision and string is too long to be formatted;
                               keep original string */
                            luaL_addvalue(&b);
                            break;
                        }
                        else {
                            nb = sprintf(buff, form, s);
                            lua_pop(L, 1);  /* remove result from 'luaL_tolstring' */
                            break;
                        }
                    }
                    default: {  /* also treat cases `pnLlh' */
                        return luaL_error(L, "invalid option " LUA_QL("%%%c") " to "
                            LUA_QL("format"), *(strfrmt - 1));
                    }
                }
                luaL_addsize(&b, nb);
            }
        }
        luaL_pushresult(&b);
        return 1;
    }
#endif 

    
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
        s.push( vStr.tostr().substr( sBeg.tonum(), sEnd.tonum()-sBeg.tonum()+1 ) );
    }

    void lt( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Value& sRt = s.pop();
        const Value& sLt = s.pop();
        checkstrtype( sLt );
        checkstrtype( sRt );
        s.push( sLt.tostr() < sRt.tostr() );
    }
    
    void to_bytes( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        checkstrtype(s.loc_top());
        const Value& v = s.pop();
        const auto& str = v.tostr();
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
            :  str == "to-bytes"   ? &to_bytes
            :  str == "from-bytes"   ? &from_bytes
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("String library does not implement" + str);
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

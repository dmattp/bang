
#include "bang.h"

#include <string>
#include <stdexcept>
#include <stdlib.h>
#include <algorithm>

#define LCFG_FULL_SUPPORT 0

#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"
#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)
/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary.
*/
#if !defined(LUA_MAXCAPTURES)
#define LUA_MAXCAPTURES		32
#endif

#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif

#define luaL_error( l, ... ) \
    (std::cerr << "ERROR\n" << std::endl), 0

typedef unsigned char uchar;

// throw std::runtime_error("stringlib X01 error")

namespace String
{
    using namespace Bang;

/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
  size_t upto = 0;
  do {
    if (strpbrk(p + upto, SPECIALS))
      return 0;  /* pattern has a special character */
    upto += strlen(p + upto) + 1;  /* may have more after \0 */
  } while (upto <= l);
  return 1;  /* no special chars found */
}

static const char *lmemfind (const char *s1, size_t l1,
                               const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative `l1' */
  else {
    const char *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1-l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct `l1' and `s1' to try again */
        l1 -= init-s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}

typedef struct MatchState {
  int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  const char *p_end;  /* end ('\0') of pattern */
  Bang::Stack* pStack;
    //lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;

/* recursive function */
static const char *match (MatchState *ms, const char *s, const char *p);



static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    return luaL_error(ms->L, "invalid capture index %%%d", l + 1);
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return luaL_error(ms->L, "invalid pattern capture");
}


static const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (p == ms->p_end)
        luaL_error(ms->L, "malformed pattern (ends with " LUA_QL("%%") ")");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a `]' */
        if (p == ms->p_end)
          luaL_error(ms->L, "malformed pattern (missing " LUA_QL("]") ")");
        if (*(p++) == L_ESC && p < ms->p_end)
          p++;  /* skip escapes (e.g. `%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'g' : res = isgraph(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;  /* deprecated option */
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}


static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
  if (s >= ms->src_end)
    return 0;
  else {
    int c = uchar(*s);
    switch (*p) {
      case '.': return 1;  /* matches any char */
      case L_ESC: return match_class(c, uchar(*(p+1)));
      case '[': return matchbracketclass(c, p, ep-1);
      default:  return (uchar(*p) == c);
    }
  }
}


static const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (p >= ms->p_end - 1)
    luaL_error(ms->L, "malformed pattern "
                      "(missing arguments to " LUA_QL("%%b") ")");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while (singlematch(ms, s + i, p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (singlematch(ms, s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}

static const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) luaL_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}

static const char *end_capture (MatchState *ms, const char *s,
                                  const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}

    
static const char *match (MatchState *ms, const char *s, const char *p) {

#if LCFG_FULL_SUPPORT    
  if (ms->matchdepth-- == 0)
    luaL_error(ms->L, "pattern too complex");
#endif 
  
  init: /* using goto's to optimize tail recursion */
  if (p != ms->p_end) {  /* end of pattern? */
    switch (*p) {
      case '(': {  /* start capture */
        if (*(p + 1) == ')')  /* position capture? */
          s = start_capture(ms, s, p + 2, CAP_POSITION);
        else
          s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
        break;
      }
      case ')': {  /* end capture */
        s = end_capture(ms, s, p + 1);
        break;
      }
      case '$': {
        if ((p + 1) != ms->p_end)  /* is the `$' the last char in pattern? */
          goto dflt;  /* no; go to default */
        s = (s == ms->src_end) ? s : NULL;  /* check end of string */
        break;
      }
      case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
        switch (*(p + 1)) {
          case 'b': {  /* balanced string? */
            s = matchbalance(ms, s, p + 2);
            if (s != NULL) {
              p += 4; goto init;  /* return match(ms, s, p + 4); */
            }  /* else fail (s == NULL) */
            break;
          }
          case 'f': {  /* frontier? */
            const char *ep; char previous;
            p += 2;
            if (*p != '[')
              luaL_error(ms->L, "missing " LUA_QL("[") " after "
                                 LUA_QL("%%f") " in pattern");
            ep = classend(ms, p);  /* points to what is next */
            previous = (s == ms->src_init) ? '\0' : *(s - 1);
            if (!matchbracketclass(uchar(previous), p, ep - 1) &&
               matchbracketclass(uchar(*s), p, ep - 1)) {
              p = ep; goto init;  /* return match(ms, s, ep); */
            }
            s = NULL;  /* match failed */
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, uchar(*(p + 1)));
            if (s != NULL) {
              p += 2; goto init;  /* return match(ms, s, p + 2) */
            }
            break;
          }
          default: goto dflt;
        }
        break;
      }
      default: dflt: {  /* pattern class plus optional suffix */
        const char *ep = classend(ms, p);  /* points to optional suffix */
        /* does not match at least once? */
        if (!singlematch(ms, s, p, ep)) {
          if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
            p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
          }
          else  /* '+' or no suffix */
            s = NULL;  /* fail */
        }
        else {  /* matched once */
          switch (*ep) {  /* handle optional suffix */
            case '?': {  /* optional */
              const char *res;
              if ((res = match(ms, s + 1, ep + 1)) != NULL)
                s = res;
              else {
                p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
              }
              break;
            }
            case '+':  /* 1 or more repetitions */
              s++;  /* 1 match already done */
              /* go through */
            case '*':  /* 0 or more repetitions */
              s = max_expand(ms, s, p, ep);
              break;
            case '-':  /* 0 or more repetitions (minimum) */
              s = min_expand(ms, s, p, ep);
              break;
            default:  /* no suffix */
              s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
          }
        }
        break;
      }
    }
  }
  ms->matchdepth++;
  return s;
}

static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
    {
        ms->pStack->push_bs( Bang::bangstring( s, e - s ) );
//      lua_pushlstring(ms->L, s, e - s);  /* add whole match */
    }
    else
        luaL_error(ms->L, "invalid capture index");
  }
  else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED)
        luaL_error(ms->L, "unfinished capture");
    if (l == CAP_POSITION)
        ms->pStack->push( double(ms->capture[i].init - ms->src_init + 1) );
    else
        ms->pStack->push_bs( Bang::bangstring( ms->capture[i].init, l) );
  }
}
    

static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
#if LCFG_FULL_SUPPORT  
  luaL_checkstack(ms->L, nlevels, "too many captures");
#endif 
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}
    
    void str_find_aux( Bang::Stack& stack, int find )
    {
        const auto v_s = stack.pop(); // string  / haystack
        const auto v_p = stack.pop(); // pattern / needle
        const bool nospecials_arg = false;
        
        const char *s = v_s.tostr().c_str(); // luaL_checklstring(L, 1, &ls);
        const char *p = v_p.tostr().c_str(); // luaL_checklstring(L, 2, &lp);
        size_t ls = v_s.tostr().length();
        size_t lp = v_p.tostr().length();

        
        size_t init = 1; // posrelat(luaL_optinteger(L, 3, 1), ls);
        if (init < 1) init = 1;
        else if (init > ls + 1) {  /* start after string's end? */
            // lua_pushnil(L);  /* cannot find anything */
            stack.push(false);
            return; //  1;
        }

//        std::cerr << "s=" << s << " p=" << p <<std::endl;
        
        /* explicit request or no special characters? */
        if (find && (nospecials_arg || nospecials(p, lp))) {
            /* do a plain search */
            const char *s2 = lmemfind(s + init - 1, ls - init + 1, p, lp);
            if (s2) {
                stack.push(double( s2 - s + 1 ));
                stack.push(double( s2 - s + lp ));
                stack.push( true );
                return;
            }
        }
        else {
            MatchState ms;
            const char *s1 = s + init - 1;
            int anchor = (*p == '^');
            if (anchor) {
                p++; lp--;  /* skip anchor character */
            }
            ms.pStack = &stack;
            ms.matchdepth = MAXCCALLS;
            ms.src_init = s;
            ms.src_end = s + ls;
            ms.p_end = p + lp;
            do {
                const char *res;
                ms.level = 0;
                // lua_assert(ms.matchdepth == MAXCCALLS);
                if ((res=match(&ms, s1, p)) != NULL) {
                    if (find) {
                        push_captures(&ms, NULL, 0) + 2;
                        stack.push(double( s1 - s + 1 ));  /* start */
                        stack.push(double(res - s));   /* end */
                        stack.push( true );
                        return;
                    }
                    else
                    {
                        push_captures(&ms, s1, res);
                        stack.push( true );
                        return;
                    }
                }
            } while (s1++ < ms.src_end && !anchor);
        }
        stack.push(false);
    }

    void str_find( Bang::Stack& stack, const Bang::RunContext& ctx)
    {
        str_find_aux( stack, 1 );
    }

    void str_match( Bang::Stack& stack, const Bang::RunContext& ctx)
    {
        str_find_aux( stack, 0 );
    }

    
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
//        std::cerr << "to-bytes begin=" << str.begin() << " end=" <<str.end() << std::endl;
        std::for_each( str.begin(), str.end(),
            [&]( char c ) {
//                std::cerr << "to-bytes c=" << int(c) << std::endl;
                 s.push( double((unsigned char)c) );
            } );
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
            (  str == "len"        ? &len
            :  str == "sub"        ? &sub
            :  str == "byte"       ? &byte
            :  str == "to-bytes"   ? &to_bytes
            :  str == "find"       ? &str_find
            :  str == "match"      ? &str_match
            :  str == "from-bytes" ? &from_bytes
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

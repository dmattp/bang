#include <iostream>
#include <numeric> // for std::accumulate
#include <stdio.h>
#include <windows.h>

#include "bang.h"

 #define kDefaultScript "c:/m/n2proj/bang/test/opt-01-multiops-upval.bang";


using namespace Bang;

namespace {
    bool gDumpMode = false;
}


void repl_prompt()
{
    std::cout << "Bang! " << std::flush;
}

class RegurgeStdinRepl : public RegurgeIo
{
    bool atEof_;
public:
    RegurgeStdinRepl()
    : atEof_(false)
    {}

    char getc()
    {
        int icud = RegurgeIo::getcud();

        if (icud != EOF)
            return icud;

        if (atEof_)
            throw ErrorEof();
        
        int istream = fgetc(stdin);
        if (istream == EOF || istream == 0x0d || istream == 0x0a) // CR
        {
            atEof_ = true;
            return 0x0a; // LF is close enough
        }
        else
        {
            return istream;
        }
    }
};


class BangmainParsingContext : public ParsingContext
{
    bool bEof_;
public:
    BangmainParsingContext( InteractiveEnvironment& e, bool isLive )
    : ParsingContext(e),
      bEof_( isLive )
    {}
    
    Ast::Base* hitEof( const Ast::CloseValue* uvchain );
};

Ast::Program* parseStdinToProgram( ParsingContext& ctx, const Ast::CloseValue* closeValueChain )
{
    RegurgeStdinRepl strmStdin;
    return
    ParseToProgram
    (   ctx,
        strmStdin,
        gDumpMode,
        closeValueChain
    );
}
    

class BangmainEofMarker : public Ast::EofMarker
{
public:
    BangmainEofMarker( BangmainParsingContext& bmpc )
    : Ast::EofMarker( bmpc )
    {}


    virtual void report_error( const std::exception& e ) const
    {
        std::cerr << "REPL Error: " << e.what() << std::endl;
    }
    
    DLLEXPORT Ast::Program* getNextProgram( SHAREDUPVALUE uv ) const
    {
        const Ast::CloseValue* closeValueChain = uv ? uv->upvalParseChain() : static_cast<const Ast::CloseValue*>(nullptr);
        // std::cerr << "EofMarker::getNextProgram\n";
        while (true)
        {
            try {
                auto rc = parseStdinToProgram( parsectx_, closeValueChain );
                return rc;
            } catch (std::exception&e) {
                std::cout << "a08 Error: " << e.what() << std::endl;
                parsectx_.interact.repl_prompt();
            }
        }
    }
};

Ast::Base* BangmainParsingContext::hitEof( const Ast::CloseValue* uvchain )
{
    if (bEof_)
        return ( new BangmainEofMarker(*this) );
    else
        return ( new Ast::BreakProg() );
}

    class RegurgeString  : public Bang::RegurgeIo
    {
        std::string str_;
        bool atEof_;
    public:
        RegurgeString( const std::string& str )
        : str_( str ),
          atEof_( str.size() < 1 )
        {
        }
        char getc()
        {
            int icud = RegurgeIo::getcud();

            if (icud != EOF)
                return icud;

            if (atEof_)
                throw Bang::ErrorEof();
        
            if (str_.size() < 1)
            {
                atEof_ = true;
                return 0x0a;
            }
            else
            {
                char rc = str_.front();
                str_.erase( str_.begin() );
                return rc;
            }
        }
    };

    class ShParsingContext : public Bang::ParsingContext
    {
    public:
        ShParsingContext( Bang::InteractiveEnvironment& i )
        : Bang::ParsingContext(i)
        {
        }
        Bang::Ast::Base* hitEof( const Bang::Ast::CloseValue* uvchain )
        {
//            std::cerr << "ShParsingContext::hitEof\n";
            return new Ast::BreakProg(); // ShEofMarker( *this, uvchain );
        }
    };

    void eval_more_bang_code( const std::string& vstr, Bang::Thread& thread ) // , const Bang::RunContext& ctx )
    {
        Bang::InteractiveEnvironment interact;
        ShParsingContext parsectx( interact );

        try
        {
            RegurgeString strmFile( vstr );
            auto prog = Bang::ParseToProgram( parsectx, strmFile, false, nullptr );
            RunProgram( &thread, prog, SHAREDUPVALUE() );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Error parsing command line: " << e.what() << std::endl;
            exit(-1);
        }
    }




/*
Generate test output:

  dir test\*.bang | %{ .\bang $_ | out-file -encoding ascii .\test\$("out." + $_.name + ".out") }

Test against reference output:

  dir test\*.bang | %{ $t=.\bang $_;  $ref = cat .\test\$("out." + $_.name + ".out"); if (compare-object $t $ref) { throw "FAILED $($_.name)!" } }  
 */
DLLEXPORT int bangmain( int argc, char* argv[] )
{

    bool bDump = false;
    bool bInteractive = false;
    bool bCmdLineAfterProgram = false;

    Bang::InteractiveEnvironment interact;

    const char* fname = nullptr;
    for (int n = 1; n < argc; ++n)
    {
        std::string arg = argv[n];
        if (arg == "-dump")
        {
            bDump = true;
            argv[n] = nullptr;
        }
        else if (arg == "-v")
        {
            std::cerr << "Bang! v" << BANG_VERSION << " - Welcome!" << std::endl;
            exit(0);
        }
        else if (arg == "-i")
        {
            interact.repl_prompt = &repl_prompt;
            bInteractive = true;
            argv[n] = nullptr;
        }
        else if (arg.size() > 5 && arg.substr(arg.size()-5,5) == ".bang")
        {
            fname = argv[n];
            argv[n] = nullptr;
        }
        else if (arg == "-e") // everything after -e is a program
        {
            argv[n] = nullptr;
            break;
        }
        else if (arg == "-em") // everything after -e is a program
        {
            bCmdLineAfterProgram = true;
            argv[n] = nullptr;
            break;
        }
    }
    
    if (!fname)
    {
#ifdef kDefaultScript
        bDump = true;
        fname = kDefaultScript;
#else
        interact.repl_prompt = &repl_prompt;
        bInteractive = true;
#endif 
    }

    std::string cmdlineprog =
        std::accumulate( argv+1, argv + argc, std::string(""),
            []( std::string acc, const char* arg ) {
                return acc + (arg ? (std::string(" ") + arg) : "");
            });

    gDumpMode = bDump;

    Bang::Thread thread;

    if (cmdlineprog.size() > 0)
    {
//        std::cout << "CLPROG=" << cmdlineprog << '\n';
        eval_more_bang_code( cmdlineprog, thread );
        if (!fname) // if no file to parse, dump the stack and exit
        {
            thread.stack.dump( std::cout );
            if (!bCmdLineAfterProgram)
            {
                return 0;
            }
        }
    }

    interact.repl_prompt();
    
    BangmainParsingContext parsectx( interact, bInteractive );
    do
    {
        try
        {
            Ast::Program* prog;
            if (fname)
            {
                if (cmdlineprog.size() > 0 && bCmdLineAfterProgram)
                {
/*
  PARSE(upvalDESCchain-1,program-2 text)  => program-2, upvalDESCchain-2
    EXECUTE (program-2, bound-upvals-1 ) => (I/O), bound-upvals-2
    PARSE(upvalDESCchain-2,program-3 text)  => program-3, upvalDESCchain-3
    EXECUTE (program-3, bound-upvals-2 ) => (I/O), bound-upvals-3
    ...
    ...
    is that right?
*/

                    
#if 0 // need ImportParsingContext class
                    ImportParsingContext importContext
                    (   parsecontext, stream,
                        nullptr /*parent*/,
                        nullptr /*upvalueChain*/,
                        pRecParsing
                    );
                    RequireKeyword requireImport( fname );
                    auto prog = requireImport.parseToProgramNoUpvals( importContext, gDumpMode ); // DUMP
#endif 
                }
                else
                {
                    Bang::RequireKeyword requireMain( fname );
//                    auto before = GetTickCount();
                    prog = requireMain.parseToProgramNoUpvals( parsectx, bDump );
//                    auto after = GetTickCount();
                    RunProgram( &thread, prog, SHAREDUPVALUE() );
//                    std::cout << "elapsed parse time=" << (after-before) << "\n";
                }
            }
            else
            {
                prog = parseStdinToProgram( parsectx, nullptr );
                RunProgram( &thread, prog, SHAREDUPVALUE() );
            }
//             Bang::RequireKeyword requireMain( fname );
//             requireMain.parseAndRun( parsectx, thread, bDump );
            thread.stack.dump( std::cout );
        }
#if 0        
        catch( const AstExecFail& ast )
        {
            gFailedAst = ast.pStep;
//            closure->dump( std::cerr );
            std::cerr << "Runtime AST exec Error" << gFailedAst << ": " << ast.e.what() << std::endl;
        }
#endif 
        catch( const std::exception& e )
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    while (bInteractive);

    Bang::dumpProfilingStats();
//    std::cerr << "toodaloo!" << std::endl;

    return 0;
}

int main( int argc, char* argv[] )
{
    bangmain( argc, argv );
}

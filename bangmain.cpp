#include <iostream>
#include <stdio.h>

#include "bang.h"

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



/*
Generate test output:

  dir test\*.bang | %{ .\bang $_ | out-file -encoding ascii .\test\$("out." + $_.name + ".out") }

Test against reference output:

  dir test\*.bang | %{ $t=.\bang $_;  $ref = cat .\test\$("out." + $_.name + ".out"); if (compare-object $t $ref) { throw "FAILED $($_.name)!" } }  
 */
DLLEXPORT int bangmain( int argc, char* argv[] )
{
    std::cerr << "Bang! v" << BANG_VERSION << " - Welcome!" << std::endl;

    bool bDump = false;
    bool bInteractive = false;

    Bang::InteractiveEnvironment interact;

    if (argc > 1)
    {
        if (std::string("-dump") == argv[1])
        {
            bDump = true;
            argv[1] = argv[2];
            --argc;
        }

        if (std::string("-i") == argv[1])
        {
            interact.repl_prompt = &repl_prompt;
            bInteractive = true;
            argv[1] = argv[2];
            --argc;
        }
    }
    const char* fname = argv[1];
    if (argc < 2)
    {
#ifdef kDefaultScript
        bDump = true;
        fname = kDefaultScript;
#else
        fname = nullptr; // kDefaultScript;
        interact.repl_prompt = &repl_prompt;
        bInteractive = true;
#endif 
    }

    gDumpMode = bDump;

    Bang::Thread thread;

    interact.repl_prompt();
    
    BangmainParsingContext parsectx( interact, bInteractive );
    do
    {
        try
        {
            Ast::Program* prog;
            if (fname)
            {
                Bang::RequireKeyword requireMain( fname );
                prog = requireMain.parseToProgramNoUpvals( parsectx, bDump );
            }
            else
            {
                prog = parseStdinToProgram( parsectx, nullptr );
            }
            SHAREDUPVALUE noUpvals;
            RunProgram( &thread, prog, noUpvals );
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
    
    std::cerr << "toodaloo!" << std::endl;

    return 0;
}

int main( int argc, char* argv[] )
{
    bangmain( argc, argv );
}

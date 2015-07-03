#include <iostream>

#include "bang.h"

void repl_prompt()
{
    std::cout << "Bang! " << std::flush;
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
            interact.bEof = true;
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
        interact.bEof = true;
#endif 
    }

//    gDumpMode = bDump;

    Bang::Thread thread;

    interact.repl_prompt();
    
    Bang::ParsingContext parsectx( interact );
    do
    {
        try
        {
            Bang::RequireKeyword requireMain( fname );
            requireMain.parseAndRun( parsectx, thread, bDump );
            thread.stack.dump( std::cout );
        }
        catch( const std::exception& e )
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    while (interact.bEof);
    
    std::cerr << "toodaloo!" << std::endl;

    return 0;
}

int main( int argc, char* argv[] )
{
    bangmain( argc, argv );
}

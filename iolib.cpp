#include "bang.h"

#include <string>
#include <vector>
#include <iterator>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

namespace IO
{
    using namespace Bang;
    
    class CstdFile : public Function
    {
        FILE* file_;
        std::string filename_;
        
    public:
        CstdFile( const std::string& fname )
        : filename_( fname )
        {
//            filename_ = fname.tostr();
            file_ = fopen( filename_.c_str(), "r" );
        }

        ~CstdFile()
        {
//            std::cerr << "~CstdFile" << std::endl;
            if (file_)
                fclose( file_ );
        }

        virtual void apply( Stack& s ) // , CLOSURE_CREF running )
        {
        }

        virtual void readAll( Stack& s ) // , CLOSURE_CREF running )
        {
            std::string fileContent;
            while (true)
            {
                char buffer[4096];
                size_t nread = fread( buffer, 1, sizeof(buffer), file_ );
                if (nread > 0)
                    fileContent.append( buffer, nread );

                if (nread != sizeof(buffer))
                    break;
            }
            s.push_bs( fileContent );
        }
        
        void customOperator( const bangstring& str, Stack& s)
        {
            const static Bang::bangstring opReadAll("/read-all");
            const static Bang::bangstring opClose("/close");
        
            if (str == opReadAll)
                readAll( s );
            else if (str == opClose)
            {
                fclose( file_ );
                file_ = 0;
            }
        }
    }; // end, FunctionStackToArray class

    void ioOpen( Stack& s, const RunContext& rc )
    {
        const Value &v = s.pop();
        const auto& theFile = NEW_BANGFUN( CstdFile, v.tostr() );
        s.push( STATIC_CAST_TO_BANGFUN(theFile) );
    }

    void lookup( Bang::Stack& s, const Bang::RunContext& ctx)
    {
        const Bang::Value& v = s.pop();
        if (!v.isstr())
            throw std::runtime_error("Array library . operator expects string");
        const auto& str = v.tostr();
        
        const Bang::tfn_primitive p =
            (  str == "open" ? &ioOpen
            :  nullptr
            );

        if (p)
            s.push( p );
        else
            throw std::runtime_error("IO library does not implement: " + std::string(str));
    }
    
} // end namespace IO


extern "C"
#if _WINDOWS
__declspec(dllexport)
#endif 
void bang_open( Bang::Stack* stack, const Bang::RunContext* )
{
    stack->push( &IO::lookup );
}

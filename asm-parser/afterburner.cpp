/*********************************************************************
 *
 * Copyright (C) 2006,  University of Karlsruhe
 *
 * File path:     asm-parser/afterburner.cpp
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/


#include <iostream>
#include <fstream>

#include "AsmLexer.hpp"
#include "AsmParser.hpp"
#include "AsmTreeParser.hpp"
#include <antlr/TokenBuffer.hpp>

int main( int argc, char *argv[] )
{
    ANTLR_USING_NAMESPACE(std);
    ANTLR_USING_NAMESPACE(antlr);
    ANTLR_USING_NAMESPACE(Asm);

    bool burn_gprof = false;
    bool burn_gcov = false;
    bool burn_sensitive = true;
    bool burn_32 = false;
    char *filename = NULL;

    for( int i = 1; i < argc; i++ ) {
	if( !strcmp(argv[i], "-p") )
	    burn_gprof = true;
	else if( !strcmp(argv[i], "-c") )
	    burn_gcov = true;
	else if( !strcmp(argv[i], "-s") )
	    burn_sensitive = false;
	else if( !strcmp(argv[i], "-m32") )
	    burn_32 = true;
	else if( !strcmp(argv[i], "-m64") )
	    burn_32 = false;
	else
	    filename = argv[i];
    }

    if( NULL == filename )
	return 1;

    try {
        ifstream input( filename );
	AsmLexer lexer(input);
	TokenBuffer buffer(lexer);
	AsmParser parser(buffer);

	ASTFactory ast_factory;
	parser.initializeASTFactory( ast_factory );
	parser.setASTFactory( &ast_factory );

	parser.asmFile();
	RefAST a = parser.getAST();

	AsmTreeParser tree_parser;
	tree_parser.init( burn_sensitive, burn_gprof, burn_gcov, burn_32 );
	tree_parser.initializeASTFactory( ast_factory );
	tree_parser.setASTFactory( &ast_factory );

	tree_parser.asmFile( a );

#if 0
	cout << "List:" << endl;
	cout << a->toStringList() << endl;
	cout << "Tree:" << endl;
	cout << a->toStringTree() << endl;
#endif
    }
    catch( ANTLRException& e )
    {
	cerr << "exception: " << e.getMessage() << endl;
	return 2;
    }
    catch( exception& e )
    {
	cerr << "exception: " << e.what() << endl;
	return 3;
    }
    return 0;
}


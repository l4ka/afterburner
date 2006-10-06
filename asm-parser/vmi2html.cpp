/*********************************************************************
 *
 * Copyright (C) 2006,  University of Karlsruhe
 *
 * File path:     asm-parser/vmi2html.cpp
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

#include "VmiCallsLexer.hpp"
#include "VmiCallsParser.hpp"
#include "VmiCallsHtmlEmitter.hpp"
#include <antlr/TokenBuffer.hpp>

int main( int argc, char *argv[] )
{
    ANTLR_USING_NAMESPACE(std);
    ANTLR_USING_NAMESPACE(antlr);
    ANTLR_USING_NAMESPACE(VmiCalls);

    if( argc < 2 )
	exit( 0 );
    try {
        ifstream input( argv[1] );
	VmiCallsLexer lexer(input);
	TokenBuffer buffer(lexer);
	VmiCallsParser parser(buffer);

	ASTFactory ast_factory;
	parser.initializeASTFactory( ast_factory );
	parser.setASTFactory( &ast_factory );

	parser.vmiCallsFile();
	RefAST a = parser.getAST();

	VmiCallsHtmlEmitter tree_walker;
	tree_walker.initializeASTFactory( ast_factory );
	tree_walker.setASTFactory( &ast_factory );

	tree_walker.vmiCalls( a );

	/*
	cout << "Tree:" << endl;
	cout << a->toStringTree() << endl;
	*/
    }
    catch( ANTLRException& e )
    {
	cerr << "exception: " << e.getMessage() << endl;
	return -1;
    }
    catch( exception& e )
    {
	cerr << "exception: " << e.what() << endl;
	return -1;
    }
    return 0;
}


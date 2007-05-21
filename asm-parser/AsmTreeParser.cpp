/* $ANTLR 2.7.6 (20060929): "Asm.g" -> "AsmTreeParser.cpp"$ */
#line 13 "Asm.g"

    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before the antlr generated includes in the cpp file

#line 10 "AsmTreeParser.cpp"
#include "AsmTreeParser.hpp"
#include <antlr/Token.hpp>
#include <antlr/AST.hpp>
#include <antlr/NoViableAltException.hpp>
#include <antlr/MismatchedTokenException.hpp>
#include <antlr/SemanticException.hpp>
#include <antlr/BitSet.hpp>
#line 19 "Asm.g"

    // Inserted after the antlr generated includes in the cpp file

#line 22 "AsmTreeParser.cpp"
ANTLR_BEGIN_NAMESPACE(Asm)
#line 422 "Asm.g"

// Global stuff in the cpp file.

#line 27 "AsmTreeParser.cpp"
AsmTreeParser::AsmTreeParser()
	: antlr::TreeParser() {
}

void AsmTreeParser::asmFile(antlr::RefAST _t) {
	antlr::RefAST asmFile_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		asmBlocks(_t);
		_t = _retTree;
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmBlocks(antlr::RefAST _t) {
	antlr::RefAST asmBlocks_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_t->getType() == ASTBasicBlock)) {
				asmBlock(_t);
				_t = _retTree;
			}
			else {
				goto _loop265;
			}
			
		}
		_loop265:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmBlock(antlr::RefAST _t) {
	antlr::RefAST asmBlock_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		antlr::RefAST __t267 = _t;
		antlr::RefAST tmp1_AST_in = _t;
		match(_t,ASTBasicBlock);
		_t = _t->getFirstChild();
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_t->getType() == Label || _t->getType() == LocalLabel)) {
				asmLabel(_t);
				_t = _retTree;
			}
			else {
				goto _loop269;
			}
			
		}
		_loop269:;
		} // ( ... )*
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_tokenSet_0.member(_t->getType()))) {
				asmStatementLine(_t);
				_t = _retTree;
			}
			else {
				goto _loop271;
			}
			
		}
		_loop271:;
		} // ( ... )*
		_t = __t267;
		_t = _t->getNextSibling();
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmLabel(antlr::RefAST _t) {
	antlr::RefAST asmLabel_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST l = antlr::nullAST;
	antlr::RefAST ll = antlr::nullAST;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case Label:
		{
			l = _t;
			match(_t,Label);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 554 "Asm.g"
				std::cout << l->getText() << std::endl;
#line 153 "AsmTreeParser.cpp"
			}
			break;
		}
		case LocalLabel:
		{
			ll = _t;
			match(_t,LocalLabel);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 555 "Asm.g"
				std::cout << ll->getText() << std::endl;
#line 165 "AsmTreeParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmStatementLine(antlr::RefAST _t) {
	antlr::RefAST asmStatementLine_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		asmStatement(_t);
		_t = _retTree;
		if ( inputState->guessing==0 ) {
#line 558 "Asm.g"
			std::cout << std::endl;
#line 196 "AsmTreeParser.cpp"
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmStatement(antlr::RefAST _t) {
	antlr::RefAST asmStatement_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTInstruction:
		case ASTInstructionPrefix:
		case ASTSensitive:
		case ASTGprof:
		case ASTGcov:
		{
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ASTInstructionPrefix:
			{
				asmInstrPrefix(_t);
				_t = _retTree;
				break;
			}
			case ASTInstruction:
			case ASTSensitive:
			case ASTGprof:
			case ASTGcov:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			asmInstr(_t);
			_t = _retTree;
			break;
		}
		case ASTMacroDef:
		case ASTCommand:
		case ASTDbgCommand:
		{
			asmCommand(_t);
			_t = _retTree;
			break;
		}
		case ASSIGN:
		{
			asmAssignment(_t);
			_t = _retTree;
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmInstrPrefix(antlr::RefAST _t) {
	antlr::RefAST asmInstrPrefix_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST i = antlr::nullAST;
	
	try {      // for error handling
		antlr::RefAST __t292 = _t;
		antlr::RefAST tmp2_AST_in = _t;
		match(_t,ASTInstructionPrefix);
		_t = _t->getFirstChild();
		i = _t;
		if ( _t == antlr::nullAST ) throw antlr::MismatchedTokenException();
		_t = _t->getNextSibling();
		if ( inputState->guessing==0 ) {
#line 597 "Asm.g"
			std::cout << i->getText() << ';';
#line 298 "AsmTreeParser.cpp"
		}
		_t = __t292;
		_t = _t->getNextSibling();
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmInstr(antlr::RefAST _t) {
	antlr::RefAST asmInstr_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST i = antlr::nullAST;
#line 600 "Asm.g"
	antlr::RefAST r;
#line 320 "AsmTreeParser.cpp"
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTInstruction:
		{
			antlr::RefAST __t294 = _t;
			antlr::RefAST tmp3_AST_in = _t;
			match(_t,ASTInstruction);
			_t = _t->getFirstChild();
			i = _t;
			if ( _t == antlr::nullAST ) throw antlr::MismatchedTokenException();
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 601 "Asm.g"
				std::cout << '\t' << i->getText();
#line 338 "AsmTreeParser.cpp"
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ID:
			case Command:
			case Int:
			case Hex:
			case STAR:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DIV:
			case PERCENT:
			case SHIFTLEFT:
			case SHIFTRIGHT:
			case AND:
			case OR:
			case XOR:
			case PLUS:
			case DOLLAR:
			case ASTRegisterDisplacement:
			case ASTRegisterBaseIndexScale:
			case ASTRegisterIndex:
			case ASTDereference:
			case ASTSegment:
			case ASTRegister:
			case ASTNegative:
			{
				instrParams(_t);
				_t = _retTree;
				break;
			}
			case 3:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			_t = __t294;
			_t = _t->getNextSibling();
			break;
		}
		case ASTSensitive:
		{
			antlr::RefAST __t296 = _t;
			antlr::RefAST tmp4_AST_in = _t;
			match(_t,ASTSensitive);
			_t = _t->getFirstChild();
			r=asmSensitiveInstr(_t);
			_t = _retTree;
			if ( inputState->guessing==0 ) {
#line 603 "Asm.g"
				startSensitive(r);
#line 398 "AsmTreeParser.cpp"
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ID:
			case Command:
			case Int:
			case Hex:
			case STAR:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DIV:
			case PERCENT:
			case SHIFTLEFT:
			case SHIFTRIGHT:
			case AND:
			case OR:
			case XOR:
			case PLUS:
			case DOLLAR:
			case ASTRegisterDisplacement:
			case ASTRegisterBaseIndexScale:
			case ASTRegisterIndex:
			case ASTDereference:
			case ASTSegment:
			case ASTRegister:
			case ASTNegative:
			{
				instrParams(_t);
				_t = _retTree;
				break;
			}
			case 3:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			if ( inputState->guessing==0 ) {
#line 604 "Asm.g"
				endSensitive(r);
#line 446 "AsmTreeParser.cpp"
			}
			_t = __t296;
			_t = _t->getNextSibling();
			break;
		}
		case ASTGprof:
		case ASTGcov:
		{
			asmGprof(_t);
			_t = _retTree;
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmCommand(antlr::RefAST _t) {
	antlr::RefAST asmCommand_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST c = antlr::nullAST;
	antlr::RefAST d = antlr::nullAST;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTMacroDef:
		{
			asmMacroDef(_t);
			_t = _retTree;
			break;
		}
		case ASTCommand:
		{
			antlr::RefAST __t279 = _t;
			antlr::RefAST tmp5_AST_in = _t;
			match(_t,ASTCommand);
			_t = _t->getFirstChild();
			c = _t;
			match(_t,Command);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 572 "Asm.g"
				std::cout << '\t' << c->getText();
#line 504 "AsmTreeParser.cpp"
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ID:
			case Command:
			case String:
			case Option:
			case Int:
			case Hex:
			case STAR:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DIV:
			case PERCENT:
			case SHIFTLEFT:
			case SHIFTRIGHT:
			case AND:
			case OR:
			case XOR:
			case PLUS:
			case DOLLAR:
			case ASTDefaultParam:
			case ASTRegisterDisplacement:
			case ASTRegisterBaseIndexScale:
			case ASTRegisterIndex:
			case ASTDereference:
			case ASTSegment:
			case ASTRegister:
			case ASTNegative:
			{
				commandParams(_t);
				_t = _retTree;
				break;
			}
			case 3:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			_t = __t279;
			_t = _t->getNextSibling();
			break;
		}
		case ASTDbgCommand:
		{
			antlr::RefAST __t281 = _t;
			antlr::RefAST tmp6_AST_in = _t;
			match(_t,ASTDbgCommand);
			_t = _t->getFirstChild();
			d = _t;
			match(_t,Command);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 575 "Asm.g"
				std::cout << '\t' << d->getText();
#line 568 "AsmTreeParser.cpp"
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ID:
			case Command:
			case String:
			case Option:
			case Int:
			case Hex:
			case STAR:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DIV:
			case PERCENT:
			case SHIFTLEFT:
			case SHIFTRIGHT:
			case AND:
			case OR:
			case XOR:
			case PLUS:
			case DOLLAR:
			case ASTDefaultParam:
			case ASTRegisterDisplacement:
			case ASTRegisterBaseIndexScale:
			case ASTRegisterIndex:
			case ASTDereference:
			case ASTSegment:
			case ASTRegister:
			case ASTNegative:
			{
				dbgCommandParams(_t);
				_t = _retTree;
				break;
			}
			case 3:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			_t = __t281;
			_t = _t->getNextSibling();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmAssignment(antlr::RefAST _t) {
	antlr::RefAST asmAssignment_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST l = antlr::nullAST;
	
	try {      // for error handling
		antlr::RefAST __t277 = _t;
		antlr::RefAST tmp7_AST_in = _t;
		match(_t,ASSIGN);
		_t = _t->getFirstChild();
		l = _t;
		if ( _t == antlr::nullAST ) throw antlr::MismatchedTokenException();
		_t = _t->getNextSibling();
		if ( inputState->guessing==0 ) {
#line 567 "Asm.g"
			std::cout << l->getText() << '=';
#line 653 "AsmTreeParser.cpp"
		}
		expr(_t);
		_t = _retTree;
		_t = __t277;
		_t = _t->getNextSibling();
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::expr(antlr::RefAST _t) {
	antlr::RefAST expr_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST p = antlr::nullAST;
	antlr::RefAST m = antlr::nullAST;
	antlr::RefAST o = antlr::nullAST;
	antlr::RefAST n = antlr::nullAST;
	antlr::RefAST s = antlr::nullAST;
	antlr::RefAST d = antlr::nullAST;
	antlr::RefAST a = antlr::nullAST;
	antlr::RefAST x = antlr::nullAST;
	antlr::RefAST sl = antlr::nullAST;
	antlr::RefAST sr = antlr::nullAST;
	antlr::RefAST p2 = antlr::nullAST;
	antlr::RefAST D = antlr::nullAST;
	antlr::RefAST r = antlr::nullAST;
	antlr::RefAST ri = antlr::nullAST;
	antlr::RefAST in = antlr::nullAST;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case PLUS:
		{
			antlr::RefAST __t324 = _t;
			p = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,PLUS);
			_t = _t->getFirstChild();
			expr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt326=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 666 "Asm.g"
						print(p);
#line 711 "AsmTreeParser.cpp"
					}
					expr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt326>=1 ) { goto _loop326; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt326++;
			}
			_loop326:;
			}  // ( ... )+
			_t = __t324;
			_t = _t->getNextSibling();
			break;
		}
		case MINUS:
		{
			antlr::RefAST __t327 = _t;
			m = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,MINUS);
			_t = _t->getFirstChild();
			expr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt329=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 667 "Asm.g"
						print(m);
#line 745 "AsmTreeParser.cpp"
					}
					expr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt329>=1 ) { goto _loop329; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt329++;
			}
			_loop329:;
			}  // ( ... )+
			_t = __t327;
			_t = _t->getNextSibling();
			break;
		}
		case OR:
		{
			antlr::RefAST __t330 = _t;
			o = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,OR);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt332=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 668 "Asm.g"
						print(o);
#line 779 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt332>=1 ) { goto _loop332; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt332++;
			}
			_loop332:;
			}  // ( ... )+
			_t = __t330;
			_t = _t->getNextSibling();
			break;
		}
		case NOT:
		{
			antlr::RefAST __t333 = _t;
			n = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,NOT);
			_t = _t->getFirstChild();
			if ( inputState->guessing==0 ) {
#line 669 "Asm.g"
				print(n);
#line 805 "AsmTreeParser.cpp"
			}
			subexpr(_t);
			_t = _retTree;
			_t = __t333;
			_t = _t->getNextSibling();
			break;
		}
		case ASTNegative:
		{
			antlr::RefAST __t334 = _t;
			antlr::RefAST tmp8_AST_in = _t;
			match(_t,ASTNegative);
			_t = _t->getFirstChild();
			if ( inputState->guessing==0 ) {
#line 670 "Asm.g"
				print('-');
#line 822 "AsmTreeParser.cpp"
			}
			subexpr(_t);
			_t = _retTree;
			_t = __t334;
			_t = _t->getNextSibling();
			break;
		}
		case STAR:
		{
			antlr::RefAST __t335 = _t;
			s = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,STAR);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt337=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 671 "Asm.g"
						print(s);
#line 847 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt337>=1 ) { goto _loop337; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt337++;
			}
			_loop337:;
			}  // ( ... )+
			_t = __t335;
			_t = _t->getNextSibling();
			break;
		}
		case DIV:
		{
			antlr::RefAST __t338 = _t;
			d = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,DIV);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt340=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 672 "Asm.g"
						print(d);
#line 881 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt340>=1 ) { goto _loop340; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt340++;
			}
			_loop340:;
			}  // ( ... )+
			_t = __t338;
			_t = _t->getNextSibling();
			break;
		}
		case AND:
		{
			antlr::RefAST __t341 = _t;
			a = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,AND);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt343=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 673 "Asm.g"
						print(a);
#line 915 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt343>=1 ) { goto _loop343; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt343++;
			}
			_loop343:;
			}  // ( ... )+
			_t = __t341;
			_t = _t->getNextSibling();
			break;
		}
		case XOR:
		{
			antlr::RefAST __t344 = _t;
			x = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,XOR);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt346=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 674 "Asm.g"
						print(x);
#line 949 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt346>=1 ) { goto _loop346; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt346++;
			}
			_loop346:;
			}  // ( ... )+
			_t = __t344;
			_t = _t->getNextSibling();
			break;
		}
		case SHIFTLEFT:
		{
			antlr::RefAST __t347 = _t;
			sl = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,SHIFTLEFT);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt349=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 675 "Asm.g"
						print(sl);
#line 983 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt349>=1 ) { goto _loop349; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt349++;
			}
			_loop349:;
			}  // ( ... )+
			_t = __t347;
			_t = _t->getNextSibling();
			break;
		}
		case SHIFTRIGHT:
		{
			antlr::RefAST __t350 = _t;
			sr = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,SHIFTRIGHT);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt352=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 676 "Asm.g"
						print(sr);
#line 1017 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt352>=1 ) { goto _loop352; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt352++;
			}
			_loop352:;
			}  // ( ... )+
			_t = __t350;
			_t = _t->getNextSibling();
			break;
		}
		case PERCENT:
		{
			antlr::RefAST __t353 = _t;
			p2 = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,PERCENT);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			{ // ( ... )+
			int _cnt355=0;
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_tokenSet_1.member(_t->getType()))) {
					if ( inputState->guessing==0 ) {
#line 677 "Asm.g"
						print(p2);
#line 1051 "AsmTreeParser.cpp"
					}
					subexpr(_t);
					_t = _retTree;
				}
				else {
					if ( _cnt355>=1 ) { goto _loop355; } else {throw antlr::NoViableAltException(_t);}
				}
				
				_cnt355++;
			}
			_loop355:;
			}  // ( ... )+
			_t = __t353;
			_t = _t->getNextSibling();
			break;
		}
		case DOLLAR:
		{
			antlr::RefAST __t356 = _t;
			D = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
			match(_t,DOLLAR);
			_t = _t->getFirstChild();
			if ( inputState->guessing==0 ) {
#line 678 "Asm.g"
				print(D);
#line 1077 "AsmTreeParser.cpp"
			}
			subexpr(_t);
			_t = _retTree;
			_t = __t356;
			_t = _t->getNextSibling();
			break;
		}
		case ID:
		case Command:
		case Int:
		case Hex:
		case RelativeLocation:
		case ASTRegister:
		{
			primitive(_t);
			_t = _retTree;
			break;
		}
		case ASTDereference:
		{
			antlr::RefAST __t357 = _t;
			antlr::RefAST tmp9_AST_in = _t;
			match(_t,ASTDereference);
			_t = _t->getFirstChild();
			if ( inputState->guessing==0 ) {
#line 680 "Asm.g"
				std::cout << '*';
#line 1105 "AsmTreeParser.cpp"
			}
			expr(_t);
			_t = _retTree;
			_t = __t357;
			_t = _t->getNextSibling();
			break;
		}
		case ASTSegment:
		{
			antlr::RefAST __t358 = _t;
			antlr::RefAST tmp10_AST_in = _t;
			match(_t,ASTSegment);
			_t = _t->getFirstChild();
			r = _t;
			match(_t,ASTRegister);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 681 "Asm.g"
				std::cout << r->getText() << ':';
#line 1125 "AsmTreeParser.cpp"
			}
			expr(_t);
			_t = _retTree;
			_t = __t358;
			_t = _t->getNextSibling();
			break;
		}
		case ASTRegisterDisplacement:
		{
			antlr::RefAST __t359 = _t;
			antlr::RefAST tmp11_AST_in = _t;
			match(_t,ASTRegisterDisplacement);
			_t = _t->getFirstChild();
			subexpr(_t);
			_t = _retTree;
			expr(_t);
			_t = _retTree;
			_t = __t359;
			_t = _t->getNextSibling();
			break;
		}
		case ASTRegisterBaseIndexScale:
		{
			antlr::RefAST __t360 = _t;
			antlr::RefAST tmp12_AST_in = _t;
			match(_t,ASTRegisterBaseIndexScale);
			_t = _t->getFirstChild();
			regOffsetBase(_t);
			_t = _retTree;
			_t = __t360;
			_t = _t->getNextSibling();
			break;
		}
		case ASTRegisterIndex:
		{
			antlr::RefAST __t361 = _t;
			antlr::RefAST tmp13_AST_in = _t;
			match(_t,ASTRegisterIndex);
			_t = _t->getFirstChild();
			ri = _t;
			match(_t,ASTRegister);
			_t = _t->getNextSibling();
			in = _t;
			match(_t,Int);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 685 "Asm.g"
				std::cout << ri->getText() << '(' << in->getText() << ')';
#line 1174 "AsmTreeParser.cpp"
			}
			_t = __t361;
			_t = _t->getNextSibling();
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmMacroDef(antlr::RefAST _t) {
	antlr::RefAST asmMacroDef_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST i = antlr::nullAST;
	
	try {      // for error handling
		antlr::RefAST __t284 = _t;
		antlr::RefAST tmp14_AST_in = _t;
		match(_t,ASTMacroDef);
		_t = _t->getFirstChild();
		i = _t;
		match(_t,ID);
		_t = _t->getNextSibling();
		if ( inputState->guessing==0 ) {
#line 581 "Asm.g"
			std::cout << ".macro " << i->getText();
#line 1213 "AsmTreeParser.cpp"
		}
		asmMacroDefParams(_t);
		_t = _retTree;
		if ( inputState->guessing==0 ) {
#line 582 "Asm.g"
			std::cout << std::endl;
#line 1220 "AsmTreeParser.cpp"
		}
		if ( inputState->guessing==0 ) {
#line 583 "Asm.g"
			this->in_macro=true;
#line 1225 "AsmTreeParser.cpp"
		}
		asmBlocks(_t);
		_t = _retTree;
		if ( inputState->guessing==0 ) {
#line 585 "Asm.g"
			this->in_macro=false;
#line 1232 "AsmTreeParser.cpp"
		}
		if ( inputState->guessing==0 ) {
#line 586 "Asm.g"
			std::cout << ".endm" << std::endl;
#line 1237 "AsmTreeParser.cpp"
		}
		_t = __t284;
		_t = _t->getNextSibling();
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::commandParams(antlr::RefAST _t) {
	antlr::RefAST commandParams_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		if ( inputState->guessing==0 ) {
#line 614 "Asm.g"
			std::cout << '\t';
#line 1261 "AsmTreeParser.cpp"
		}
		commandParam(_t);
		_t = _retTree;
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_tokenSet_2.member(_t->getType()))) {
				if ( inputState->guessing==0 ) {
#line 615 "Asm.g"
					std::cout << ',' << ' ';
#line 1273 "AsmTreeParser.cpp"
				}
				commandParam(_t);
				_t = _retTree;
			}
			else {
				goto _loop303;
			}
			
		}
		_loop303:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::dbgCommandParams(antlr::RefAST _t) {
	antlr::RefAST dbgCommandParams_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		if ( inputState->guessing==0 ) {
#line 618 "Asm.g"
			std::cout << '\t';
#line 1305 "AsmTreeParser.cpp"
		}
		commandParam(_t);
		_t = _retTree;
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_tokenSet_2.member(_t->getType()))) {
				if ( inputState->guessing==0 ) {
#line 619 "Asm.g"
					std::cout << ' ';
#line 1317 "AsmTreeParser.cpp"
				}
				commandParam(_t);
				_t = _retTree;
			}
			else {
				goto _loop306;
			}
			
		}
		_loop306:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::asmMacroDefParams(antlr::RefAST _t) {
	antlr::RefAST asmMacroDefParams_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST i = antlr::nullAST;
	antlr::RefAST ii = antlr::nullAST;
	
	try {      // for error handling
		antlr::RefAST __t286 = _t;
		antlr::RefAST tmp15_AST_in = _t;
		match(_t,ASTMacroDefParams);
		_t = _t->getFirstChild();
		{
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ID:
		{
			{
			i = _t;
			match(_t,ID);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 591 "Asm.g"
				std::cout << ' ' << i->getText();
#line 1365 "AsmTreeParser.cpp"
			}
			}
			{ // ( ... )*
			for (;;) {
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				if ((_t->getType() == ID)) {
					ii = _t;
					match(_t,ID);
					_t = _t->getNextSibling();
					if ( inputState->guessing==0 ) {
#line 592 "Asm.g"
						std::cout << ", " << ii->getText();
#line 1379 "AsmTreeParser.cpp"
					}
				}
				else {
					goto _loop290;
				}
				
			}
			_loop290:;
			} // ( ... )*
			break;
		}
		case 3:
		{
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
		}
		_t = __t286;
		_t = _t->getNextSibling();
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::instrParams(antlr::RefAST _t) {
	antlr::RefAST instrParams_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		if ( inputState->guessing==0 ) {
#line 629 "Asm.g"
			std::cout << '\t';
#line 1423 "AsmTreeParser.cpp"
		}
		instrParam(_t);
		_t = _retTree;
		{ // ( ... )*
		for (;;) {
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			if ((_tokenSet_1.member(_t->getType()))) {
				if ( inputState->guessing==0 ) {
#line 630 "Asm.g"
					std::cout << ',' << ' ';
#line 1435 "AsmTreeParser.cpp"
				}
				instrParam(_t);
				_t = _retTree;
			}
			else {
				goto _loop310;
			}
			
		}
		_loop310:;
		} // ( ... )*
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

antlr::RefAST  AsmTreeParser::asmSensitiveInstr(antlr::RefAST _t) {
#line 688 "Asm.g"
	antlr::RefAST r;
#line 1463 "AsmTreeParser.cpp"
	antlr::RefAST asmSensitiveInstr_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
#line 688 "Asm.g"
	pad=8; r=_t;
#line 1467 "AsmTreeParser.cpp"
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case IA32_popf:
		{
			antlr::RefAST tmp16_AST_in = _t;
			match(_t,IA32_popf);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 689 "Asm.g"
				pad=21;
#line 1481 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_pushf:
		{
			antlr::RefAST tmp17_AST_in = _t;
			match(_t,IA32_pushf);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 690 "Asm.g"
				pad=5;
#line 1493 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lgdt:
		{
			antlr::RefAST tmp18_AST_in = _t;
			match(_t,IA32_lgdt);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 691 "Asm.g"
				pad=9;
#line 1505 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_sgdt:
		{
			antlr::RefAST tmp19_AST_in = _t;
			match(_t,IA32_sgdt);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_lidt:
		{
			antlr::RefAST tmp20_AST_in = _t;
			match(_t,IA32_lidt);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 693 "Asm.g"
				pad=9;
#line 1524 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_sidt:
		{
			antlr::RefAST tmp21_AST_in = _t;
			match(_t,IA32_sidt);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_ljmp:
		{
			antlr::RefAST tmp22_AST_in = _t;
			match(_t,IA32_ljmp);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 695 "Asm.g"
				pad=9;
#line 1543 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lds:
		{
			antlr::RefAST tmp23_AST_in = _t;
			match(_t,IA32_lds);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 696 "Asm.g"
				pad=16;
#line 1555 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_les:
		{
			antlr::RefAST tmp24_AST_in = _t;
			match(_t,IA32_les);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 697 "Asm.g"
				pad=16;
#line 1567 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lfs:
		{
			antlr::RefAST tmp25_AST_in = _t;
			match(_t,IA32_lfs);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 698 "Asm.g"
				pad=16;
#line 1579 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lgs:
		{
			antlr::RefAST tmp26_AST_in = _t;
			match(_t,IA32_lgs);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 699 "Asm.g"
				pad=16;
#line 1591 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lss:
		{
			antlr::RefAST tmp27_AST_in = _t;
			match(_t,IA32_lss);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 700 "Asm.g"
				pad=16;
#line 1603 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_clts:
		{
			antlr::RefAST tmp28_AST_in = _t;
			match(_t,IA32_clts);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 701 "Asm.g"
				pad=14;
#line 1615 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_hlt:
		{
			antlr::RefAST tmp29_AST_in = _t;
			match(_t,IA32_hlt);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 702 "Asm.g"
				pad=6;
#line 1627 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_cli:
		{
			antlr::RefAST tmp30_AST_in = _t;
			match(_t,IA32_cli);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 703 "Asm.g"
				pad=7;
#line 1639 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_sti:
		{
			antlr::RefAST tmp31_AST_in = _t;
			match(_t,IA32_sti);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 704 "Asm.g"
				pad=23;
#line 1651 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lldt:
		{
			antlr::RefAST tmp32_AST_in = _t;
			match(_t,IA32_lldt);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 705 "Asm.g"
				pad=16;
#line 1663 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_sldt:
		{
			antlr::RefAST tmp33_AST_in = _t;
			match(_t,IA32_sldt);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 706 "Asm.g"
				pad=6;
#line 1675 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_ltr:
		{
			antlr::RefAST tmp34_AST_in = _t;
			match(_t,IA32_ltr);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 707 "Asm.g"
				pad=16;
#line 1687 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_str:
		{
			antlr::RefAST tmp35_AST_in = _t;
			match(_t,IA32_str);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 708 "Asm.g"
				pad=9;
#line 1699 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_in:
		{
			antlr::RefAST tmp36_AST_in = _t;
			match(_t,IA32_in);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 709 "Asm.g"
				pad=13;
#line 1711 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_out:
		{
			antlr::RefAST tmp37_AST_in = _t;
			match(_t,IA32_out);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 710 "Asm.g"
				pad=16;
#line 1723 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_invlpg:
		{
			antlr::RefAST tmp38_AST_in = _t;
			match(_t,IA32_invlpg);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 711 "Asm.g"
				pad=6;
#line 1735 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_iret:
		{
			antlr::RefAST tmp39_AST_in = _t;
			match(_t,IA32_iret);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 712 "Asm.g"
				pad=4;
#line 1747 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_lret:
		{
			antlr::RefAST tmp40_AST_in = _t;
			match(_t,IA32_lret);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 713 "Asm.g"
				pad=4;
#line 1759 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_cpuid:
		{
			antlr::RefAST tmp41_AST_in = _t;
			match(_t,IA32_cpuid);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_wrmsr:
		{
			antlr::RefAST tmp42_AST_in = _t;
			match(_t,IA32_wrmsr);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_rdmsr:
		{
			antlr::RefAST tmp43_AST_in = _t;
			match(_t,IA32_rdmsr);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_int:
		{
			antlr::RefAST tmp44_AST_in = _t;
			match(_t,IA32_int);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 717 "Asm.g"
				pad=11;
#line 1792 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_ud2:
		{
			antlr::RefAST tmp45_AST_in = _t;
			match(_t,IA32_ud2);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_invd:
		{
			antlr::RefAST tmp46_AST_in = _t;
			match(_t,IA32_invd);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_wbinvd:
		{
			antlr::RefAST tmp47_AST_in = _t;
			match(_t,IA32_wbinvd);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_smsw:
		{
			antlr::RefAST tmp48_AST_in = _t;
			match(_t,IA32_smsw);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_lmsw:
		{
			antlr::RefAST tmp49_AST_in = _t;
			match(_t,IA32_lmsw);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_arpl:
		{
			antlr::RefAST tmp50_AST_in = _t;
			match(_t,IA32_arpl);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_lar:
		{
			antlr::RefAST tmp51_AST_in = _t;
			match(_t,IA32_lar);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_lsl:
		{
			antlr::RefAST tmp52_AST_in = _t;
			match(_t,IA32_lsl);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_rsm:
		{
			antlr::RefAST tmp53_AST_in = _t;
			match(_t,IA32_rsm);
			_t = _t->getNextSibling();
			break;
		}
		case IA32_pop:
		{
			antlr::RefAST tmp54_AST_in = _t;
			match(_t,IA32_pop);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 727 "Asm.g"
				pad=9;
#line 1867 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_push:
		{
			antlr::RefAST tmp55_AST_in = _t;
			match(_t,IA32_push);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 728 "Asm.g"
				pad=5;
#line 1879 "AsmTreeParser.cpp"
			}
			break;
		}
		case IA32_mov:
		{
			antlr::RefAST tmp56_AST_in = _t;
			match(_t,IA32_mov);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 729 "Asm.g"
				pad=14;
#line 1891 "AsmTreeParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
	return r;
}

void AsmTreeParser::asmGprof(antlr::RefAST _t) {
	antlr::RefAST asmGprof_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTGprof:
		{
			antlr::RefAST __t299 = _t;
			antlr::RefAST tmp57_AST_in = _t;
			match(_t,ASTGprof);
			_t = _t->getFirstChild();
			antlr::RefAST tmp58_AST_in = _t;
			match(_t,IA32_call);
			_t = _t->getNextSibling();
			antlr::RefAST tmp59_AST_in = _t;
			match(_t,LITERAL_mcount);
			_t = _t->getNextSibling();
			_t = __t299;
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 609 "Asm.g"
				emit_gprof();
#line 1938 "AsmTreeParser.cpp"
			}
			break;
		}
		case ASTGcov:
		{
			antlr::RefAST __t300 = _t;
			antlr::RefAST tmp60_AST_in = _t;
			match(_t,ASTGcov);
			_t = _t->getFirstChild();
			antlr::RefAST tmp61_AST_in = _t;
			match(_t,IA32_call);
			_t = _t->getNextSibling();
			antlr::RefAST tmp62_AST_in = _t;
			match(_t,LITERAL___gcov_merge_add);
			_t = _t->getNextSibling();
			_t = __t300;
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 610 "Asm.g"
				emit_gcov();
#line 1959 "AsmTreeParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::commandParam(antlr::RefAST _t) {
	antlr::RefAST commandParam_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST s = antlr::nullAST;
	antlr::RefAST o = antlr::nullAST;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case String:
		{
			s = _t;
			match(_t,String);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 622 "Asm.g"
				print(s);
#line 1998 "AsmTreeParser.cpp"
			}
			break;
		}
		case Option:
		{
			o = _t;
			match(_t,Option);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 623 "Asm.g"
				print(o);
#line 2010 "AsmTreeParser.cpp"
			}
			break;
		}
		case ASTDefaultParam:
		{
			antlr::RefAST tmp63_AST_in = _t;
			match(_t,ASTDefaultParam);
			_t = _t->getNextSibling();
			break;
		}
		case ID:
		case Command:
		case Int:
		case Hex:
		case STAR:
		case RelativeLocation:
		case MINUS:
		case NOT:
		case DIV:
		case PERCENT:
		case SHIFTLEFT:
		case SHIFTRIGHT:
		case AND:
		case OR:
		case XOR:
		case PLUS:
		case DOLLAR:
		case ASTRegisterDisplacement:
		case ASTRegisterBaseIndexScale:
		case ASTRegisterIndex:
		case ASTDereference:
		case ASTSegment:
		case ASTRegister:
		case ASTNegative:
		{
			instrParam(_t);
			_t = _retTree;
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::instrParam(antlr::RefAST _t) {
	antlr::RefAST instrParam_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		regExpression(_t);
		_t = _retTree;
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::regExpression(antlr::RefAST _t) {
	antlr::RefAST regExpression_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		expr(_t);
		_t = _retTree;
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::regOffsetBase(antlr::RefAST _t) {
	antlr::RefAST regOffsetBase_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST r1 = antlr::nullAST;
	antlr::RefAST r2 = antlr::nullAST;
	antlr::RefAST i = antlr::nullAST;
	
	try {      // for error handling
		if ( inputState->guessing==0 ) {
#line 643 "Asm.g"
			std::cout << '(';
#line 2116 "AsmTreeParser.cpp"
		}
		{
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTDefaultParam:
		{
			antlr::RefAST tmp64_AST_in = _t;
			match(_t,ASTDefaultParam);
			_t = _t->getNextSibling();
			break;
		}
		case ASTRegister:
		{
			r1 = _t;
			match(_t,ASTRegister);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 644 "Asm.g"
				print(r1);
#line 2137 "AsmTreeParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
		}
		{
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ASTDefaultParam:
		case ASTRegister:
		{
			if ( inputState->guessing==0 ) {
#line 645 "Asm.g"
				std::cout << ',';
#line 2157 "AsmTreeParser.cpp"
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case ASTDefaultParam:
			{
				antlr::RefAST tmp65_AST_in = _t;
				match(_t,ASTDefaultParam);
				_t = _t->getNextSibling();
				break;
			}
			case ASTRegister:
			{
				r2 = _t;
				match(_t,ASTRegister);
				_t = _t->getNextSibling();
				if ( inputState->guessing==0 ) {
#line 645 "Asm.g"
					print(r2);
#line 2178 "AsmTreeParser.cpp"
				}
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			{
			if (_t == antlr::nullAST )
				_t = ASTNULL;
			switch ( _t->getType()) {
			case Int:
			case ASTDefaultParam:
			{
				if ( inputState->guessing==0 ) {
#line 646 "Asm.g"
					std::cout << ',';
#line 2198 "AsmTreeParser.cpp"
				}
				{
				if (_t == antlr::nullAST )
					_t = ASTNULL;
				switch ( _t->getType()) {
				case Int:
				{
					i = _t;
					match(_t,Int);
					_t = _t->getNextSibling();
					if ( inputState->guessing==0 ) {
#line 646 "Asm.g"
						print(i);
#line 2212 "AsmTreeParser.cpp"
					}
					break;
				}
				case ASTDefaultParam:
				{
					antlr::RefAST tmp66_AST_in = _t;
					match(_t,ASTDefaultParam);
					_t = _t->getNextSibling();
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(_t);
				}
				}
				}
				break;
			}
			case 3:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(_t);
			}
			}
			}
			break;
		}
		case 3:
		{
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
		}
		if ( inputState->guessing==0 ) {
#line 648 "Asm.g"
			std::cout << ')';
#line 2256 "AsmTreeParser.cpp"
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::primitive(antlr::RefAST _t) {
	antlr::RefAST primitive_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	antlr::RefAST i = antlr::nullAST;
	antlr::RefAST n = antlr::nullAST;
	antlr::RefAST h = antlr::nullAST;
	antlr::RefAST c = antlr::nullAST;
	antlr::RefAST r = antlr::nullAST;
	antlr::RefAST rl = antlr::nullAST;
	
	try {      // for error handling
		if (_t == antlr::nullAST )
			_t = ASTNULL;
		switch ( _t->getType()) {
		case ID:
		{
			i = _t;
			match(_t,ID);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 652 "Asm.g"
				print(i);
#line 2292 "AsmTreeParser.cpp"
			}
			break;
		}
		case Int:
		{
			n = _t;
			match(_t,Int);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 653 "Asm.g"
				print(n);
#line 2304 "AsmTreeParser.cpp"
			}
			break;
		}
		case Hex:
		{
			h = _t;
			match(_t,Hex);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 654 "Asm.g"
				print(h);
#line 2316 "AsmTreeParser.cpp"
			}
			break;
		}
		case Command:
		{
			c = _t;
			match(_t,Command);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 655 "Asm.g"
				print(c);
#line 2328 "AsmTreeParser.cpp"
			}
			break;
		}
		case ASTRegister:
		{
			r = _t;
			match(_t,ASTRegister);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 656 "Asm.g"
				print(r);
#line 2340 "AsmTreeParser.cpp"
			}
			break;
		}
		case RelativeLocation:
		{
			rl = _t;
			match(_t,RelativeLocation);
			_t = _t->getNextSibling();
			if ( inputState->guessing==0 ) {
#line 657 "Asm.g"
				print(rl);
#line 2352 "AsmTreeParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(_t);
		}
		}
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::subexpr(antlr::RefAST _t) {
	antlr::RefAST subexpr_AST_in = (_t == antlr::RefAST(ASTNULL)) ? antlr::nullAST : _t;
	
	try {      // for error handling
		bool synPredMatched322 = false;
		if (((_tokenSet_1.member(_t->getType())))) {
			antlr::RefAST __t322 = _t;
			synPredMatched322 = true;
			inputState->guessing++;
			try {
				{
				primitive(_t);
				_t = _retTree;
				}
			}
			catch (antlr::RecognitionException& pe) {
				synPredMatched322 = false;
			}
			_t = __t322;
			inputState->guessing--;
		}
		if ( synPredMatched322 ) {
			expr(_t);
			_t = _retTree;
		}
		else if ((_tokenSet_1.member(_t->getType()))) {
			if ( inputState->guessing==0 ) {
#line 662 "Asm.g"
				print('(');
#line 2403 "AsmTreeParser.cpp"
			}
			expr(_t);
			_t = _retTree;
			if ( inputState->guessing==0 ) {
#line 662 "Asm.g"
				print(')');
#line 2410 "AsmTreeParser.cpp"
			}
		}
		else {
			throw antlr::NoViableAltException(_t);
		}
		
	}
	catch (antlr::RecognitionException& ex) {
		if( inputState->guessing == 0 ) {
			reportError(ex);
			if ( _t != antlr::nullAST )
				_t = _t->getNextSibling();
		} else {
			throw;
		}
	}
	_retTree = _t;
}

void AsmTreeParser::initializeASTFactory( antlr::ASTFactory& )
{
}
const char* AsmTreeParser::tokenNames[] = {
	"<0>",
	"EOF",
	"<2>",
	"NULL_TREE_LOOKAHEAD",
	"Newline",
	"SEMI",
	"ID",
	"ASSIGN",
	"Label",
	"LocalLabel",
	"COMMA",
	"\".macro\"",
	"\".endm\"",
	"Command",
	"\".file\"",
	"\".loc\"",
	"\"mcount\"",
	"\"__gcov_merge_add\"",
	"String",
	"Option",
	"Int",
	"Hex",
	"STAR",
	"COLON",
	"LPAREN",
	"RPAREN",
	"Reg",
	"RelativeLocation",
	"MINUS",
	"NOT",
	"DIV",
	"PERCENT",
	"SHIFTLEFT",
	"SHIFTRIGHT",
	"AND",
	"OR",
	"XOR",
	"PLUS",
	"DOLLAR",
	"\"%al\"",
	"\"%bl\"",
	"\"%cl\"",
	"\"%dl\"",
	"\"%ah\"",
	"\"%bh\"",
	"\"%ch\"",
	"\"%dh\"",
	"\"%ax\"",
	"\"%bx\"",
	"\"%cx\"",
	"\"%dx\"",
	"\"%si\"",
	"\"%di\"",
	"\"%sp\"",
	"\"%bp\"",
	"\"%eax\"",
	"\"%ebx\"",
	"\"%ecx\"",
	"\"%edx\"",
	"\"%esi\"",
	"\"%edi\"",
	"\"%esp\"",
	"\"%ebp\"",
	"\"%st\"",
	"\"%cs\"",
	"\"%ds\"",
	"\"%es\"",
	"\"%fs\"",
	"\"%gs\"",
	"\"%ss\"",
	"\"%cr0\"",
	"\"%cr2\"",
	"\"%cr3\"",
	"\"%cr4\"",
	"\"%db0\"",
	"\"%db1\"",
	"\"%db2\"",
	"\"%db3\"",
	"\"%db4\"",
	"\"%db5\"",
	"\"%db6\"",
	"\"%db7\"",
	"\"lock\"",
	"\"rep\"",
	"\"repnz\"",
	"\"call\"",
	"\"popf\"",
	"\"popfl\"",
	"\"popfd\"",
	"\"pushf\"",
	"\"pushfl\"",
	"\"pushfd\"",
	"\"lgdt\"",
	"\"lgdtl\"",
	"\"lgdtd\"",
	"\"sgdt\"",
	"\"sgdtl\"",
	"\"sgdtd\"",
	"\"lidt\"",
	"\"lidtl\"",
	"\"lidtd\"",
	"\"sidt\"",
	"\"sidtl\"",
	"\"sidtd\"",
	"\"ljmp\"",
	"\"lds\"",
	"\"les\"",
	"\"lfs\"",
	"\"lgs\"",
	"\"lss\"",
	"\"clts\"",
	"\"hlt\"",
	"\"cli\"",
	"\"sti\"",
	"\"lldt\"",
	"\"lldtl\"",
	"\"lldtd\"",
	"\"sldt\"",
	"\"sldtl\"",
	"\"sldtd\"",
	"\"ltr\"",
	"\"ltrl\"",
	"\"ltrd\"",
	"\"str\"",
	"\"strl\"",
	"\"strd\"",
	"\"inb\"",
	"\"inw\"",
	"\"inl\"",
	"\"outb\"",
	"\"outw\"",
	"\"outl\"",
	"\"invlpg\"",
	"\"iret\"",
	"\"iretl\"",
	"\"iretd\"",
	"\"lret\"",
	"\"cpuid\"",
	"\"wrmsr\"",
	"\"rdmsr\"",
	"\"int\"",
	"\"ud2\"",
	"\"invd\"",
	"\"wbinvd\"",
	"\"smsw\"",
	"\"smswl\"",
	"\"smswd\"",
	"\"lmsw\"",
	"\"lmswl\"",
	"\"lmswd\"",
	"\"arpl\"",
	"\"lar\"",
	"\"lsl\"",
	"\"rsm\"",
	"\"pop\"",
	"\"popl\"",
	"\"popd\"",
	"\"popb\"",
	"\"popw\"",
	"\"push\"",
	"\"pushl\"",
	"\"pushd\"",
	"\"pushb\"",
	"\"pushw\"",
	"\"mov\"",
	"\"movl\"",
	"\"movd\"",
	"\"movb\"",
	"\"movw\"",
	"ASTMacroDef",
	"ASTMacroDefParams",
	"ASTInstruction",
	"ASTInstructionPrefix",
	"ASTSensitive",
	"ASTBasicBlock",
	"ASTCommand",
	"ASTDbgCommand",
	"ASTDefaultParam",
	"ASTRegisterDisplacement",
	"ASTRegisterBaseIndexScale",
	"ASTRegisterIndex",
	"ASTDereference",
	"ASTSegment",
	"ASTRegister",
	"ASTNegative",
	"ASTGprof",
	"ASTGcov",
	"DOT",
	"AT",
	"HASH",
	"LBRACKET",
	"RBRACKET",
	"LCURLY",
	"RCURLY",
	"Whitespace",
	"AsmComment",
	"Comment",
	"CCommentBegin",
	"CCommentEnd",
	"CComment",
	"CPPComment",
	"Letter",
	"Digit",
	"Name",
	"StringEscape",
	"IA32_call",
	"IA32_popf",
	"IA32_pushf",
	"IA32_lgdt",
	"IA32_sgdt",
	"IA32_lidt",
	"IA32_sidt",
	"IA32_ljmp",
	"IA32_lds",
	"IA32_les",
	"IA32_lfs",
	"IA32_lgs",
	"IA32_lss",
	"IA32_clts",
	"IA32_hlt",
	"IA32_cli",
	"IA32_sti",
	"IA32_lldt",
	"IA32_sldt",
	"IA32_ltr",
	"IA32_str",
	"IA32_in",
	"IA32_out",
	"IA32_invlpg",
	"IA32_iret",
	"IA32_lret",
	"IA32_cpuid",
	"IA32_wrmsr",
	"IA32_rdmsr",
	"IA32_int",
	"IA32_ud2",
	"IA32_invd",
	"IA32_wbinvd",
	"IA32_smsw",
	"IA32_lmsw",
	"IA32_arpl",
	"IA32_lar",
	"IA32_lsl",
	"IA32_rsm",
	"IA32_pop",
	"IA32_push",
	"IA32_mov",
	0
};

const unsigned long AsmTreeParser::_tokenSet_0_data_[] = { 128UL, 0UL, 0UL, 0UL, 0UL, 100776448UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// ASSIGN ASTMacroDef ASTInstruction ASTInstructionPrefix ASTSensitive 
// ASTCommand ASTDbgCommand ASTGprof ASTGcov 
const antlr::BitSet AsmTreeParser::_tokenSet_0(_tokenSet_0_data_,12);
const unsigned long AsmTreeParser::_tokenSet_1_data_[] = { 4168097856UL, 127UL, 0UL, 0UL, 0UL, 33292288UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// ID Command Int Hex STAR RelativeLocation MINUS NOT DIV PERCENT SHIFTLEFT 
// SHIFTRIGHT AND OR XOR PLUS DOLLAR ASTRegisterDisplacement ASTRegisterBaseIndexScale 
// ASTRegisterIndex ASTDereference ASTSegment ASTRegister ASTNegative 
const antlr::BitSet AsmTreeParser::_tokenSet_1(_tokenSet_1_data_,12);
const unsigned long AsmTreeParser::_tokenSet_2_data_[] = { 4168884288UL, 127UL, 0UL, 0UL, 0UL, 33423360UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// ID Command String Option Int Hex STAR RelativeLocation MINUS NOT DIV 
// PERCENT SHIFTLEFT SHIFTRIGHT AND OR XOR PLUS DOLLAR ASTDefaultParam 
// ASTRegisterDisplacement ASTRegisterBaseIndexScale ASTRegisterIndex ASTDereference 
// ASTSegment ASTRegister ASTNegative 
const antlr::BitSet AsmTreeParser::_tokenSet_2(_tokenSet_2_data_,12);


ANTLR_END_NAMESPACE

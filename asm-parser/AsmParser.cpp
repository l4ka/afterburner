/* $ANTLR 2.7.6 (20060929): "Asm.g" -> "AsmParser.cpp"$ */
#line 13 "Asm.g"

    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before the antlr generated includes in the cpp file

#line 10 "AsmParser.cpp"
#include "AsmParser.hpp"
#include <antlr/NoViableAltException.hpp>
#include <antlr/SemanticException.hpp>
#include <antlr/ASTFactory.hpp>
#line 19 "Asm.g"

    // Inserted after the antlr generated includes in the cpp file

#line 19 "AsmParser.cpp"
ANTLR_BEGIN_NAMESPACE(Asm)
#line 31 "Asm.g"

// Global stuff in the cpp file.

#line 24 "AsmParser.cpp"
AsmParser::AsmParser(antlr::TokenBuffer& tokenBuf, int k)
: antlr::LLkParser(tokenBuf,k)
{
}

AsmParser::AsmParser(antlr::TokenBuffer& tokenBuf)
: antlr::LLkParser(tokenBuf,2)
{
}

AsmParser::AsmParser(antlr::TokenStream& lexer, int k)
: antlr::LLkParser(lexer,k)
{
}

AsmParser::AsmParser(antlr::TokenStream& lexer)
: antlr::LLkParser(lexer,2)
{
}

AsmParser::AsmParser(const antlr::ParserSharedInputState& state)
: antlr::LLkParser(state,2)
{
}

void AsmParser::asmFile() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmFile_AST = antlr::nullAST;
	
	asmBlocks();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	asmFile_AST = currentAST.root;
	returnAST = asmFile_AST;
}

void AsmParser::asmBlocks() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmBlocks_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case Newline:
	case SEMI:
	case ID:
	case 11:
	case Command:
	case 14:
	case 15:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	case LITERAL_call:
	case LITERAL_popf:
	case LITERAL_popfl:
	case LITERAL_popfd:
	case LITERAL_pushf:
	case LITERAL_pushfl:
	case LITERAL_pushfd:
	case LITERAL_lgdt:
	case LITERAL_lgdtl:
	case LITERAL_lgdtd:
	case LITERAL_sgdt:
	case LITERAL_sgdtl:
	case LITERAL_sgdtd:
	case LITERAL_lidt:
	case LITERAL_lidtl:
	case LITERAL_lidtd:
	case LITERAL_sidt:
	case LITERAL_sidtl:
	case LITERAL_sidtd:
	case LITERAL_ljmp:
	case LITERAL_lds:
	case LITERAL_les:
	case LITERAL_lfs:
	case LITERAL_lgs:
	case LITERAL_lss:
	case LITERAL_clts:
	case LITERAL_hlt:
	case LITERAL_cli:
	case LITERAL_sti:
	case LITERAL_lldt:
	case LITERAL_lldtl:
	case LITERAL_lldtd:
	case LITERAL_sldt:
	case LITERAL_sldtl:
	case LITERAL_sldtd:
	case LITERAL_ltr:
	case LITERAL_ltrl:
	case LITERAL_ltrd:
	case LITERAL_str:
	case LITERAL_strl:
	case LITERAL_strd:
	case LITERAL_inb:
	case LITERAL_inw:
	case LITERAL_inl:
	case LITERAL_outb:
	case LITERAL_outw:
	case LITERAL_outl:
	case LITERAL_invlpg:
	case LITERAL_iret:
	case LITERAL_iretl:
	case LITERAL_iretd:
	case LITERAL_lret:
	case LITERAL_cpuid:
	case LITERAL_wrmsr:
	case LITERAL_rdmsr:
	case LITERAL_int:
	case 141:
	case LITERAL_invd:
	case LITERAL_wbinvd:
	case LITERAL_smsw:
	case LITERAL_smswl:
	case LITERAL_smswd:
	case LITERAL_lmsw:
	case LITERAL_lmswl:
	case LITERAL_lmswd:
	case LITERAL_arpl:
	case LITERAL_lar:
	case LITERAL_lsl:
	case LITERAL_rsm:
	case LITERAL_pop:
	case LITERAL_popl:
	case LITERAL_popd:
	case LITERAL_popb:
	case LITERAL_popw:
	case LITERAL_push:
	case LITERAL_pushl:
	case LITERAL_pushd:
	case LITERAL_pushb:
	case LITERAL_pushw:
	case LITERAL_mov:
	case LITERAL_movl:
	case LITERAL_movd:
	case LITERAL_movb:
	case LITERAL_movw:
	{
		asmAnonymousBlock();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		break;
	}
	case antlr::Token::EOF_TYPE:
	case Label:
	case LocalLabel:
	case 12:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	{ // ( ... )*
	for (;;) {
		if ((LA(1) == Label || LA(1) == LocalLabel)) {
			asmBasicBlock();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
		}
		else {
			goto _loop5;
		}
		
	}
	_loop5:;
	} // ( ... )*
	asmBlocks_AST = currentAST.root;
	returnAST = asmBlocks_AST;
}

void AsmParser::asmAnonymousBlock() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmAnonymousBlock_AST = antlr::nullAST;
	
	{ // ( ... )+
	int _cnt8=0;
	for (;;) {
		if ((_tokenSet_0.member(LA(1)))) {
			asmStatement();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
		}
		else {
			if ( _cnt8>=1 ) { goto _loop8; } else {throw antlr::NoViableAltException(LT(1), getFilename());}
		}
		
		_cnt8++;
	}
	_loop8:;
	}  // ( ... )+
	if ( inputState->guessing==0 ) {
		asmAnonymousBlock_AST = antlr::RefAST(currentAST.root);
#line 51 "Asm.g"
		asmAnonymousBlock_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTBasicBlock,"basic block"))->add(asmAnonymousBlock_AST)));
#line 229 "AsmParser.cpp"
		currentAST.root = asmAnonymousBlock_AST;
		if ( asmAnonymousBlock_AST!=antlr::nullAST &&
			asmAnonymousBlock_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = asmAnonymousBlock_AST->getFirstChild();
		else
			currentAST.child = asmAnonymousBlock_AST;
		currentAST.advanceChildToEnd();
	}
	asmAnonymousBlock_AST = currentAST.root;
	returnAST = asmAnonymousBlock_AST;
}

void AsmParser::asmBasicBlock() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmBasicBlock_AST = antlr::nullAST;
	
	{ // ( ... )+
	int _cnt11=0;
	for (;;) {
		if ((LA(1) == Label || LA(1) == LocalLabel)) {
			asmLabel();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
		}
		else {
			if ( _cnt11>=1 ) { goto _loop11; } else {throw antlr::NoViableAltException(LT(1), getFilename());}
		}
		
		_cnt11++;
	}
	_loop11:;
	}  // ( ... )+
	{ // ( ... )+
	int _cnt13=0;
	for (;;) {
		if ((_tokenSet_0.member(LA(1)))) {
			asmStatement();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
		}
		else {
			if ( _cnt13>=1 ) { goto _loop13; } else {throw antlr::NoViableAltException(LT(1), getFilename());}
		}
		
		_cnt13++;
	}
	_loop13:;
	}  // ( ... )+
	if ( inputState->guessing==0 ) {
		asmBasicBlock_AST = antlr::RefAST(currentAST.root);
#line 55 "Asm.g"
		asmBasicBlock_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTBasicBlock,"basic block"))->add(asmBasicBlock_AST)));
#line 285 "AsmParser.cpp"
		currentAST.root = asmBasicBlock_AST;
		if ( asmBasicBlock_AST!=antlr::nullAST &&
			asmBasicBlock_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = asmBasicBlock_AST->getFirstChild();
		else
			currentAST.child = asmBasicBlock_AST;
		currentAST.advanceChildToEnd();
	}
	asmBasicBlock_AST = currentAST.root;
	returnAST = asmBasicBlock_AST;
}

void AsmParser::asmStatement() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmStatement_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		asmInstrPrefix();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		{
		switch ( LA(1)) {
		case ID:
		case LITERAL_call:
		case LITERAL_popf:
		case LITERAL_popfl:
		case LITERAL_popfd:
		case LITERAL_pushf:
		case LITERAL_pushfl:
		case LITERAL_pushfd:
		case LITERAL_lgdt:
		case LITERAL_lgdtl:
		case LITERAL_lgdtd:
		case LITERAL_sgdt:
		case LITERAL_sgdtl:
		case LITERAL_sgdtd:
		case LITERAL_lidt:
		case LITERAL_lidtl:
		case LITERAL_lidtd:
		case LITERAL_sidt:
		case LITERAL_sidtl:
		case LITERAL_sidtd:
		case LITERAL_ljmp:
		case LITERAL_lds:
		case LITERAL_les:
		case LITERAL_lfs:
		case LITERAL_lgs:
		case LITERAL_lss:
		case LITERAL_clts:
		case LITERAL_hlt:
		case LITERAL_cli:
		case LITERAL_sti:
		case LITERAL_lldt:
		case LITERAL_lldtl:
		case LITERAL_lldtd:
		case LITERAL_sldt:
		case LITERAL_sldtl:
		case LITERAL_sldtd:
		case LITERAL_ltr:
		case LITERAL_ltrl:
		case LITERAL_ltrd:
		case LITERAL_str:
		case LITERAL_strl:
		case LITERAL_strd:
		case LITERAL_inb:
		case LITERAL_inw:
		case LITERAL_inl:
		case LITERAL_outb:
		case LITERAL_outw:
		case LITERAL_outl:
		case LITERAL_invlpg:
		case LITERAL_iret:
		case LITERAL_iretl:
		case LITERAL_iretd:
		case LITERAL_lret:
		case LITERAL_cpuid:
		case LITERAL_wrmsr:
		case LITERAL_rdmsr:
		case LITERAL_int:
		case 141:
		case LITERAL_invd:
		case LITERAL_wbinvd:
		case LITERAL_smsw:
		case LITERAL_smswl:
		case LITERAL_smswd:
		case LITERAL_lmsw:
		case LITERAL_lmswl:
		case LITERAL_lmswd:
		case LITERAL_arpl:
		case LITERAL_lar:
		case LITERAL_lsl:
		case LITERAL_rsm:
		case LITERAL_pop:
		case LITERAL_popl:
		case LITERAL_popd:
		case LITERAL_popb:
		case LITERAL_popw:
		case LITERAL_push:
		case LITERAL_pushl:
		case LITERAL_pushd:
		case LITERAL_pushb:
		case LITERAL_pushw:
		case LITERAL_mov:
		case LITERAL_movl:
		case LITERAL_movd:
		case LITERAL_movb:
		case LITERAL_movw:
		{
			asmInstr();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		case Newline:
		case SEMI:
		{
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		asmEnd();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmStatement_AST = currentAST.root;
		break;
	}
	case 11:
	case Command:
	case 14:
	case 15:
	{
		asmCommand();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmEnd();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmStatement_AST = currentAST.root;
		break;
	}
	case Newline:
	case SEMI:
	{
		asmEnd();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmStatement_AST = currentAST.root;
		break;
	}
	default:
		if ((_tokenSet_1.member(LA(1))) && (_tokenSet_2.member(LA(2)))) {
			asmInstr();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			asmEnd();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			asmStatement_AST = currentAST.root;
		}
		else if ((LA(1) == ID) && (LA(2) == ASSIGN)) {
			asmAssignment();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			asmEnd();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			asmStatement_AST = currentAST.root;
		}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmStatement_AST;
}

void AsmParser::asmLabel() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmLabel_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case Label:
	{
		antlr::RefAST tmp67_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp67_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp67_AST);
		}
		match(Label);
		asmLabel_AST = currentAST.root;
		break;
	}
	case LocalLabel:
	{
		antlr::RefAST tmp68_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp68_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp68_AST);
		}
		match(LocalLabel);
		asmLabel_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmLabel_AST;
}

void AsmParser::asmEnd() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmEnd_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case Newline:
	{
		match(Newline);
		asmEnd_AST = currentAST.root;
		break;
	}
	case SEMI:
	{
		match(SEMI);
		asmEnd_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmEnd_AST;
}

void AsmParser::asmInstrPrefix() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmInstrPrefix_AST = antlr::nullAST;
	
	asmInstrPrefixTokens();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	if ( inputState->guessing==0 ) {
		asmInstrPrefix_AST = antlr::RefAST(currentAST.root);
#line 224 "Asm.g"
		asmInstrPrefix_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTInstructionPrefix,"prefix"))->add(asmInstrPrefix_AST)));
#line 555 "AsmParser.cpp"
		currentAST.root = asmInstrPrefix_AST;
		if ( asmInstrPrefix_AST!=antlr::nullAST &&
			asmInstrPrefix_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = asmInstrPrefix_AST->getFirstChild();
		else
			currentAST.child = asmInstrPrefix_AST;
		currentAST.advanceChildToEnd();
	}
	asmInstrPrefix_AST = currentAST.root;
	returnAST = asmInstrPrefix_AST;
}

void AsmParser::asmInstr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmInstr_AST = antlr::nullAST;
#line 93 "Asm.g"
	bool sensitive=false;
#line 574 "AsmParser.cpp"
	
	if ((LA(1) == LITERAL_call) && (LA(2) == LITERAL_mcount || LA(2) == LITERAL___gcov_merge_add)) {
		asmGprof();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmInstr_AST = currentAST.root;
	}
	else if ((_tokenSet_1.member(LA(1))) && (_tokenSet_3.member(LA(2)))) {
		{
		switch ( LA(1)) {
		case ID:
		case LITERAL_call:
		case LITERAL_pop:
		case LITERAL_popl:
		case LITERAL_popd:
		case LITERAL_popb:
		case LITERAL_popw:
		case LITERAL_push:
		case LITERAL_pushl:
		case LITERAL_pushd:
		case LITERAL_pushb:
		case LITERAL_pushw:
		case LITERAL_mov:
		case LITERAL_movl:
		case LITERAL_movd:
		case LITERAL_movb:
		case LITERAL_movw:
		{
			asmInnocuousInstr();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			{
			switch ( LA(1)) {
			case ID:
			case Command:
			case Int:
			case Hex:
			case STAR:
			case LPAREN:
			case Reg:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DOLLAR:
			case 39:
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
			case 58:
			case 59:
			case 60:
			case 61:
			case 62:
			case 63:
			case 64:
			case 65:
			case 66:
			case 67:
			case 68:
			case 69:
			case 70:
			case 71:
			case 72:
			case 73:
			case 74:
			case 75:
			case 76:
			case 77:
			case 78:
			case 79:
			case 80:
			case 81:
			case LITERAL_lock:
			case LITERAL_rep:
			case LITERAL_repnz:
			{
				sensitive=instrParams();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				break;
			}
			case Newline:
			case SEMI:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			break;
		}
		case LITERAL_popf:
		case LITERAL_popfl:
		case LITERAL_popfd:
		case LITERAL_pushf:
		case LITERAL_pushfl:
		case LITERAL_pushfd:
		case LITERAL_lgdt:
		case LITERAL_lgdtl:
		case LITERAL_lgdtd:
		case LITERAL_sgdt:
		case LITERAL_sgdtl:
		case LITERAL_sgdtd:
		case LITERAL_lidt:
		case LITERAL_lidtl:
		case LITERAL_lidtd:
		case LITERAL_sidt:
		case LITERAL_sidtl:
		case LITERAL_sidtd:
		case LITERAL_ljmp:
		case LITERAL_lds:
		case LITERAL_les:
		case LITERAL_lfs:
		case LITERAL_lgs:
		case LITERAL_lss:
		case LITERAL_clts:
		case LITERAL_hlt:
		case LITERAL_cli:
		case LITERAL_sti:
		case LITERAL_lldt:
		case LITERAL_lldtl:
		case LITERAL_lldtd:
		case LITERAL_sldt:
		case LITERAL_sldtl:
		case LITERAL_sldtd:
		case LITERAL_ltr:
		case LITERAL_ltrl:
		case LITERAL_ltrd:
		case LITERAL_str:
		case LITERAL_strl:
		case LITERAL_strd:
		case LITERAL_inb:
		case LITERAL_inw:
		case LITERAL_inl:
		case LITERAL_outb:
		case LITERAL_outw:
		case LITERAL_outl:
		case LITERAL_invlpg:
		case LITERAL_iret:
		case LITERAL_iretl:
		case LITERAL_iretd:
		case LITERAL_lret:
		case LITERAL_cpuid:
		case LITERAL_wrmsr:
		case LITERAL_rdmsr:
		case LITERAL_int:
		case 141:
		case LITERAL_invd:
		case LITERAL_wbinvd:
		case LITERAL_smsw:
		case LITERAL_smswl:
		case LITERAL_smswd:
		case LITERAL_lmsw:
		case LITERAL_lmswl:
		case LITERAL_lmswd:
		case LITERAL_arpl:
		case LITERAL_lar:
		case LITERAL_lsl:
		case LITERAL_rsm:
		{
			asmSensitiveInstr();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			{
			switch ( LA(1)) {
			case ID:
			case Command:
			case Int:
			case Hex:
			case STAR:
			case LPAREN:
			case Reg:
			case RelativeLocation:
			case MINUS:
			case NOT:
			case DOLLAR:
			case 39:
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
			case 58:
			case 59:
			case 60:
			case 61:
			case 62:
			case 63:
			case 64:
			case 65:
			case 66:
			case 67:
			case 68:
			case 69:
			case 70:
			case 71:
			case 72:
			case 73:
			case 74:
			case 75:
			case 76:
			case 77:
			case 78:
			case 79:
			case 80:
			case 81:
			case LITERAL_lock:
			case LITERAL_rep:
			case LITERAL_repnz:
			{
				instrParams();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				break;
			}
			case Newline:
			case SEMI:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			if ( inputState->guessing==0 ) {
#line 96 "Asm.g"
				sensitive=true;
#line 840 "AsmParser.cpp"
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		if ( inputState->guessing==0 ) {
			asmInstr_AST = antlr::RefAST(currentAST.root);
#line 98 "Asm.g"
			
			if( sensitive )
				    asmInstr_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTSensitive,"sensitive instruction"))->add(asmInstr_AST)));
			else
				    asmInstr_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTInstruction,"instruction"))->add(asmInstr_AST)));
			
#line 859 "AsmParser.cpp"
			currentAST.root = asmInstr_AST;
			if ( asmInstr_AST!=antlr::nullAST &&
				asmInstr_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmInstr_AST->getFirstChild();
			else
				currentAST.child = asmInstr_AST;
			currentAST.advanceChildToEnd();
		}
		asmInstr_AST = currentAST.root;
	}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	
	returnAST = asmInstr_AST;
}

void AsmParser::asmCommand() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmCommand_AST = antlr::nullAST;
	antlr::RefToken  f = antlr::nullToken;
	antlr::RefAST f_AST = antlr::nullAST;
	antlr::RefToken  l = antlr::nullToken;
	antlr::RefAST l_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case 11:
	{
		asmMacroDef();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmCommand_AST = currentAST.root;
		break;
	}
	case Command:
	{
		antlr::RefAST tmp71_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp71_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp71_AST);
		}
		match(Command);
		commandParams();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		if ( inputState->guessing==0 ) {
			asmCommand_AST = antlr::RefAST(currentAST.root);
#line 85 "Asm.g"
			asmCommand_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTCommand,"command"))->add(asmCommand_AST)));
#line 912 "AsmParser.cpp"
			currentAST.root = asmCommand_AST;
			if ( asmCommand_AST!=antlr::nullAST &&
				asmCommand_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmCommand_AST->getFirstChild();
			else
				currentAST.child = asmCommand_AST;
			currentAST.advanceChildToEnd();
		}
		asmCommand_AST = currentAST.root;
		break;
	}
	case 14:
	{
		f = LT(1);
		if ( inputState->guessing == 0 ) {
			f_AST = astFactory->create(f);
			astFactory->addASTChild(currentAST, f_AST);
		}
		match(14);
		simpleParams();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		if ( inputState->guessing==0 ) {
			asmCommand_AST = antlr::RefAST(currentAST.root);
#line 87 "Asm.g"
			f_AST->setType(Command); 
							  asmCommand_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTDbgCommand,"command"))->add(asmCommand_AST)));
#line 941 "AsmParser.cpp"
			currentAST.root = asmCommand_AST;
			if ( asmCommand_AST!=antlr::nullAST &&
				asmCommand_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmCommand_AST->getFirstChild();
			else
				currentAST.child = asmCommand_AST;
			currentAST.advanceChildToEnd();
		}
		asmCommand_AST = currentAST.root;
		break;
	}
	case 15:
	{
		l = LT(1);
		if ( inputState->guessing == 0 ) {
			l_AST = astFactory->create(l);
			astFactory->addASTChild(currentAST, l_AST);
		}
		match(15);
		simpleParams();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		if ( inputState->guessing==0 ) {
			asmCommand_AST = antlr::RefAST(currentAST.root);
#line 89 "Asm.g"
			l_AST->setType(Command);
							  asmCommand_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTDbgCommand,"command"))->add(asmCommand_AST)));
#line 970 "AsmParser.cpp"
			currentAST.root = asmCommand_AST;
			if ( asmCommand_AST!=antlr::nullAST &&
				asmCommand_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmCommand_AST->getFirstChild();
			else
				currentAST.child = asmCommand_AST;
			currentAST.advanceChildToEnd();
		}
		asmCommand_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmCommand_AST;
}

void AsmParser::asmAssignment() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmAssignment_AST = antlr::nullAST;
	
	antlr::RefAST tmp72_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp72_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp72_AST);
	}
	match(ID);
	antlr::RefAST tmp73_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp73_AST = astFactory->create(LT(1));
		astFactory->makeASTRoot(currentAST, tmp73_AST);
	}
	match(ASSIGN);
	commandParam();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	asmAssignment_AST = currentAST.root;
	returnAST = asmAssignment_AST;
}

void AsmParser::commandParam() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST commandParam_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case String:
	{
		antlr::RefAST tmp74_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp74_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp74_AST);
		}
		match(String);
		commandParam_AST = currentAST.root;
		break;
	}
	case Option:
	{
		antlr::RefAST tmp75_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp75_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp75_AST);
		}
		match(Option);
		commandParam_AST = currentAST.root;
		break;
	}
	case ID:
	case Command:
	case Int:
	case Hex:
	case STAR:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case MINUS:
	case NOT:
	case DOLLAR:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		instrParam();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		commandParam_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = commandParam_AST;
}

void AsmParser::asmMacroDefParams() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmMacroDefParams_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case ID:
	{
		antlr::RefAST tmp76_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp76_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp76_AST);
		}
		match(ID);
		{ // ( ... )*
		for (;;) {
			if ((LA(1) == COMMA)) {
				match(COMMA);
				antlr::RefAST tmp78_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp78_AST = astFactory->create(LT(1));
					astFactory->addASTChild(currentAST, tmp78_AST);
				}
				match(ID);
			}
			else {
				goto _loop22;
			}
			
		}
		_loop22:;
		} // ( ... )*
		break;
	}
	case Newline:
	case SEMI:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		asmMacroDefParams_AST = antlr::RefAST(currentAST.root);
#line 73 "Asm.g"
		asmMacroDefParams_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTMacroDefParams,"macro parameters"))->add(asmMacroDefParams_AST)));
#line 1166 "AsmParser.cpp"
		currentAST.root = asmMacroDefParams_AST;
		if ( asmMacroDefParams_AST!=antlr::nullAST &&
			asmMacroDefParams_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = asmMacroDefParams_AST->getFirstChild();
		else
			currentAST.child = asmMacroDefParams_AST;
		currentAST.advanceChildToEnd();
	}
	asmMacroDefParams_AST = currentAST.root;
	returnAST = asmMacroDefParams_AST;
}

void AsmParser::asmMacroDef() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmMacroDef_AST = antlr::nullAST;
	
	match(11);
	antlr::RefAST tmp80_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp80_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp80_AST);
	}
	match(ID);
	asmMacroDefParams();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	asmEnd();
	asmBlocks();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	match(12);
	if ( inputState->guessing==0 ) {
		asmMacroDef_AST = antlr::RefAST(currentAST.root);
#line 80 "Asm.g"
		asmMacroDef_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTMacroDef,"macro definition"))->add(asmMacroDef_AST)));
#line 1205 "AsmParser.cpp"
		currentAST.root = asmMacroDef_AST;
		if ( asmMacroDef_AST!=antlr::nullAST &&
			asmMacroDef_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = asmMacroDef_AST->getFirstChild();
		else
			currentAST.child = asmMacroDef_AST;
		currentAST.advanceChildToEnd();
	}
	asmMacroDef_AST = currentAST.root;
	returnAST = asmMacroDef_AST;
}

void AsmParser::commandParams() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST commandParams_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case ID:
	case Command:
	case String:
	case Option:
	case Int:
	case Hex:
	case STAR:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case MINUS:
	case NOT:
	case DOLLAR:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		commandParam();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		{ // ( ... )*
		for (;;) {
			if ((LA(1) == COMMA)) {
				match(COMMA);
				{
				switch ( LA(1)) {
				case ID:
				case Command:
				case String:
				case Option:
				case Int:
				case Hex:
				case STAR:
				case LPAREN:
				case Reg:
				case RelativeLocation:
				case MINUS:
				case NOT:
				case DOLLAR:
				case 39:
				case 40:
				case 41:
				case 42:
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
				case 48:
				case 49:
				case 50:
				case 51:
				case 52:
				case 53:
				case 54:
				case 55:
				case 56:
				case 57:
				case 58:
				case 59:
				case 60:
				case 61:
				case 62:
				case 63:
				case 64:
				case 65:
				case 66:
				case 67:
				case 68:
				case 69:
				case 70:
				case 71:
				case 72:
				case 73:
				case 74:
				case 75:
				case 76:
				case 77:
				case 78:
				case 79:
				case 80:
				case 81:
				case LITERAL_lock:
				case LITERAL_rep:
				case LITERAL_repnz:
				{
					commandParam();
					if (inputState->guessing==0) {
						astFactory->addASTChild( currentAST, returnAST );
					}
					break;
				}
				case Newline:
				case SEMI:
				case COMMA:
				{
					defaultParam();
					if (inputState->guessing==0) {
						astFactory->addASTChild( currentAST, returnAST );
					}
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
			}
			else {
				goto _loop37;
			}
			
		}
		_loop37:;
		} // ( ... )*
		commandParams_AST = currentAST.root;
		break;
	}
	case COMMA:
	{
		defaultParam();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		{ // ( ... )+
		int _cnt40=0;
		for (;;) {
			if ((LA(1) == COMMA)) {
				match(COMMA);
				{
				switch ( LA(1)) {
				case ID:
				case Command:
				case String:
				case Option:
				case Int:
				case Hex:
				case STAR:
				case LPAREN:
				case Reg:
				case RelativeLocation:
				case MINUS:
				case NOT:
				case DOLLAR:
				case 39:
				case 40:
				case 41:
				case 42:
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
				case 48:
				case 49:
				case 50:
				case 51:
				case 52:
				case 53:
				case 54:
				case 55:
				case 56:
				case 57:
				case 58:
				case 59:
				case 60:
				case 61:
				case 62:
				case 63:
				case 64:
				case 65:
				case 66:
				case 67:
				case 68:
				case 69:
				case 70:
				case 71:
				case 72:
				case 73:
				case 74:
				case 75:
				case 76:
				case 77:
				case 78:
				case 79:
				case 80:
				case 81:
				case LITERAL_lock:
				case LITERAL_rep:
				case LITERAL_repnz:
				{
					commandParam();
					if (inputState->guessing==0) {
						astFactory->addASTChild( currentAST, returnAST );
					}
					break;
				}
				case Newline:
				case SEMI:
				case COMMA:
				{
					defaultParam();
					if (inputState->guessing==0) {
						astFactory->addASTChild( currentAST, returnAST );
					}
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
			}
			else {
				if ( _cnt40>=1 ) { goto _loop40; } else {throw antlr::NoViableAltException(LT(1), getFilename());}
			}
			
			_cnt40++;
		}
		_loop40:;
		}  // ( ... )+
		commandParams_AST = currentAST.root;
		break;
	}
	case Newline:
	case SEMI:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = commandParams_AST;
}

void AsmParser::simpleParams() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST simpleParams_AST = antlr::nullAST;
	
	{ // ( ... )*
	for (;;) {
		if ((_tokenSet_4.member(LA(1)))) {
			simpleParam();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
		}
		else {
			goto _loop32;
		}
		
	}
	_loop32:;
	} // ( ... )*
	simpleParams_AST = currentAST.root;
	returnAST = simpleParams_AST;
}

void AsmParser::asmGprof() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmGprof_AST = antlr::nullAST;
	antlr::RefToken  m = antlr::nullToken;
	antlr::RefAST m_AST = antlr::nullAST;
	antlr::RefToken  a = antlr::nullToken;
	antlr::RefAST a_AST = antlr::nullAST;
	
	if ((LA(1) == LITERAL_call) && (LA(2) == LITERAL_mcount)) {
		ia32_call();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		m = LT(1);
		if ( inputState->guessing == 0 ) {
			m_AST = astFactory->create(m);
			astFactory->addASTChild(currentAST, m_AST);
		}
		match(LITERAL_mcount);
		if ( inputState->guessing==0 ) {
			asmGprof_AST = antlr::RefAST(currentAST.root);
#line 107 "Asm.g"
			asmGprof_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTGprof,"gprof"))->add(asmGprof_AST)));
#line 1555 "AsmParser.cpp"
			currentAST.root = asmGprof_AST;
			if ( asmGprof_AST!=antlr::nullAST &&
				asmGprof_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmGprof_AST->getFirstChild();
			else
				currentAST.child = asmGprof_AST;
			currentAST.advanceChildToEnd();
		}
		asmGprof_AST = currentAST.root;
	}
	else if ((LA(1) == LITERAL_call) && (LA(2) == LITERAL___gcov_merge_add)) {
		ia32_call();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		a = LT(1);
		if ( inputState->guessing == 0 ) {
			a_AST = astFactory->create(a);
			astFactory->addASTChild(currentAST, a_AST);
		}
		match(LITERAL___gcov_merge_add);
		if ( inputState->guessing==0 ) {
			asmGprof_AST = antlr::RefAST(currentAST.root);
#line 108 "Asm.g"
			asmGprof_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTGcov,"gprof"))->add(asmGprof_AST)));
#line 1581 "AsmParser.cpp"
			currentAST.root = asmGprof_AST;
			if ( asmGprof_AST!=antlr::nullAST &&
				asmGprof_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = asmGprof_AST->getFirstChild();
			else
				currentAST.child = asmGprof_AST;
			currentAST.advanceChildToEnd();
		}
		asmGprof_AST = currentAST.root;
	}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	
	returnAST = asmGprof_AST;
}

void AsmParser::asmInnocuousInstr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmInnocuousInstr_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case ID:
	{
		antlr::RefAST tmp84_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp84_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp84_AST);
		}
		match(ID);
		asmInnocuousInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_pop:
	case LITERAL_popl:
	case LITERAL_popd:
	case LITERAL_popb:
	case LITERAL_popw:
	{
		ia32_pop();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmInnocuousInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_push:
	case LITERAL_pushl:
	case LITERAL_pushd:
	case LITERAL_pushb:
	case LITERAL_pushw:
	{
		ia32_push();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmInnocuousInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_mov:
	case LITERAL_movl:
	case LITERAL_movd:
	case LITERAL_movb:
	case LITERAL_movw:
	{
		ia32_mov();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmInnocuousInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_call:
	{
		ia32_call();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmInnocuousInstr_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmInnocuousInstr_AST;
}

bool  AsmParser::instrParams() {
#line 125 "Asm.g"
	bool s;
#line 1675 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST instrParams_AST = antlr::nullAST;
#line 125 "Asm.g"
	s=false; bool t;
#line 1681 "AsmParser.cpp"
	
	s=instrParam();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{ // ( ... )*
	for (;;) {
		if ((LA(1) == COMMA)) {
			match(COMMA);
			t=instrParam();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			if ( inputState->guessing==0 ) {
#line 126 "Asm.g"
				s|=t;
#line 1698 "AsmParser.cpp"
			}
		}
		else {
			goto _loop45;
		}
		
	}
	_loop45:;
	} // ( ... )*
	instrParams_AST = currentAST.root;
	returnAST = instrParams_AST;
	return s;
}

void AsmParser::asmSensitiveInstr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmSensitiveInstr_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case LITERAL_popf:
	case LITERAL_popfl:
	case LITERAL_popfd:
	{
		ia32_popf();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_pushf:
	case LITERAL_pushfl:
	case LITERAL_pushfd:
	{
		ia32_pushf();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lgdt:
	case LITERAL_lgdtl:
	case LITERAL_lgdtd:
	{
		ia32_lgdt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_sgdt:
	case LITERAL_sgdtl:
	case LITERAL_sgdtd:
	{
		ia32_sgdt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lidt:
	case LITERAL_lidtl:
	case LITERAL_lidtd:
	{
		ia32_lidt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_sidt:
	case LITERAL_sidtl:
	case LITERAL_sidtd:
	{
		ia32_sidt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_ljmp:
	{
		ia32_ljmp();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lds:
	{
		ia32_lds();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_les:
	{
		ia32_les();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lfs:
	{
		ia32_lfs();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lgs:
	{
		ia32_lgs();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lss:
	{
		ia32_lss();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_clts:
	{
		ia32_clts();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_hlt:
	{
		ia32_hlt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_cli:
	{
		ia32_cli();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_sti:
	{
		ia32_sti();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lldt:
	case LITERAL_lldtl:
	case LITERAL_lldtd:
	{
		ia32_lldt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_sldt:
	case LITERAL_sldtl:
	case LITERAL_sldtd:
	{
		ia32_sldt();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_ltr:
	case LITERAL_ltrl:
	case LITERAL_ltrd:
	{
		ia32_ltr();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_str:
	case LITERAL_strl:
	case LITERAL_strd:
	{
		ia32_str();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_inb:
	case LITERAL_inw:
	case LITERAL_inl:
	{
		ia32_in();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_outb:
	case LITERAL_outw:
	case LITERAL_outl:
	{
		ia32_out();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_invlpg:
	{
		ia32_invlpg();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_iret:
	case LITERAL_iretl:
	case LITERAL_iretd:
	{
		ia32_iret();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lret:
	{
		ia32_lret();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_cpuid:
	{
		ia32_cpuid();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_wrmsr:
	{
		ia32_wrmsr();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_rdmsr:
	{
		ia32_rdmsr();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_int:
	{
		ia32_int();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case 141:
	{
		ia32_ud2();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_invd:
	{
		ia32_invd();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_wbinvd:
	{
		ia32_wbinvd();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_smsw:
	case LITERAL_smswl:
	case LITERAL_smswd:
	{
		ia32_smsw();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lmsw:
	case LITERAL_lmswl:
	case LITERAL_lmswd:
	{
		ia32_lmsw();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_arpl:
	{
		ia32_arpl();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lar:
	{
		ia32_lar();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_lsl:
	{
		ia32_lsl();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	case LITERAL_rsm:
	{
		ia32_rsm();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		asmSensitiveInstr_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = asmSensitiveInstr_AST;
}

void AsmParser::ia32_call() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_call_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp86_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp86_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp86_AST);
	}
	match(LITERAL_call);
	}
	if ( inputState->guessing==0 ) {
		ia32_call_AST = antlr::RefAST(currentAST.root);
#line 231 "Asm.g"
		ia32_call_AST->setType(IA32_call);
#line 2116 "AsmParser.cpp"
	}
	ia32_call_AST = currentAST.root;
	returnAST = ia32_call_AST;
}

void AsmParser::simpleParam() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST simpleParam_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case String:
	{
		antlr::RefAST tmp87_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp87_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp87_AST);
		}
		match(String);
		simpleParam_AST = currentAST.root;
		break;
	}
	case Option:
	{
		antlr::RefAST tmp88_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp88_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp88_AST);
		}
		match(Option);
		simpleParam_AST = currentAST.root;
		break;
	}
	case Int:
	{
		antlr::RefAST tmp89_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp89_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp89_AST);
		}
		match(Int);
		simpleParam_AST = currentAST.root;
		break;
	}
	case Hex:
	{
		antlr::RefAST tmp90_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp90_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp90_AST);
		}
		match(Hex);
		simpleParam_AST = currentAST.root;
		break;
	}
	case Command:
	{
		antlr::RefAST tmp91_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp91_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp91_AST);
		}
		match(Command);
		simpleParam_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = simpleParam_AST;
}

void AsmParser::defaultParam() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST defaultParam_AST = antlr::nullAST;
	
	if ( inputState->guessing==0 ) {
		defaultParam_AST = antlr::RefAST(currentAST.root);
#line 122 "Asm.g"
		defaultParam_AST = astFactory->create(ASTDefaultParam,"default");
#line 2200 "AsmParser.cpp"
		currentAST.root = defaultParam_AST;
		if ( defaultParam_AST!=antlr::nullAST &&
			defaultParam_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = defaultParam_AST->getFirstChild();
		else
			currentAST.child = defaultParam_AST;
		currentAST.advanceChildToEnd();
	}
	defaultParam_AST = currentAST.root;
	returnAST = defaultParam_AST;
}

bool  AsmParser::instrParam() {
#line 127 "Asm.g"
	bool s;
#line 2216 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST instrParam_AST = antlr::nullAST;
#line 127 "Asm.g"
	s=false;
#line 2222 "AsmParser.cpp"
	
	s=regExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	instrParam_AST = currentAST.root;
	returnAST = instrParam_AST;
	return s;
}

bool  AsmParser::regExpression() {
#line 129 "Asm.g"
	bool s;
#line 2236 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST regExpression_AST = antlr::nullAST;
#line 129 "Asm.g"
	s=false;
#line 2242 "AsmParser.cpp"
	
	s=regDereferenceExpr();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	regExpression_AST = currentAST.root;
	returnAST = regExpression_AST;
	return s;
}

bool  AsmParser::regDereferenceExpr() {
#line 131 "Asm.g"
	bool sensitive;
#line 2256 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST regDereferenceExpr_AST = antlr::nullAST;
	antlr::RefToken  s = antlr::nullToken;
	antlr::RefAST s_AST = antlr::nullAST;
#line 131 "Asm.g"
	sensitive=false;
#line 2264 "AsmParser.cpp"
	
	{
	switch ( LA(1)) {
	case STAR:
	{
		s = LT(1);
		if ( inputState->guessing == 0 ) {
			s_AST = astFactory->create(s);
			astFactory->makeASTRoot(currentAST, s_AST);
		}
		match(STAR);
		if ( inputState->guessing==0 ) {
#line 132 "Asm.g"
			s_AST->setType(ASTDereference);
#line 2279 "AsmParser.cpp"
		}
		break;
	}
	case ID:
	case Command:
	case Int:
	case Hex:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case MINUS:
	case NOT:
	case DOLLAR:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	sensitive=regSegmentExpr();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	regDereferenceExpr_AST = currentAST.root;
	returnAST = regDereferenceExpr_AST;
	return sensitive;
}

bool  AsmParser::regSegmentExpr() {
#line 135 "Asm.g"
	bool s;
#line 2360 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST regSegmentExpr_AST = antlr::nullAST;
	antlr::RefToken  c = antlr::nullToken;
	antlr::RefAST c_AST = antlr::nullAST;
#line 135 "Asm.g"
	s=false;
#line 2368 "AsmParser.cpp"
	
	{
	if (((LA(1) >= 64 && LA(1) <= 69)) && (LA(2) == COLON)) {
		asmSegReg();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		c = LT(1);
		if ( inputState->guessing == 0 ) {
			c_AST = astFactory->create(c);
			astFactory->makeASTRoot(currentAST, c_AST);
		}
		match(COLON);
		if ( inputState->guessing==0 ) {
#line 136 "Asm.g"
			c_AST->setType(ASTSegment);
#line 2385 "AsmParser.cpp"
		}
	}
	else if ((_tokenSet_5.member(LA(1))) && (_tokenSet_6.member(LA(2)))) {
	}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	
	}
	s=regDisplacementExpr();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	regSegmentExpr_AST = currentAST.root;
	returnAST = regSegmentExpr_AST;
	return s;
}

void AsmParser::asmSegReg() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmSegReg_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case 64:
	{
		antlr::RefAST tmp92_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp92_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp92_AST);
		}
		match(64);
		break;
	}
	case 65:
	{
		antlr::RefAST tmp93_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp93_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp93_AST);
		}
		match(65);
		break;
	}
	case 66:
	{
		antlr::RefAST tmp94_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp94_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp94_AST);
		}
		match(66);
		break;
	}
	case 67:
	{
		antlr::RefAST tmp95_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp95_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp95_AST);
		}
		match(67);
		break;
	}
	case 68:
	{
		antlr::RefAST tmp96_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp96_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp96_AST);
		}
		match(68);
		break;
	}
	case 69:
	{
		antlr::RefAST tmp97_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp97_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp97_AST);
		}
		match(69);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		asmSegReg_AST = antlr::RefAST(currentAST.root);
#line 209 "Asm.g"
		asmSegReg_AST->setType(ASTRegister);
#line 2481 "AsmParser.cpp"
	}
	asmSegReg_AST = currentAST.root;
	returnAST = asmSegReg_AST;
}

bool  AsmParser::regDisplacementExpr() {
#line 140 "Asm.g"
	bool s;
#line 2490 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST regDisplacementExpr_AST = antlr::nullAST;
#line 140 "Asm.g"
	s=false;
#line 2496 "AsmParser.cpp"
	
	s=expression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{
	switch ( LA(1)) {
	case LPAREN:
	{
		regOffsetBase();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		if ( inputState->guessing==0 ) {
			regDisplacementExpr_AST = antlr::RefAST(currentAST.root);
#line 145 "Asm.g"
			regDisplacementExpr_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTRegisterDisplacement,"register displacement"))->add(regDisplacementExpr_AST)));
#line 2514 "AsmParser.cpp"
			currentAST.root = regDisplacementExpr_AST;
			if ( regDisplacementExpr_AST!=antlr::nullAST &&
				regDisplacementExpr_AST->getFirstChild() != antlr::nullAST )
				  currentAST.child = regDisplacementExpr_AST->getFirstChild();
			else
				currentAST.child = regDisplacementExpr_AST;
			currentAST.advanceChildToEnd();
		}
		break;
	}
	case Newline:
	case SEMI:
	case COMMA:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	regDisplacementExpr_AST = currentAST.root;
	returnAST = regDisplacementExpr_AST;
	return s;
}

bool  AsmParser::expression() {
#line 196 "Asm.g"
	bool s;
#line 2545 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST expression_AST = antlr::nullAST;
#line 196 "Asm.g"
	s=false;
#line 2551 "AsmParser.cpp"
	
	s=makeConstantExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	expression_AST = currentAST.root;
	returnAST = expression_AST;
	return s;
}

void AsmParser::regOffsetBase() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST regOffsetBase_AST = antlr::nullAST;
	
	{
	if ((LA(1) == LPAREN) && (LA(2) == COMMA)) {
		match(LPAREN);
		defaultParam();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		match(COMMA);
		{
		switch ( LA(1)) {
		case 39:
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		case 48:
		case 49:
		case 50:
		case 51:
		case 52:
		case 53:
		case 54:
		case 55:
		case 56:
		case 57:
		case 58:
		case 59:
		case 60:
		case 61:
		case 62:
		{
			asmReg();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		case COMMA:
		{
			defaultParam();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		match(COMMA);
		{
		switch ( LA(1)) {
		case Int:
		{
			antlr::RefAST tmp101_AST = antlr::nullAST;
			if ( inputState->guessing == 0 ) {
				tmp101_AST = astFactory->create(LT(1));
				astFactory->addASTChild(currentAST, tmp101_AST);
			}
			match(Int);
			break;
		}
		case RPAREN:
		{
			defaultParam();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		match(RPAREN);
	}
	else if ((LA(1) == LPAREN) && ((LA(2) >= 39 && LA(2) <= 62))) {
		match(LPAREN);
		asmReg();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		{
		switch ( LA(1)) {
		case COMMA:
		{
			match(COMMA);
			{
			switch ( LA(1)) {
			case 39:
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
			case 48:
			case 49:
			case 50:
			case 51:
			case 52:
			case 53:
			case 54:
			case 55:
			case 56:
			case 57:
			case 58:
			case 59:
			case 60:
			case 61:
			case 62:
			{
				asmReg();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				break;
			}
			case COMMA:
			case RPAREN:
			{
				defaultParam();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			{
			switch ( LA(1)) {
			case COMMA:
			{
				match(COMMA);
				{
				switch ( LA(1)) {
				case Int:
				{
					antlr::RefAST tmp106_AST = antlr::nullAST;
					if ( inputState->guessing == 0 ) {
						tmp106_AST = astFactory->create(LT(1));
						astFactory->addASTChild(currentAST, tmp106_AST);
					}
					match(Int);
					break;
				}
				case RPAREN:
				{
					defaultParam();
					if (inputState->guessing==0) {
						astFactory->addASTChild( currentAST, returnAST );
					}
					break;
				}
				default:
				{
					throw antlr::NoViableAltException(LT(1), getFilename());
				}
				}
				}
				break;
			}
			case RPAREN:
			{
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			break;
		}
		case RPAREN:
		{
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		match(RPAREN);
	}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	
	}
	if ( inputState->guessing==0 ) {
		regOffsetBase_AST = antlr::RefAST(currentAST.root);
#line 156 "Asm.g"
		regOffsetBase_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTRegisterBaseIndexScale,"register base index scale"))->add(regOffsetBase_AST)));
#line 2776 "AsmParser.cpp"
		currentAST.root = regOffsetBase_AST;
		if ( regOffsetBase_AST!=antlr::nullAST &&
			regOffsetBase_AST->getFirstChild() != antlr::nullAST )
			  currentAST.child = regOffsetBase_AST->getFirstChild();
		else
			currentAST.child = regOffsetBase_AST;
		currentAST.advanceChildToEnd();
	}
	regOffsetBase_AST = currentAST.root;
	returnAST = regOffsetBase_AST;
}

void AsmParser::asmReg() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmReg_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case 39:
	{
		antlr::RefAST tmp108_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp108_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp108_AST);
		}
		match(39);
		break;
	}
	case 40:
	{
		antlr::RefAST tmp109_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp109_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp109_AST);
		}
		match(40);
		break;
	}
	case 41:
	{
		antlr::RefAST tmp110_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp110_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp110_AST);
		}
		match(41);
		break;
	}
	case 42:
	{
		antlr::RefAST tmp111_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp111_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp111_AST);
		}
		match(42);
		break;
	}
	case 43:
	{
		antlr::RefAST tmp112_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp112_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp112_AST);
		}
		match(43);
		break;
	}
	case 44:
	{
		antlr::RefAST tmp113_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp113_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp113_AST);
		}
		match(44);
		break;
	}
	case 45:
	{
		antlr::RefAST tmp114_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp114_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp114_AST);
		}
		match(45);
		break;
	}
	case 46:
	{
		antlr::RefAST tmp115_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp115_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp115_AST);
		}
		match(46);
		break;
	}
	case 47:
	{
		antlr::RefAST tmp116_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp116_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp116_AST);
		}
		match(47);
		break;
	}
	case 48:
	{
		antlr::RefAST tmp117_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp117_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp117_AST);
		}
		match(48);
		break;
	}
	case 49:
	{
		antlr::RefAST tmp118_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp118_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp118_AST);
		}
		match(49);
		break;
	}
	case 50:
	{
		antlr::RefAST tmp119_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp119_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp119_AST);
		}
		match(50);
		break;
	}
	case 51:
	{
		antlr::RefAST tmp120_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp120_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp120_AST);
		}
		match(51);
		break;
	}
	case 52:
	{
		antlr::RefAST tmp121_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp121_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp121_AST);
		}
		match(52);
		break;
	}
	case 53:
	{
		antlr::RefAST tmp122_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp122_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp122_AST);
		}
		match(53);
		break;
	}
	case 54:
	{
		antlr::RefAST tmp123_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp123_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp123_AST);
		}
		match(54);
		break;
	}
	case 55:
	{
		antlr::RefAST tmp124_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp124_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp124_AST);
		}
		match(55);
		break;
	}
	case 56:
	{
		antlr::RefAST tmp125_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp125_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp125_AST);
		}
		match(56);
		break;
	}
	case 57:
	{
		antlr::RefAST tmp126_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp126_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp126_AST);
		}
		match(57);
		break;
	}
	case 58:
	{
		antlr::RefAST tmp127_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp127_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp127_AST);
		}
		match(58);
		break;
	}
	case 59:
	{
		antlr::RefAST tmp128_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp128_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp128_AST);
		}
		match(59);
		break;
	}
	case 60:
	{
		antlr::RefAST tmp129_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp129_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp129_AST);
		}
		match(60);
		break;
	}
	case 61:
	{
		antlr::RefAST tmp130_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp130_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp130_AST);
		}
		match(61);
		break;
	}
	case 62:
	{
		antlr::RefAST tmp131_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp131_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp131_AST);
		}
		match(62);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		asmReg_AST = antlr::RefAST(currentAST.root);
#line 204 "Asm.g"
		asmReg_AST->setType(ASTRegister);
#line 3046 "AsmParser.cpp"
	}
	asmReg_AST = currentAST.root;
	returnAST = asmReg_AST;
}

bool  AsmParser::primitive() {
#line 159 "Asm.g"
	bool s;
#line 3055 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST primitive_AST = antlr::nullAST;
#line 159 "Asm.g"
	s=false;
#line 3061 "AsmParser.cpp"
	
	switch ( LA(1)) {
	case ID:
	{
		antlr::RefAST tmp132_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp132_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp132_AST);
		}
		match(ID);
		primitive_AST = currentAST.root;
		break;
	}
	case Int:
	{
		antlr::RefAST tmp133_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp133_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp133_AST);
		}
		match(Int);
		primitive_AST = currentAST.root;
		break;
	}
	case Hex:
	{
		antlr::RefAST tmp134_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp134_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp134_AST);
		}
		match(Hex);
		primitive_AST = currentAST.root;
		break;
	}
	case Command:
	{
		antlr::RefAST tmp135_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp135_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp135_AST);
		}
		match(Command);
		primitive_AST = currentAST.root;
		break;
	}
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	{
		asmReg();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		primitive_AST = currentAST.root;
		break;
	}
	case Reg:
	{
		antlr::RefAST tmp136_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp136_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp136_AST);
		}
		match(Reg);
		primitive_AST = currentAST.root;
		break;
	}
	case RelativeLocation:
	{
		antlr::RefAST tmp137_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp137_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp137_AST);
		}
		match(RelativeLocation);
		primitive_AST = currentAST.root;
		break;
	}
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	{
		{
		switch ( LA(1)) {
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
		{
			asmSegReg();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		case 70:
		case 71:
		case 72:
		case 73:
		case 74:
		case 75:
		case 76:
		case 77:
		case 78:
		case 79:
		case 80:
		case 81:
		{
			asmSensitiveReg();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			break;
		}
		default:
		{
			throw antlr::NoViableAltException(LT(1), getFilename());
		}
		}
		}
		if ( inputState->guessing==0 ) {
#line 161 "Asm.g"
			s=true;
#line 3224 "AsmParser.cpp"
		}
		primitive_AST = currentAST.root;
		break;
	}
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		asmInstrPrefixTokens();
		if (inputState->guessing==0) {
			astFactory->addASTChild( currentAST, returnAST );
		}
		primitive_AST = currentAST.root;
		break;
	}
	default:
		bool synPredMatched65 = false;
		if (((LA(1) == 63) && (LA(2) == LPAREN))) {
			int _m65 = mark();
			synPredMatched65 = true;
			inputState->guessing++;
			try {
				{
				asmFpReg();
				match(LPAREN);
				}
			}
			catch (antlr::RecognitionException& pe) {
				synPredMatched65 = false;
			}
			rewind(_m65);
			inputState->guessing--;
		}
		if ( synPredMatched65 ) {
			asmFpReg();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			match(LPAREN);
			antlr::RefAST tmp139_AST = antlr::nullAST;
			if ( inputState->guessing == 0 ) {
				tmp139_AST = astFactory->create(LT(1));
				astFactory->addASTChild(currentAST, tmp139_AST);
			}
			match(Int);
			match(RPAREN);
			if ( inputState->guessing==0 ) {
				primitive_AST = antlr::RefAST(currentAST.root);
#line 163 "Asm.g"
				primitive_AST = antlr::RefAST(astFactory->make((new antlr::ASTArray(2))->add(astFactory->create(ASTRegisterIndex,"register index"))->add(primitive_AST)));
#line 3275 "AsmParser.cpp"
				currentAST.root = primitive_AST;
				if ( primitive_AST!=antlr::nullAST &&
					primitive_AST->getFirstChild() != antlr::nullAST )
					  currentAST.child = primitive_AST->getFirstChild();
				else
					currentAST.child = primitive_AST;
				currentAST.advanceChildToEnd();
			}
			primitive_AST = currentAST.root;
		}
		else if ((LA(1) == 63) && (_tokenSet_7.member(LA(2)))) {
			asmFpReg();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			primitive_AST = currentAST.root;
		}
		else {
			bool synPredMatched68 = false;
			if (((LA(1) == LPAREN) && (_tokenSet_8.member(LA(2))))) {
				int _m68 = mark();
				synPredMatched68 = true;
				inputState->guessing++;
				try {
					{
					match(LPAREN);
					{
					switch ( LA(1)) {
					case 39:
					case 40:
					case 41:
					case 42:
					case 43:
					case 44:
					case 45:
					case 46:
					case 47:
					case 48:
					case 49:
					case 50:
					case 51:
					case 52:
					case 53:
					case 54:
					case 55:
					case 56:
					case 57:
					case 58:
					case 59:
					case 60:
					case 61:
					case 62:
					{
						asmReg();
						break;
					}
					case COMMA:
					{
						match(COMMA);
						break;
					}
					default:
					{
						throw antlr::NoViableAltException(LT(1), getFilename());
					}
					}
					}
					}
				}
				catch (antlr::RecognitionException& pe) {
					synPredMatched68 = false;
				}
				rewind(_m68);
				inputState->guessing--;
			}
			if ( synPredMatched68 ) {
				regOffsetBase();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				primitive_AST = currentAST.root;
			}
			else if ((LA(1) == LPAREN) && (_tokenSet_5.member(LA(2)))) {
				match(LPAREN);
				s=expression();
				if (inputState->guessing==0) {
					astFactory->addASTChild( currentAST, returnAST );
				}
				match(RPAREN);
				primitive_AST = currentAST.root;
			}
	else {
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}}
	returnAST = primitive_AST;
	return s;
}

void AsmParser::asmSensitiveReg() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmSensitiveReg_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case 70:
	{
		antlr::RefAST tmp143_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp143_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp143_AST);
		}
		match(70);
		break;
	}
	case 71:
	{
		antlr::RefAST tmp144_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp144_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp144_AST);
		}
		match(71);
		break;
	}
	case 72:
	{
		antlr::RefAST tmp145_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp145_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp145_AST);
		}
		match(72);
		break;
	}
	case 73:
	{
		antlr::RefAST tmp146_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp146_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp146_AST);
		}
		match(73);
		break;
	}
	case 74:
	{
		antlr::RefAST tmp147_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp147_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp147_AST);
		}
		match(74);
		break;
	}
	case 75:
	{
		antlr::RefAST tmp148_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp148_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp148_AST);
		}
		match(75);
		break;
	}
	case 76:
	{
		antlr::RefAST tmp149_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp149_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp149_AST);
		}
		match(76);
		break;
	}
	case 77:
	{
		antlr::RefAST tmp150_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp150_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp150_AST);
		}
		match(77);
		break;
	}
	case 78:
	{
		antlr::RefAST tmp151_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp151_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp151_AST);
		}
		match(78);
		break;
	}
	case 79:
	{
		antlr::RefAST tmp152_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp152_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp152_AST);
		}
		match(79);
		break;
	}
	case 80:
	{
		antlr::RefAST tmp153_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp153_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp153_AST);
		}
		match(80);
		break;
	}
	case 81:
	{
		antlr::RefAST tmp154_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp154_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp154_AST);
		}
		match(81);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		asmSensitiveReg_AST = antlr::RefAST(currentAST.root);
#line 215 "Asm.g"
		asmSensitiveReg_AST->setType(ASTRegister);
#line 3512 "AsmParser.cpp"
	}
	asmSensitiveReg_AST = currentAST.root;
	returnAST = asmSensitiveReg_AST;
}

void AsmParser::asmFpReg() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmFpReg_AST = antlr::nullAST;
	
	antlr::RefAST tmp155_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp155_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp155_AST);
	}
	match(63);
	if ( inputState->guessing==0 ) {
		asmFpReg_AST = antlr::RefAST(currentAST.root);
#line 206 "Asm.g"
		asmFpReg_AST->setType(ASTRegister);
#line 3533 "AsmParser.cpp"
	}
	asmFpReg_AST = currentAST.root;
	returnAST = asmFpReg_AST;
}

void AsmParser::asmInstrPrefixTokens() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST asmInstrPrefixTokens_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_lock:
	{
		antlr::RefAST tmp156_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp156_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp156_AST);
		}
		match(LITERAL_lock);
		break;
	}
	case LITERAL_rep:
	{
		antlr::RefAST tmp157_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp157_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp157_AST);
		}
		match(LITERAL_rep);
		break;
	}
	case LITERAL_repnz:
	{
		antlr::RefAST tmp158_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp158_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp158_AST);
		}
		match(LITERAL_repnz);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		asmInstrPrefixTokens_AST = antlr::RefAST(currentAST.root);
#line 220 "Asm.g"
		asmInstrPrefixTokens_AST->setType(ID);
#line 3586 "AsmParser.cpp"
	}
	asmInstrPrefixTokens_AST = currentAST.root;
	returnAST = asmInstrPrefixTokens_AST;
}

bool  AsmParser::signExpression() {
#line 171 "Asm.g"
	bool s;
#line 3595 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST signExpression_AST = antlr::nullAST;
	antlr::RefToken  m = antlr::nullToken;
	antlr::RefAST m_AST = antlr::nullAST;
#line 171 "Asm.g"
	s=false;
#line 3603 "AsmParser.cpp"
	
	{
	switch ( LA(1)) {
	case MINUS:
	{
		m = LT(1);
		if ( inputState->guessing == 0 ) {
			m_AST = astFactory->create(m);
			astFactory->makeASTRoot(currentAST, m_AST);
		}
		match(MINUS);
		if ( inputState->guessing==0 ) {
#line 172 "Asm.g"
			m_AST->setType(ASTNegative);
#line 3618 "AsmParser.cpp"
		}
		break;
	}
	case ID:
	case Command:
	case Int:
	case Hex:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	s=primitive();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	signExpression_AST = currentAST.root;
	returnAST = signExpression_AST;
	return s;
}

bool  AsmParser::notExpression() {
#line 174 "Asm.g"
	bool s;
#line 3696 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST notExpression_AST = antlr::nullAST;
#line 174 "Asm.g"
	s=false;
#line 3702 "AsmParser.cpp"
	
	{
	switch ( LA(1)) {
	case NOT:
	{
		antlr::RefAST tmp159_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp159_AST = astFactory->create(LT(1));
			astFactory->makeASTRoot(currentAST, tmp159_AST);
		}
		match(NOT);
		break;
	}
	case ID:
	case Command:
	case Int:
	case Hex:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case MINUS:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	s=signExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	notExpression_AST = currentAST.root;
	returnAST = notExpression_AST;
	return s;
}

bool  AsmParser::multiplyingExpression() {
#line 177 "Asm.g"
	bool s;
#line 3791 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST multiplyingExpression_AST = antlr::nullAST;
#line 177 "Asm.g"
	s=false; bool t;
#line 3797 "AsmParser.cpp"
	
	s=notExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{ // ( ... )*
	for (;;) {
		if ((LA(1) == STAR || LA(1) == DIV || LA(1) == PERCENT)) {
			{
			switch ( LA(1)) {
			case STAR:
			{
				antlr::RefAST tmp160_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp160_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp160_AST);
				}
				match(STAR);
				break;
			}
			case DIV:
			{
				antlr::RefAST tmp161_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp161_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp161_AST);
				}
				match(DIV);
				break;
			}
			case PERCENT:
			{
				antlr::RefAST tmp162_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp162_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp162_AST);
				}
				match(PERCENT);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			t=notExpression();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			if ( inputState->guessing==0 ) {
#line 179 "Asm.g"
				s|=t;
#line 3851 "AsmParser.cpp"
			}
		}
		else {
			goto _loop76;
		}
		
	}
	_loop76:;
	} // ( ... )*
	multiplyingExpression_AST = currentAST.root;
	returnAST = multiplyingExpression_AST;
	return s;
}

bool  AsmParser::shiftingExpression() {
#line 181 "Asm.g"
	bool s;
#line 3869 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST shiftingExpression_AST = antlr::nullAST;
#line 181 "Asm.g"
	s=false; bool t;
#line 3875 "AsmParser.cpp"
	
	s=multiplyingExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{ // ( ... )*
	for (;;) {
		if ((LA(1) == SHIFTLEFT || LA(1) == SHIFTRIGHT)) {
			{
			switch ( LA(1)) {
			case SHIFTLEFT:
			{
				antlr::RefAST tmp163_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp163_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp163_AST);
				}
				match(SHIFTLEFT);
				break;
			}
			case SHIFTRIGHT:
			{
				antlr::RefAST tmp164_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp164_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp164_AST);
				}
				match(SHIFTRIGHT);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			t=multiplyingExpression();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			if ( inputState->guessing==0 ) {
#line 183 "Asm.g"
				s|=t;
#line 3919 "AsmParser.cpp"
			}
		}
		else {
			goto _loop80;
		}
		
	}
	_loop80:;
	} // ( ... )*
	shiftingExpression_AST = currentAST.root;
	returnAST = shiftingExpression_AST;
	return s;
}

bool  AsmParser::bitwiseExpression() {
#line 185 "Asm.g"
	bool s;
#line 3937 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST bitwiseExpression_AST = antlr::nullAST;
#line 185 "Asm.g"
	s=false; bool t;
#line 3943 "AsmParser.cpp"
	
	s=shiftingExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{ // ( ... )*
	for (;;) {
		if (((LA(1) >= AND && LA(1) <= XOR))) {
			{
			switch ( LA(1)) {
			case AND:
			{
				antlr::RefAST tmp165_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp165_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp165_AST);
				}
				match(AND);
				break;
			}
			case OR:
			{
				antlr::RefAST tmp166_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp166_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp166_AST);
				}
				match(OR);
				break;
			}
			case XOR:
			{
				antlr::RefAST tmp167_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp167_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp167_AST);
				}
				match(XOR);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			t=shiftingExpression();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			if ( inputState->guessing==0 ) {
#line 186 "Asm.g"
				s|=t;
#line 3997 "AsmParser.cpp"
			}
		}
		else {
			goto _loop84;
		}
		
	}
	_loop84:;
	} // ( ... )*
	bitwiseExpression_AST = currentAST.root;
	returnAST = bitwiseExpression_AST;
	return s;
}

bool  AsmParser::addingExpression() {
#line 188 "Asm.g"
	bool s;
#line 4015 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST addingExpression_AST = antlr::nullAST;
#line 188 "Asm.g"
	s=false; bool t;
#line 4021 "AsmParser.cpp"
	
	s=bitwiseExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	{ // ( ... )*
	for (;;) {
		if ((LA(1) == MINUS || LA(1) == PLUS)) {
			{
			switch ( LA(1)) {
			case PLUS:
			{
				antlr::RefAST tmp168_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp168_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp168_AST);
				}
				match(PLUS);
				break;
			}
			case MINUS:
			{
				antlr::RefAST tmp169_AST = antlr::nullAST;
				if ( inputState->guessing == 0 ) {
					tmp169_AST = astFactory->create(LT(1));
					astFactory->makeASTRoot(currentAST, tmp169_AST);
				}
				match(MINUS);
				break;
			}
			default:
			{
				throw antlr::NoViableAltException(LT(1), getFilename());
			}
			}
			}
			t=bitwiseExpression();
			if (inputState->guessing==0) {
				astFactory->addASTChild( currentAST, returnAST );
			}
			if ( inputState->guessing==0 ) {
#line 190 "Asm.g"
				s|=t;
#line 4065 "AsmParser.cpp"
			}
		}
		else {
			goto _loop88;
		}
		
	}
	_loop88:;
	} // ( ... )*
	addingExpression_AST = currentAST.root;
	returnAST = addingExpression_AST;
	return s;
}

bool  AsmParser::makeConstantExpression() {
#line 192 "Asm.g"
	bool s;
#line 4083 "AsmParser.cpp"
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST makeConstantExpression_AST = antlr::nullAST;
#line 192 "Asm.g"
	s=false;
#line 4089 "AsmParser.cpp"
	
	{
	switch ( LA(1)) {
	case DOLLAR:
	{
		antlr::RefAST tmp170_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp170_AST = astFactory->create(LT(1));
			astFactory->makeASTRoot(currentAST, tmp170_AST);
		}
		match(DOLLAR);
		break;
	}
	case ID:
	case Command:
	case Int:
	case Hex:
	case LPAREN:
	case Reg:
	case RelativeLocation:
	case MINUS:
	case NOT:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 62:
	case 63:
	case 64:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
	case 72:
	case 73:
	case 74:
	case 75:
	case 76:
	case 77:
	case 78:
	case 79:
	case 80:
	case 81:
	case LITERAL_lock:
	case LITERAL_rep:
	case LITERAL_repnz:
	{
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	s=addingExpression();
	if (inputState->guessing==0) {
		astFactory->addASTChild( currentAST, returnAST );
	}
	makeConstantExpression_AST = currentAST.root;
	returnAST = makeConstantExpression_AST;
	return s;
}

void AsmParser::ia32_pop() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_pop_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_pop:
	{
		antlr::RefAST tmp171_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp171_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp171_AST);
		}
		match(LITERAL_pop);
		break;
	}
	case LITERAL_popl:
	{
		antlr::RefAST tmp172_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp172_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp172_AST);
		}
		match(LITERAL_popl);
		break;
	}
	case LITERAL_popd:
	{
		antlr::RefAST tmp173_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp173_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp173_AST);
		}
		match(LITERAL_popd);
		break;
	}
	case LITERAL_popb:
	{
		antlr::RefAST tmp174_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp174_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp174_AST);
		}
		match(LITERAL_popb);
		break;
	}
	case LITERAL_popw:
	{
		antlr::RefAST tmp175_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp175_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp175_AST);
		}
		match(LITERAL_popw);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_pop_AST = antlr::RefAST(currentAST.root);
#line 271 "Asm.g"
		ia32_pop_AST->setType(IA32_pop);
#line 4243 "AsmParser.cpp"
	}
	ia32_pop_AST = currentAST.root;
	returnAST = ia32_pop_AST;
}

void AsmParser::ia32_push() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_push_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_push:
	{
		antlr::RefAST tmp176_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp176_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp176_AST);
		}
		match(LITERAL_push);
		break;
	}
	case LITERAL_pushl:
	{
		antlr::RefAST tmp177_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp177_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp177_AST);
		}
		match(LITERAL_pushl);
		break;
	}
	case LITERAL_pushd:
	{
		antlr::RefAST tmp178_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp178_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp178_AST);
		}
		match(LITERAL_pushd);
		break;
	}
	case LITERAL_pushb:
	{
		antlr::RefAST tmp179_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp179_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp179_AST);
		}
		match(LITERAL_pushb);
		break;
	}
	case LITERAL_pushw:
	{
		antlr::RefAST tmp180_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp180_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp180_AST);
		}
		match(LITERAL_pushw);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_push_AST = antlr::RefAST(currentAST.root);
#line 274 "Asm.g"
		ia32_push_AST->setType(IA32_push);
#line 4316 "AsmParser.cpp"
	}
	ia32_push_AST = currentAST.root;
	returnAST = ia32_push_AST;
}

void AsmParser::ia32_mov() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_mov_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_mov:
	{
		antlr::RefAST tmp181_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp181_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp181_AST);
		}
		match(LITERAL_mov);
		break;
	}
	case LITERAL_movl:
	{
		antlr::RefAST tmp182_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp182_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp182_AST);
		}
		match(LITERAL_movl);
		break;
	}
	case LITERAL_movd:
	{
		antlr::RefAST tmp183_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp183_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp183_AST);
		}
		match(LITERAL_movd);
		break;
	}
	case LITERAL_movb:
	{
		antlr::RefAST tmp184_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp184_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp184_AST);
		}
		match(LITERAL_movb);
		break;
	}
	case LITERAL_movw:
	{
		antlr::RefAST tmp185_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp185_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp185_AST);
		}
		match(LITERAL_movw);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_mov_AST = antlr::RefAST(currentAST.root);
#line 276 "Asm.g"
		ia32_mov_AST->setType(IA32_mov);
#line 4389 "AsmParser.cpp"
	}
	ia32_mov_AST = currentAST.root;
	returnAST = ia32_mov_AST;
}

void AsmParser::ia32_popf() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_popf_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_popf:
	{
		antlr::RefAST tmp186_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp186_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp186_AST);
		}
		match(LITERAL_popf);
		break;
	}
	case LITERAL_popfl:
	{
		antlr::RefAST tmp187_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp187_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp187_AST);
		}
		match(LITERAL_popfl);
		break;
	}
	case LITERAL_popfd:
	{
		antlr::RefAST tmp188_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp188_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp188_AST);
		}
		match(LITERAL_popfd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_popf_AST = antlr::RefAST(currentAST.root);
#line 232 "Asm.g"
		ia32_popf_AST->setType(IA32_popf);
#line 4442 "AsmParser.cpp"
	}
	ia32_popf_AST = currentAST.root;
	returnAST = ia32_popf_AST;
}

void AsmParser::ia32_pushf() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_pushf_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_pushf:
	{
		antlr::RefAST tmp189_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp189_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp189_AST);
		}
		match(LITERAL_pushf);
		break;
	}
	case LITERAL_pushfl:
	{
		antlr::RefAST tmp190_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp190_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp190_AST);
		}
		match(LITERAL_pushfl);
		break;
	}
	case LITERAL_pushfd:
	{
		antlr::RefAST tmp191_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp191_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp191_AST);
		}
		match(LITERAL_pushfd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_pushf_AST = antlr::RefAST(currentAST.root);
#line 233 "Asm.g"
		ia32_pushf_AST->setType(IA32_pushf);
#line 4495 "AsmParser.cpp"
	}
	ia32_pushf_AST = currentAST.root;
	returnAST = ia32_pushf_AST;
}

void AsmParser::ia32_lgdt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lgdt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_lgdt:
	{
		antlr::RefAST tmp192_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp192_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp192_AST);
		}
		match(LITERAL_lgdt);
		break;
	}
	case LITERAL_lgdtl:
	{
		antlr::RefAST tmp193_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp193_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp193_AST);
		}
		match(LITERAL_lgdtl);
		break;
	}
	case LITERAL_lgdtd:
	{
		antlr::RefAST tmp194_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp194_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp194_AST);
		}
		match(LITERAL_lgdtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_lgdt_AST = antlr::RefAST(currentAST.root);
#line 234 "Asm.g"
		ia32_lgdt_AST->setType(IA32_lgdt);
#line 4548 "AsmParser.cpp"
	}
	ia32_lgdt_AST = currentAST.root;
	returnAST = ia32_lgdt_AST;
}

void AsmParser::ia32_sgdt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_sgdt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_sgdt:
	{
		antlr::RefAST tmp195_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp195_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp195_AST);
		}
		match(LITERAL_sgdt);
		break;
	}
	case LITERAL_sgdtl:
	{
		antlr::RefAST tmp196_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp196_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp196_AST);
		}
		match(LITERAL_sgdtl);
		break;
	}
	case LITERAL_sgdtd:
	{
		antlr::RefAST tmp197_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp197_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp197_AST);
		}
		match(LITERAL_sgdtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_sgdt_AST = antlr::RefAST(currentAST.root);
#line 235 "Asm.g"
		ia32_sgdt_AST->setType(IA32_sgdt);
#line 4601 "AsmParser.cpp"
	}
	ia32_sgdt_AST = currentAST.root;
	returnAST = ia32_sgdt_AST;
}

void AsmParser::ia32_lidt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lidt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_lidt:
	{
		antlr::RefAST tmp198_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp198_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp198_AST);
		}
		match(LITERAL_lidt);
		break;
	}
	case LITERAL_lidtl:
	{
		antlr::RefAST tmp199_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp199_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp199_AST);
		}
		match(LITERAL_lidtl);
		break;
	}
	case LITERAL_lidtd:
	{
		antlr::RefAST tmp200_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp200_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp200_AST);
		}
		match(LITERAL_lidtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_lidt_AST = antlr::RefAST(currentAST.root);
#line 236 "Asm.g"
		ia32_lidt_AST->setType(IA32_lidt);
#line 4654 "AsmParser.cpp"
	}
	ia32_lidt_AST = currentAST.root;
	returnAST = ia32_lidt_AST;
}

void AsmParser::ia32_sidt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_sidt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_sidt:
	{
		antlr::RefAST tmp201_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp201_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp201_AST);
		}
		match(LITERAL_sidt);
		break;
	}
	case LITERAL_sidtl:
	{
		antlr::RefAST tmp202_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp202_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp202_AST);
		}
		match(LITERAL_sidtl);
		break;
	}
	case LITERAL_sidtd:
	{
		antlr::RefAST tmp203_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp203_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp203_AST);
		}
		match(LITERAL_sidtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_sidt_AST = antlr::RefAST(currentAST.root);
#line 237 "Asm.g"
		ia32_sidt_AST->setType(IA32_sidt);
#line 4707 "AsmParser.cpp"
	}
	ia32_sidt_AST = currentAST.root;
	returnAST = ia32_sidt_AST;
}

void AsmParser::ia32_ljmp() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_ljmp_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp204_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp204_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp204_AST);
	}
	match(LITERAL_ljmp);
	}
	if ( inputState->guessing==0 ) {
		ia32_ljmp_AST = antlr::RefAST(currentAST.root);
#line 238 "Asm.g"
		ia32_ljmp_AST->setType(IA32_ljmp);
#line 4730 "AsmParser.cpp"
	}
	ia32_ljmp_AST = currentAST.root;
	returnAST = ia32_ljmp_AST;
}

void AsmParser::ia32_lds() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lds_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp205_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp205_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp205_AST);
	}
	match(LITERAL_lds);
	}
	if ( inputState->guessing==0 ) {
		ia32_lds_AST = antlr::RefAST(currentAST.root);
#line 239 "Asm.g"
		ia32_lds_AST->setType(IA32_lds);
#line 4753 "AsmParser.cpp"
	}
	ia32_lds_AST = currentAST.root;
	returnAST = ia32_lds_AST;
}

void AsmParser::ia32_les() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_les_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp206_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp206_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp206_AST);
	}
	match(LITERAL_les);
	}
	if ( inputState->guessing==0 ) {
		ia32_les_AST = antlr::RefAST(currentAST.root);
#line 240 "Asm.g"
		ia32_les_AST->setType(IA32_les);
#line 4776 "AsmParser.cpp"
	}
	ia32_les_AST = currentAST.root;
	returnAST = ia32_les_AST;
}

void AsmParser::ia32_lfs() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lfs_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp207_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp207_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp207_AST);
	}
	match(LITERAL_lfs);
	}
	if ( inputState->guessing==0 ) {
		ia32_lfs_AST = antlr::RefAST(currentAST.root);
#line 241 "Asm.g"
		ia32_lfs_AST->setType(IA32_lfs);
#line 4799 "AsmParser.cpp"
	}
	ia32_lfs_AST = currentAST.root;
	returnAST = ia32_lfs_AST;
}

void AsmParser::ia32_lgs() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lgs_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp208_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp208_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp208_AST);
	}
	match(LITERAL_lgs);
	}
	if ( inputState->guessing==0 ) {
		ia32_lgs_AST = antlr::RefAST(currentAST.root);
#line 242 "Asm.g"
		ia32_lgs_AST->setType(IA32_lgs);
#line 4822 "AsmParser.cpp"
	}
	ia32_lgs_AST = currentAST.root;
	returnAST = ia32_lgs_AST;
}

void AsmParser::ia32_lss() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lss_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp209_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp209_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp209_AST);
	}
	match(LITERAL_lss);
	}
	if ( inputState->guessing==0 ) {
		ia32_lss_AST = antlr::RefAST(currentAST.root);
#line 243 "Asm.g"
		ia32_lss_AST->setType(IA32_lss);
#line 4845 "AsmParser.cpp"
	}
	ia32_lss_AST = currentAST.root;
	returnAST = ia32_lss_AST;
}

void AsmParser::ia32_clts() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_clts_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp210_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp210_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp210_AST);
	}
	match(LITERAL_clts);
	}
	if ( inputState->guessing==0 ) {
		ia32_clts_AST = antlr::RefAST(currentAST.root);
#line 244 "Asm.g"
		ia32_clts_AST->setType(IA32_clts);
#line 4868 "AsmParser.cpp"
	}
	ia32_clts_AST = currentAST.root;
	returnAST = ia32_clts_AST;
}

void AsmParser::ia32_hlt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_hlt_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp211_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp211_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp211_AST);
	}
	match(LITERAL_hlt);
	}
	if ( inputState->guessing==0 ) {
		ia32_hlt_AST = antlr::RefAST(currentAST.root);
#line 245 "Asm.g"
		ia32_hlt_AST->setType(IA32_hlt);
#line 4891 "AsmParser.cpp"
	}
	ia32_hlt_AST = currentAST.root;
	returnAST = ia32_hlt_AST;
}

void AsmParser::ia32_cli() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_cli_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp212_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp212_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp212_AST);
	}
	match(LITERAL_cli);
	}
	if ( inputState->guessing==0 ) {
		ia32_cli_AST = antlr::RefAST(currentAST.root);
#line 246 "Asm.g"
		ia32_cli_AST->setType(IA32_cli);
#line 4914 "AsmParser.cpp"
	}
	ia32_cli_AST = currentAST.root;
	returnAST = ia32_cli_AST;
}

void AsmParser::ia32_sti() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_sti_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp213_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp213_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp213_AST);
	}
	match(LITERAL_sti);
	}
	if ( inputState->guessing==0 ) {
		ia32_sti_AST = antlr::RefAST(currentAST.root);
#line 247 "Asm.g"
		ia32_sti_AST->setType(IA32_sti);
#line 4937 "AsmParser.cpp"
	}
	ia32_sti_AST = currentAST.root;
	returnAST = ia32_sti_AST;
}

void AsmParser::ia32_lldt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lldt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_lldt:
	{
		antlr::RefAST tmp214_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp214_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp214_AST);
		}
		match(LITERAL_lldt);
		break;
	}
	case LITERAL_lldtl:
	{
		antlr::RefAST tmp215_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp215_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp215_AST);
		}
		match(LITERAL_lldtl);
		break;
	}
	case LITERAL_lldtd:
	{
		antlr::RefAST tmp216_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp216_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp216_AST);
		}
		match(LITERAL_lldtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_lldt_AST = antlr::RefAST(currentAST.root);
#line 248 "Asm.g"
		ia32_lldt_AST->setType(IA32_lldt);
#line 4990 "AsmParser.cpp"
	}
	ia32_lldt_AST = currentAST.root;
	returnAST = ia32_lldt_AST;
}

void AsmParser::ia32_sldt() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_sldt_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_sldt:
	{
		antlr::RefAST tmp217_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp217_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp217_AST);
		}
		match(LITERAL_sldt);
		break;
	}
	case LITERAL_sldtl:
	{
		antlr::RefAST tmp218_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp218_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp218_AST);
		}
		match(LITERAL_sldtl);
		break;
	}
	case LITERAL_sldtd:
	{
		antlr::RefAST tmp219_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp219_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp219_AST);
		}
		match(LITERAL_sldtd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_sldt_AST = antlr::RefAST(currentAST.root);
#line 249 "Asm.g"
		ia32_sldt_AST->setType(IA32_sldt);
#line 5043 "AsmParser.cpp"
	}
	ia32_sldt_AST = currentAST.root;
	returnAST = ia32_sldt_AST;
}

void AsmParser::ia32_ltr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_ltr_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_ltr:
	{
		antlr::RefAST tmp220_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp220_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp220_AST);
		}
		match(LITERAL_ltr);
		break;
	}
	case LITERAL_ltrl:
	{
		antlr::RefAST tmp221_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp221_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp221_AST);
		}
		match(LITERAL_ltrl);
		break;
	}
	case LITERAL_ltrd:
	{
		antlr::RefAST tmp222_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp222_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp222_AST);
		}
		match(LITERAL_ltrd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_ltr_AST = antlr::RefAST(currentAST.root);
#line 250 "Asm.g"
		ia32_ltr_AST->setType(IA32_ltr);
#line 5096 "AsmParser.cpp"
	}
	ia32_ltr_AST = currentAST.root;
	returnAST = ia32_ltr_AST;
}

void AsmParser::ia32_str() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_str_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_str:
	{
		antlr::RefAST tmp223_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp223_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp223_AST);
		}
		match(LITERAL_str);
		break;
	}
	case LITERAL_strl:
	{
		antlr::RefAST tmp224_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp224_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp224_AST);
		}
		match(LITERAL_strl);
		break;
	}
	case LITERAL_strd:
	{
		antlr::RefAST tmp225_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp225_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp225_AST);
		}
		match(LITERAL_strd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_str_AST = antlr::RefAST(currentAST.root);
#line 251 "Asm.g"
		ia32_str_AST->setType(IA32_str);
#line 5149 "AsmParser.cpp"
	}
	ia32_str_AST = currentAST.root;
	returnAST = ia32_str_AST;
}

void AsmParser::ia32_in() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_in_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_inb:
	{
		antlr::RefAST tmp226_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp226_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp226_AST);
		}
		match(LITERAL_inb);
		break;
	}
	case LITERAL_inw:
	{
		antlr::RefAST tmp227_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp227_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp227_AST);
		}
		match(LITERAL_inw);
		break;
	}
	case LITERAL_inl:
	{
		antlr::RefAST tmp228_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp228_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp228_AST);
		}
		match(LITERAL_inl);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_in_AST = antlr::RefAST(currentAST.root);
#line 252 "Asm.g"
		ia32_in_AST->setType(IA32_in);
#line 5202 "AsmParser.cpp"
	}
	ia32_in_AST = currentAST.root;
	returnAST = ia32_in_AST;
}

void AsmParser::ia32_out() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_out_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_outb:
	{
		antlr::RefAST tmp229_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp229_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp229_AST);
		}
		match(LITERAL_outb);
		break;
	}
	case LITERAL_outw:
	{
		antlr::RefAST tmp230_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp230_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp230_AST);
		}
		match(LITERAL_outw);
		break;
	}
	case LITERAL_outl:
	{
		antlr::RefAST tmp231_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp231_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp231_AST);
		}
		match(LITERAL_outl);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_out_AST = antlr::RefAST(currentAST.root);
#line 253 "Asm.g"
		ia32_out_AST->setType(IA32_out);
#line 5255 "AsmParser.cpp"
	}
	ia32_out_AST = currentAST.root;
	returnAST = ia32_out_AST;
}

void AsmParser::ia32_invlpg() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_invlpg_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp232_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp232_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp232_AST);
	}
	match(LITERAL_invlpg);
	}
	if ( inputState->guessing==0 ) {
		ia32_invlpg_AST = antlr::RefAST(currentAST.root);
#line 254 "Asm.g"
		ia32_invlpg_AST->setType(IA32_invlpg);
#line 5278 "AsmParser.cpp"
	}
	ia32_invlpg_AST = currentAST.root;
	returnAST = ia32_invlpg_AST;
}

void AsmParser::ia32_iret() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_iret_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_iret:
	{
		antlr::RefAST tmp233_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp233_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp233_AST);
		}
		match(LITERAL_iret);
		break;
	}
	case LITERAL_iretl:
	{
		antlr::RefAST tmp234_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp234_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp234_AST);
		}
		match(LITERAL_iretl);
		break;
	}
	case LITERAL_iretd:
	{
		antlr::RefAST tmp235_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp235_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp235_AST);
		}
		match(LITERAL_iretd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_iret_AST = antlr::RefAST(currentAST.root);
#line 255 "Asm.g"
		ia32_iret_AST->setType(IA32_iret);
#line 5331 "AsmParser.cpp"
	}
	ia32_iret_AST = currentAST.root;
	returnAST = ia32_iret_AST;
}

void AsmParser::ia32_lret() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lret_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp236_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp236_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp236_AST);
	}
	match(LITERAL_lret);
	}
	if ( inputState->guessing==0 ) {
		ia32_lret_AST = antlr::RefAST(currentAST.root);
#line 256 "Asm.g"
		ia32_lret_AST->setType(IA32_lret);
#line 5354 "AsmParser.cpp"
	}
	ia32_lret_AST = currentAST.root;
	returnAST = ia32_lret_AST;
}

void AsmParser::ia32_cpuid() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_cpuid_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp237_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp237_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp237_AST);
	}
	match(LITERAL_cpuid);
	}
	if ( inputState->guessing==0 ) {
		ia32_cpuid_AST = antlr::RefAST(currentAST.root);
#line 257 "Asm.g"
		ia32_cpuid_AST->setType(IA32_cpuid);
#line 5377 "AsmParser.cpp"
	}
	ia32_cpuid_AST = currentAST.root;
	returnAST = ia32_cpuid_AST;
}

void AsmParser::ia32_wrmsr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_wrmsr_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp238_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp238_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp238_AST);
	}
	match(LITERAL_wrmsr);
	}
	if ( inputState->guessing==0 ) {
		ia32_wrmsr_AST = antlr::RefAST(currentAST.root);
#line 258 "Asm.g"
		ia32_wrmsr_AST->setType(IA32_wrmsr);
#line 5400 "AsmParser.cpp"
	}
	ia32_wrmsr_AST = currentAST.root;
	returnAST = ia32_wrmsr_AST;
}

void AsmParser::ia32_rdmsr() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_rdmsr_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp239_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp239_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp239_AST);
	}
	match(LITERAL_rdmsr);
	}
	if ( inputState->guessing==0 ) {
		ia32_rdmsr_AST = antlr::RefAST(currentAST.root);
#line 259 "Asm.g"
		ia32_rdmsr_AST->setType(IA32_rdmsr);
#line 5423 "AsmParser.cpp"
	}
	ia32_rdmsr_AST = currentAST.root;
	returnAST = ia32_rdmsr_AST;
}

void AsmParser::ia32_int() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_int_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp240_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp240_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp240_AST);
	}
	match(LITERAL_int);
	}
	if ( inputState->guessing==0 ) {
		ia32_int_AST = antlr::RefAST(currentAST.root);
#line 260 "Asm.g"
		ia32_int_AST->setType(IA32_int);
#line 5446 "AsmParser.cpp"
	}
	ia32_int_AST = currentAST.root;
	returnAST = ia32_int_AST;
}

void AsmParser::ia32_ud2() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_ud2_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp241_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp241_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp241_AST);
	}
	match(141);
	}
	if ( inputState->guessing==0 ) {
		ia32_ud2_AST = antlr::RefAST(currentAST.root);
#line 261 "Asm.g"
		ia32_ud2_AST->setType(IA32_ud2);
#line 5469 "AsmParser.cpp"
	}
	ia32_ud2_AST = currentAST.root;
	returnAST = ia32_ud2_AST;
}

void AsmParser::ia32_invd() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_invd_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp242_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp242_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp242_AST);
	}
	match(LITERAL_invd);
	}
	if ( inputState->guessing==0 ) {
		ia32_invd_AST = antlr::RefAST(currentAST.root);
#line 262 "Asm.g"
		ia32_invd_AST->setType(IA32_invd);
#line 5492 "AsmParser.cpp"
	}
	ia32_invd_AST = currentAST.root;
	returnAST = ia32_invd_AST;
}

void AsmParser::ia32_wbinvd() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_wbinvd_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp243_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp243_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp243_AST);
	}
	match(LITERAL_wbinvd);
	}
	if ( inputState->guessing==0 ) {
		ia32_wbinvd_AST = antlr::RefAST(currentAST.root);
#line 263 "Asm.g"
		ia32_wbinvd_AST->setType(IA32_wbinvd);
#line 5515 "AsmParser.cpp"
	}
	ia32_wbinvd_AST = currentAST.root;
	returnAST = ia32_wbinvd_AST;
}

void AsmParser::ia32_smsw() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_smsw_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_smsw:
	{
		antlr::RefAST tmp244_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp244_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp244_AST);
		}
		match(LITERAL_smsw);
		break;
	}
	case LITERAL_smswl:
	{
		antlr::RefAST tmp245_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp245_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp245_AST);
		}
		match(LITERAL_smswl);
		break;
	}
	case LITERAL_smswd:
	{
		antlr::RefAST tmp246_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp246_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp246_AST);
		}
		match(LITERAL_smswd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_smsw_AST = antlr::RefAST(currentAST.root);
#line 264 "Asm.g"
		ia32_smsw_AST->setType(IA32_smsw);
#line 5568 "AsmParser.cpp"
	}
	ia32_smsw_AST = currentAST.root;
	returnAST = ia32_smsw_AST;
}

void AsmParser::ia32_lmsw() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lmsw_AST = antlr::nullAST;
	
	{
	switch ( LA(1)) {
	case LITERAL_lmsw:
	{
		antlr::RefAST tmp247_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp247_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp247_AST);
		}
		match(LITERAL_lmsw);
		break;
	}
	case LITERAL_lmswl:
	{
		antlr::RefAST tmp248_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp248_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp248_AST);
		}
		match(LITERAL_lmswl);
		break;
	}
	case LITERAL_lmswd:
	{
		antlr::RefAST tmp249_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp249_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp249_AST);
		}
		match(LITERAL_lmswd);
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	}
	if ( inputState->guessing==0 ) {
		ia32_lmsw_AST = antlr::RefAST(currentAST.root);
#line 265 "Asm.g"
		ia32_lmsw_AST->setType(IA32_lmsw);
#line 5621 "AsmParser.cpp"
	}
	ia32_lmsw_AST = currentAST.root;
	returnAST = ia32_lmsw_AST;
}

void AsmParser::ia32_arpl() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_arpl_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp250_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp250_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp250_AST);
	}
	match(LITERAL_arpl);
	}
	if ( inputState->guessing==0 ) {
		ia32_arpl_AST = antlr::RefAST(currentAST.root);
#line 266 "Asm.g"
		ia32_arpl_AST->setType(IA32_arpl);
#line 5644 "AsmParser.cpp"
	}
	ia32_arpl_AST = currentAST.root;
	returnAST = ia32_arpl_AST;
}

void AsmParser::ia32_lar() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lar_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp251_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp251_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp251_AST);
	}
	match(LITERAL_lar);
	}
	if ( inputState->guessing==0 ) {
		ia32_lar_AST = antlr::RefAST(currentAST.root);
#line 267 "Asm.g"
		ia32_lar_AST->setType(IA32_lar);
#line 5667 "AsmParser.cpp"
	}
	ia32_lar_AST = currentAST.root;
	returnAST = ia32_lar_AST;
}

void AsmParser::ia32_lsl() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_lsl_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp252_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp252_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp252_AST);
	}
	match(LITERAL_lsl);
	}
	if ( inputState->guessing==0 ) {
		ia32_lsl_AST = antlr::RefAST(currentAST.root);
#line 268 "Asm.g"
		ia32_lsl_AST->setType(IA32_lsl);
#line 5690 "AsmParser.cpp"
	}
	ia32_lsl_AST = currentAST.root;
	returnAST = ia32_lsl_AST;
}

void AsmParser::ia32_rsm() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST ia32_rsm_AST = antlr::nullAST;
	
	{
	antlr::RefAST tmp253_AST = antlr::nullAST;
	if ( inputState->guessing == 0 ) {
		tmp253_AST = astFactory->create(LT(1));
		astFactory->addASTChild(currentAST, tmp253_AST);
	}
	match(LITERAL_rsm);
	}
	if ( inputState->guessing==0 ) {
		ia32_rsm_AST = antlr::RefAST(currentAST.root);
#line 269 "Asm.g"
		ia32_rsm_AST->setType(IA32_rsm);
#line 5713 "AsmParser.cpp"
	}
	ia32_rsm_AST = currentAST.root;
	returnAST = ia32_rsm_AST;
}

void AsmParser::astDefs() {
	returnAST = antlr::nullAST;
	antlr::ASTPair currentAST;
	antlr::RefAST astDefs_AST = antlr::nullAST;
	
	switch ( LA(1)) {
	case ASTMacroDef:
	{
		antlr::RefAST tmp254_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp254_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp254_AST);
		}
		match(ASTMacroDef);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTMacroDefParams:
	{
		antlr::RefAST tmp255_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp255_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp255_AST);
		}
		match(ASTMacroDefParams);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTInstruction:
	{
		antlr::RefAST tmp256_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp256_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp256_AST);
		}
		match(ASTInstruction);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTInstructionPrefix:
	{
		antlr::RefAST tmp257_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp257_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp257_AST);
		}
		match(ASTInstructionPrefix);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTSensitive:
	{
		antlr::RefAST tmp258_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp258_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp258_AST);
		}
		match(ASTSensitive);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTBasicBlock:
	{
		antlr::RefAST tmp259_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp259_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp259_AST);
		}
		match(ASTBasicBlock);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTCommand:
	{
		antlr::RefAST tmp260_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp260_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp260_AST);
		}
		match(ASTCommand);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTDbgCommand:
	{
		antlr::RefAST tmp261_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp261_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp261_AST);
		}
		match(ASTDbgCommand);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTDefaultParam:
	{
		antlr::RefAST tmp262_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp262_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp262_AST);
		}
		match(ASTDefaultParam);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTRegisterDisplacement:
	{
		antlr::RefAST tmp263_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp263_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp263_AST);
		}
		match(ASTRegisterDisplacement);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTRegisterBaseIndexScale:
	{
		antlr::RefAST tmp264_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp264_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp264_AST);
		}
		match(ASTRegisterBaseIndexScale);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTRegisterIndex:
	{
		antlr::RefAST tmp265_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp265_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp265_AST);
		}
		match(ASTRegisterIndex);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTDereference:
	{
		antlr::RefAST tmp266_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp266_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp266_AST);
		}
		match(ASTDereference);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTSegment:
	{
		antlr::RefAST tmp267_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp267_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp267_AST);
		}
		match(ASTSegment);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTRegister:
	{
		antlr::RefAST tmp268_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp268_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp268_AST);
		}
		match(ASTRegister);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTNegative:
	{
		antlr::RefAST tmp269_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp269_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp269_AST);
		}
		match(ASTNegative);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTGprof:
	{
		antlr::RefAST tmp270_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp270_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp270_AST);
		}
		match(ASTGprof);
		astDefs_AST = currentAST.root;
		break;
	}
	case ASTGcov:
	{
		antlr::RefAST tmp271_AST = antlr::nullAST;
		if ( inputState->guessing == 0 ) {
			tmp271_AST = astFactory->create(LT(1));
			astFactory->addASTChild(currentAST, tmp271_AST);
		}
		match(ASTGcov);
		astDefs_AST = currentAST.root;
		break;
	}
	default:
	{
		throw antlr::NoViableAltException(LT(1), getFilename());
	}
	}
	returnAST = astDefs_AST;
}

void AsmParser::initializeASTFactory( antlr::ASTFactory& factory )
{
	factory.setMaxNodeType(246);
}
const char* AsmParser::tokenNames[] = {
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

const unsigned long AsmParser::_tokenSet_0_data_[] = { 59504UL, 0UL, 4294705152UL, 4294967295UL, 4294967295UL, 511UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Newline SEMI ID ".macro" Command ".file" ".loc" "lock" "rep" "repnz" 
// "call" "popf" "popfl" "popfd" "pushf" "pushfl" "pushfd" "lgdt" "lgdtl" 
// "lgdtd" "sgdt" "sgdtl" "sgdtd" "lidt" "lidtl" "lidtd" "sidt" "sidtl" 
// "sidtd" "ljmp" "lds" "les" "lfs" "lgs" "lss" "clts" "hlt" "cli" "sti" 
// "lldt" "lldtl" "lldtd" "sldt" "sldtl" "sldtd" "ltr" "ltrl" "ltrd" "str" 
// "strl" "strd" "inb" "inw" "inl" "outb" "outw" "outl" "invlpg" "iret" 
// "iretl" "iretd" "lret" "cpuid" "wrmsr" "rdmsr" "int" "ud2" "invd" "wbinvd" 
// "smsw" "smswl" "smswd" "lmsw" "lmswl" "lmswd" "arpl" "lar" "lsl" "rsm" 
// "pop" "popl" "popd" "popb" "popw" "push" "pushl" "pushd" "pushb" "pushw" 
// "mov" "movl" "movd" "movb" "movw" 
const antlr::BitSet AsmParser::_tokenSet_0(_tokenSet_0_data_,12);
const unsigned long AsmParser::_tokenSet_1_data_[] = { 64UL, 0UL, 4292870144UL, 4294967295UL, 4294967295UL, 511UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// ID "call" "popf" "popfl" "popfd" "pushf" "pushfl" "pushfd" "lgdt" "lgdtl" 
// "lgdtd" "sgdt" "sgdtl" "sgdtd" "lidt" "lidtl" "lidtd" "sidt" "sidtl" 
// "sidtd" "ljmp" "lds" "les" "lfs" "lgs" "lss" "clts" "hlt" "cli" "sti" 
// "lldt" "lldtl" "lldtd" "sldt" "sldtl" "sldtd" "ltr" "ltrl" "ltrd" "str" 
// "strl" "strd" "inb" "inw" "inl" "outb" "outw" "outl" "invlpg" "iret" 
// "iretl" "iretd" "lret" "cpuid" "wrmsr" "rdmsr" "int" "ud2" "invd" "wbinvd" 
// "smsw" "smswl" "smswd" "lmsw" "lmswl" "lmswd" "arpl" "lar" "lsl" "rsm" 
// "pop" "popl" "popd" "popb" "popw" "push" "pushl" "pushd" "pushb" "pushw" 
// "mov" "movl" "movd" "movb" "movw" 
const antlr::BitSet AsmParser::_tokenSet_1(_tokenSet_1_data_,12);
const unsigned long AsmParser::_tokenSet_2_data_[] = { 1030955120UL, 4294967232UL, 2097151UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Newline SEMI ID Command "mcount" "__gcov_merge_add" Int Hex STAR LPAREN 
// Reg RelativeLocation MINUS NOT DOLLAR "%al" "%bl" "%cl" "%dl" "%ah" 
// "%bh" "%ch" "%dh" "%ax" "%bx" "%cx" "%dx" "%si" "%di" "%sp" "%bp" "%eax" 
// "%ebx" "%ecx" "%edx" "%esi" "%edi" "%esp" "%ebp" "%st" "%cs" "%ds" "%es" 
// "%fs" "%gs" "%ss" "%cr0" "%cr2" "%cr3" "%cr4" "%db0" "%db1" "%db2" "%db3" 
// "%db4" "%db5" "%db6" "%db7" "lock" "rep" "repnz" 
const antlr::BitSet AsmParser::_tokenSet_2(_tokenSet_2_data_,8);
const unsigned long AsmParser::_tokenSet_3_data_[] = { 1030758512UL, 4294967232UL, 2097151UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Newline SEMI ID Command Int Hex STAR LPAREN Reg RelativeLocation MINUS 
// NOT DOLLAR "%al" "%bl" "%cl" "%dl" "%ah" "%bh" "%ch" "%dh" "%ax" "%bx" 
// "%cx" "%dx" "%si" "%di" "%sp" "%bp" "%eax" "%ebx" "%ecx" "%edx" "%esi" 
// "%edi" "%esp" "%ebp" "%st" "%cs" "%ds" "%es" "%fs" "%gs" "%ss" "%cr0" 
// "%cr2" "%cr3" "%cr4" "%db0" "%db1" "%db2" "%db3" "%db4" "%db5" "%db6" 
// "%db7" "lock" "rep" "repnz" 
const antlr::BitSet AsmParser::_tokenSet_3(_tokenSet_3_data_,8);
const unsigned long AsmParser::_tokenSet_4_data_[] = { 3940352UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Command String Option Int Hex 
const antlr::BitSet AsmParser::_tokenSet_4(_tokenSet_4_data_,8);
const unsigned long AsmParser::_tokenSet_5_data_[] = { 1026564160UL, 4294967232UL, 2097151UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// ID Command Int Hex LPAREN Reg RelativeLocation MINUS NOT DOLLAR "%al" 
// "%bl" "%cl" "%dl" "%ah" "%bh" "%ch" "%dh" "%ax" "%bx" "%cx" "%dx" "%si" 
// "%di" "%sp" "%bp" "%eax" "%ebx" "%ecx" "%edx" "%esi" "%edi" "%esp" "%ebp" 
// "%st" "%cs" "%ds" "%es" "%fs" "%gs" "%ss" "%cr0" "%cr2" "%cr3" "%cr4" 
// "%db0" "%db1" "%db2" "%db3" "%db4" "%db5" "%db6" "%db7" "lock" "rep" 
// "repnz" 
const antlr::BitSet AsmParser::_tokenSet_5(_tokenSet_5_data_,8);
const unsigned long AsmParser::_tokenSet_6_data_[] = { 4251985008UL, 4294967295UL, 2097151UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Newline SEMI ID COMMA Command Int Hex STAR LPAREN Reg RelativeLocation 
// MINUS NOT DIV PERCENT SHIFTLEFT SHIFTRIGHT AND OR XOR PLUS DOLLAR "%al" 
// "%bl" "%cl" "%dl" "%ah" "%bh" "%ch" "%dh" "%ax" "%bx" "%cx" "%dx" "%si" 
// "%di" "%sp" "%bp" "%eax" "%ebx" "%ecx" "%edx" "%esi" "%edi" "%esp" "%ebp" 
// "%st" "%cs" "%ds" "%es" "%fs" "%gs" "%ss" "%cr0" "%cr2" "%cr3" "%cr4" 
// "%db0" "%db1" "%db2" "%db3" "%db4" "%db5" "%db6" "%db7" "lock" "rep" 
// "repnz" 
const antlr::BitSet AsmParser::_tokenSet_6(_tokenSet_6_data_,8);
const unsigned long AsmParser::_tokenSet_7_data_[] = { 3544187952UL, 63UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// Newline SEMI COMMA STAR LPAREN RPAREN MINUS DIV PERCENT SHIFTLEFT SHIFTRIGHT 
// AND OR XOR PLUS 
const antlr::BitSet AsmParser::_tokenSet_7(_tokenSet_7_data_,8);
const unsigned long AsmParser::_tokenSet_8_data_[] = { 1024UL, 2147483520UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
// COMMA "%al" "%bl" "%cl" "%dl" "%ah" "%bh" "%ch" "%dh" "%ax" "%bx" "%cx" 
// "%dx" "%si" "%di" "%sp" "%bp" "%eax" "%ebx" "%ecx" "%edx" "%esi" "%edi" 
// "%esp" "%ebp" 
const antlr::BitSet AsmParser::_tokenSet_8(_tokenSet_8_data_,8);


ANTLR_END_NAMESPACE

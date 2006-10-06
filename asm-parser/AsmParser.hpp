#ifndef INC_AsmParser_hpp_
#define INC_AsmParser_hpp_

#line 2 "Asm.g"

    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before antlr generated includes in the header file.
    #include <iostream>

#line 13 "AsmParser.hpp"
#include <antlr/config.hpp>
/* $ANTLR 2.7.6 (20060712): "Asm.g" -> "AsmParser.hpp"$ */
#include <antlr/TokenStream.hpp>
#include <antlr/TokenBuffer.hpp>
#include "AsmParserTokenTypes.hpp"
#include <antlr/LLkParser.hpp>

#line 9 "Asm.g"

    // Inserted after antlr generated includes in the header file
    // outside any generated namespace specifications

#line 26 "AsmParser.hpp"
ANTLR_BEGIN_NAMESPACE(Asm)
class CUSTOM_API AsmParser : public antlr::LLkParser, public AsmParserTokenTypes
{
#line 41 "Asm.g"

    // Additional methods and members.
#line 30 "AsmParser.hpp"
public:
	void initializeASTFactory( antlr::ASTFactory& factory );
protected:
	AsmParser(antlr::TokenBuffer& tokenBuf, int k);
public:
	AsmParser(antlr::TokenBuffer& tokenBuf);
protected:
	AsmParser(antlr::TokenStream& lexer, int k);
public:
	AsmParser(antlr::TokenStream& lexer);
	AsmParser(const antlr::ParserSharedInputState& state);
	int getNumTokens() const
	{
		return AsmParser::NUM_TOKENS;
	}
	const char* getTokenName( int type ) const
	{
		if( type > getNumTokens() ) return 0;
		return AsmParser::tokenNames[type];
	}
	const char* const* getTokenNames() const
	{
		return AsmParser::tokenNames;
	}
	public: void asmFile();
	public: void asmBlocks();
	public: void asmAnonymousBlock();
	public: void asmBasicBlock();
	public: void asmStatement();
	public: void asmLabel();
	public: void asmEnd();
	public: void asmInstrPrefix();
	public: void asmInstr();
	public: void asmCommand();
	public: void asmAssignment();
	public: void commandParam();
	public: void asmMacroDefParams();
	public: void asmMacroDef();
	public: void commandParams();
	public: void simpleParams();
	public: void asmGprof();
	public: void asmInnocuousInstr();
	public: bool  instrParams();
	public: void asmSensitiveInstr();
	public: void ia32_call();
	public: void simpleParam();
	public: void defaultParam();
	public: bool  instrParam();
	public: bool  regExpression();
	public: bool  regDereferenceExpr();
	public: bool  regSegmentExpr();
	public: void asmSegReg();
	public: bool  regDisplacementExpr();
	public: bool  expression();
	public: void regOffsetBase();
	public: void asmReg();
	public: bool  primitive();
	public: void asmSensitiveReg();
	public: void asmFpReg();
	public: void asmInstrPrefixTokens();
	public: bool  signExpression();
	public: bool  notExpression();
	public: bool  multiplyingExpression();
	public: bool  shiftingExpression();
	public: bool  bitwiseExpression();
	public: bool  addingExpression();
	public: bool  makeConstantExpression();
	public: void ia32_pop();
	public: void ia32_push();
	public: void ia32_mov();
	public: void ia32_popf();
	public: void ia32_pushf();
	public: void ia32_lgdt();
	public: void ia32_sgdt();
	public: void ia32_lidt();
	public: void ia32_sidt();
	public: void ia32_ljmp();
	public: void ia32_lds();
	public: void ia32_les();
	public: void ia32_lfs();
	public: void ia32_lgs();
	public: void ia32_lss();
	public: void ia32_clts();
	public: void ia32_hlt();
	public: void ia32_cli();
	public: void ia32_sti();
	public: void ia32_lldt();
	public: void ia32_sldt();
	public: void ia32_ltr();
	public: void ia32_str();
	public: void ia32_in();
	public: void ia32_out();
	public: void ia32_invlpg();
	public: void ia32_iret();
	public: void ia32_lret();
	public: void ia32_cpuid();
	public: void ia32_wrmsr();
	public: void ia32_rdmsr();
	public: void ia32_int();
	public: void ia32_ud2();
	public: void ia32_invd();
	public: void ia32_wbinvd();
	public: void ia32_smsw();
	public: void ia32_lmsw();
	public: void ia32_arpl();
	public: void ia32_lar();
	public: void ia32_lsl();
	public: void ia32_rsm();
	public: void astDefs();
public:
	antlr::RefAST getAST()
	{
		return returnAST;
	}
	
protected:
	antlr::RefAST returnAST;
private:
	static const char* tokenNames[];
#ifndef NO_STATIC_CONSTS
	static const int NUM_TOKENS = 247;
#else
	enum {
		NUM_TOKENS = 247
	};
#endif
	
	static const unsigned long _tokenSet_0_data_[];
	static const antlr::BitSet _tokenSet_0;
	static const unsigned long _tokenSet_1_data_[];
	static const antlr::BitSet _tokenSet_1;
	static const unsigned long _tokenSet_2_data_[];
	static const antlr::BitSet _tokenSet_2;
	static const unsigned long _tokenSet_3_data_[];
	static const antlr::BitSet _tokenSet_3;
	static const unsigned long _tokenSet_4_data_[];
	static const antlr::BitSet _tokenSet_4;
	static const unsigned long _tokenSet_5_data_[];
	static const antlr::BitSet _tokenSet_5;
	static const unsigned long _tokenSet_6_data_[];
	static const antlr::BitSet _tokenSet_6;
	static const unsigned long _tokenSet_7_data_[];
	static const antlr::BitSet _tokenSet_7;
	static const unsigned long _tokenSet_8_data_[];
	static const antlr::BitSet _tokenSet_8;
};

ANTLR_END_NAMESPACE
#endif /*INC_AsmParser_hpp_*/

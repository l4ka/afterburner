#ifndef INC_AsmTreeParser_hpp_
#define INC_AsmTreeParser_hpp_

#line 2 "Asm.g"

    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before antlr generated includes in the header file.
    #include <iostream>

#line 13 "AsmTreeParser.hpp"
#include <antlr/config.hpp>
#include "AsmParserTokenTypes.hpp"
/* $ANTLR 2.7.6 (20060929): "Asm.g" -> "AsmTreeParser.hpp"$ */
#include <antlr/TreeParser.hpp>

#line 9 "Asm.g"

    // Inserted after antlr generated includes in the header file
    // outside any generated namespace specifications

#line 24 "AsmTreeParser.hpp"
ANTLR_BEGIN_NAMESPACE(Asm)
class CUSTOM_API AsmTreeParser : public antlr::TreeParser, public AsmParserTokenTypes
{
#line 429 "Asm.g"

protected:
    unsigned pad;
    unsigned label_cnt, gprof_cnt, gcov_cnt;
    bool     in_macro;
    bool     annotate_sensitive;
    bool     burn_gprof, burn_gcov;

public:
    void init( bool do_annotations, bool do_gprof, bool do_gcov )
    {
        // We need a class init method because C++ constructors are useless.
        pad = 0;
	in_macro = false;
	label_cnt = 0;
	annotate_sensitive = do_annotations;
	burn_gprof = do_gprof;
	burn_gcov = do_gcov;
	gprof_cnt = gcov_cnt = 0;
    }

protected:
    void print( antlr::RefAST a )
        { std::cout << a->getText(); }
    void print( char ch )
        { std::cout << ch; }

    void startSensitive( antlr::RefAST s )
    {
        if( annotate_sensitive )
	{
            // Add a label for the sensitive instruction block.
            if( this->in_macro )
	        std::cout << "9997:" << std::endl;
	    else
                std::cout << ".L_sensitive_" << this->label_cnt << ":" << std::endl;
	}
	// Print the sensitive instruction.
	std::cout << '\t' << s->getText();
    }

    void endSensitive( antlr::RefAST s )
    {
        if( !annotate_sensitive )
	    return;

        unsigned begin = this->label_cnt;
        unsigned end   = this->label_cnt+1;
        this->label_cnt += 2;

        // Add a newline after the instruction's parameters.
        std::cout << std::endl;
        // Add the NOP padding.
        for( unsigned i = 0; i < pad; i++ )
            std::cout << '\t' << "nop" << std::endl;

        // Add the label following the NOP block.
        if( this->in_macro )
            std::cout << "9998:" << std::endl;
	else
            std::cout << ".L_sensitive_" << end << ":" << std::endl;

        // Record the instruction location.
        std::cout << "\t.pushsection\t.afterburn" << std::endl;
        std::cout << "\t.balign\t4" << std::endl;
        if( this->in_macro ) {
            std::cout << "\t.long\t9997b" << std::endl;
            std::cout << "\t.long\t9998b" << std::endl;
        } else {
            std::cout << "\t.long\t.L_sensitive_" << begin << std::endl;
            std::cout << "\t.long\t.L_sensitive_" << end << std::endl;
        }
        std::cout << "\t.popsection" << std::endl;
    }

    void emit_gprof()
    {
        if( !burn_gprof ) {
            std::cout << "\tcall\tmcount" << std::endl;
	    return;
        }

	std::cout << "\t.pushsection\t.burn_prof_counters, \"wa\"" << std::endl;
	std::cout << ".L_burnprof_" << gprof_cnt << ": .long 0" << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << "\t.pushsection\t.burn_prof_addr" << std::endl;
	std::cout << "\t.long .L_burnprof_instr_" << gprof_cnt << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << ".L_burnprof_instr_" << gprof_cnt << ":" << std::endl;
	std::cout << "\taddl\t$1, .L_burnprof_" << gprof_cnt << std::endl;

	gprof_cnt++;
    }

    void emit_gcov()
    {
        if( !burn_gcov ) {
            std::cout << "\tcall\t__gcov_merge_add" << std::endl;
	    return;
	}

    	std::cout << "\t.pushsection\t.burn_cov_counters, \"wa\"" << std::endl;
	std::cout << ".L_burncov_" << gcov_cnt << ": .long 0" << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << "\t.pushsection\t.burn_cov_addr" << std::endl;
	std::cout << "\t.long .L_burncov_instr_" << gcov_cnt << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << ".L_burncov_instr_" << gcov_cnt << ":" << std::endl;
	std::cout << "\taddl\t$1, .L_burncov_" << gcov_cnt << std::endl;

	gcov_cnt++;
    }
#line 28 "AsmTreeParser.hpp"
public:
	AsmTreeParser();
	static void initializeASTFactory( antlr::ASTFactory& factory );
	int getNumTokens() const
	{
		return AsmTreeParser::NUM_TOKENS;
	}
	const char* getTokenName( int type ) const
	{
		if( type > getNumTokens() ) return 0;
		return AsmTreeParser::tokenNames[type];
	}
	const char* const* getTokenNames() const
	{
		return AsmTreeParser::tokenNames;
	}
	public: void asmFile(antlr::RefAST _t);
	public: void asmBlocks(antlr::RefAST _t);
	public: void asmBlock(antlr::RefAST _t);
	public: void asmLabel(antlr::RefAST _t);
	public: void asmStatementLine(antlr::RefAST _t);
	public: void asmStatement(antlr::RefAST _t);
	public: void asmInstrPrefix(antlr::RefAST _t);
	public: void asmInstr(antlr::RefAST _t);
	public: void asmCommand(antlr::RefAST _t);
	public: void asmAssignment(antlr::RefAST _t);
	public: void expr(antlr::RefAST _t);
	public: void asmMacroDef(antlr::RefAST _t);
	public: void commandParams(antlr::RefAST _t);
	public: void dbgCommandParams(antlr::RefAST _t);
	public: void asmMacroDefParams(antlr::RefAST _t);
	public: void instrParams(antlr::RefAST _t);
	public: antlr::RefAST  asmSensitiveInstr(antlr::RefAST _t);
	public: void asmGprof(antlr::RefAST _t);
	public: void commandParam(antlr::RefAST _t);
	public: void instrParam(antlr::RefAST _t);
	public: void regExpression(antlr::RefAST _t);
	public: void regOffsetBase(antlr::RefAST _t);
	public: void primitive(antlr::RefAST _t);
	public: void subexpr(antlr::RefAST _t);
public:
	antlr::RefAST getAST()
	{
		return returnAST;
	}
	
protected:
	antlr::RefAST returnAST;
	antlr::RefAST _retTree;
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
};

ANTLR_END_NAMESPACE
#endif /*INC_AsmTreeParser_hpp_*/

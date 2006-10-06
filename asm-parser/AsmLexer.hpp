#ifndef INC_AsmLexer_hpp_
#define INC_AsmLexer_hpp_

#line 2 "Asm.g"

    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before antlr generated includes in the header file.
    #include <iostream>

#line 13 "AsmLexer.hpp"
#include <antlr/config.hpp>
/* $ANTLR 2.7.6 (20060712): "Asm.g" -> "AsmLexer.hpp"$ */
#include <antlr/CommonToken.hpp>
#include <antlr/InputBuffer.hpp>
#include <antlr/BitSet.hpp>
#include "AsmParserTokenTypes.hpp"
#include <antlr/CharScanner.hpp>
#line 9 "Asm.g"

    // Inserted after antlr generated includes in the header file
    // outside any generated namespace specifications

#line 26 "AsmLexer.hpp"
ANTLR_BEGIN_NAMESPACE(Asm)
class CUSTOM_API AsmLexer : public antlr::CharScanner, public AsmParserTokenTypes
{
#line 323 "Asm.g"

    // Additional methods and members.
#line 30 "AsmLexer.hpp"
private:
	void initLiterals();
public:
	bool getCaseSensitiveLiterals() const
	{
		return true;
	}
public:
	AsmLexer(std::istream& in);
	AsmLexer(antlr::InputBuffer& ib);
	AsmLexer(const antlr::LexerSharedInputState& state);
	antlr::RefToken nextToken();
	public: void mCOMMA(bool _createToken);
	public: void mSEMI(bool _createToken);
	public: void mCOLON(bool _createToken);
	public: void mPERCENT(bool _createToken);
	protected: void mDOT(bool _createToken);
	protected: void mAT(bool _createToken);
	protected: void mHASH(bool _createToken);
	public: void mLPAREN(bool _createToken);
	public: void mRPAREN(bool _createToken);
	public: void mLBRACKET(bool _createToken);
	public: void mRBRACKET(bool _createToken);
	public: void mLCURLY(bool _createToken);
	public: void mRCURLY(bool _createToken);
	public: void mASSIGN(bool _createToken);
	public: void mXOR(bool _createToken);
	public: void mOR(bool _createToken);
	public: void mAND(bool _createToken);
	public: void mNOT(bool _createToken);
	public: void mSHIFTLEFT(bool _createToken);
	public: void mSHIFTRIGHT(bool _createToken);
	public: void mPLUS(bool _createToken);
	public: void mMINUS(bool _createToken);
	public: void mDOLLAR(bool _createToken);
	public: void mSTAR(bool _createToken);
	public: void mDIV(bool _createToken);
	public: void mNewline(bool _createToken);
	public: void mWhitespace(bool _createToken);
	public: void mAsmComment(bool _createToken);
	public: void mComment(bool _createToken);
	protected: void mCCommentBegin(bool _createToken);
	protected: void mCComment(bool _createToken);
	protected: void mCPPComment(bool _createToken);
	protected: void mCCommentEnd(bool _createToken);
	protected: void mLetter(bool _createToken);
	protected: void mDigit(bool _createToken);
	protected: void mName(bool _createToken);
	public: void mString(bool _createToken);
	protected: void mStringEscape(bool _createToken);
	public: void mID(bool _createToken);
	public: void mInt(bool _createToken);
	public: void mHex(bool _createToken);
	public: void mCommand(bool _createToken);
	public: void mReg(bool _createToken);
	public: void mOption(bool _createToken);
private:
	
	static const unsigned long _tokenSet_0_data_[];
	static const antlr::BitSet _tokenSet_0;
	static const unsigned long _tokenSet_1_data_[];
	static const antlr::BitSet _tokenSet_1;
	static const unsigned long _tokenSet_2_data_[];
	static const antlr::BitSet _tokenSet_2;
};

ANTLR_END_NAMESPACE
#endif /*INC_AsmLexer_hpp_*/

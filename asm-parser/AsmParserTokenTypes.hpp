#ifndef INC_AsmParserTokenTypes_hpp_
#define INC_AsmParserTokenTypes_hpp_

ANTLR_BEGIN_NAMESPACE(Asm)
/* $ANTLR 2.7.6 (20060712): "Asm.g" -> "AsmParserTokenTypes.hpp"$ */

#ifndef CUSTOM_API
# define CUSTOM_API
#endif

#ifdef __cplusplus
struct CUSTOM_API AsmParserTokenTypes {
#endif
	enum {
		EOF_ = 1,
		Newline = 4,
		SEMI = 5,
		ID = 6,
		ASSIGN = 7,
		Label = 8,
		LocalLabel = 9,
		COMMA = 10,
		// ".macro" = 11
		// ".endm" = 12
		Command = 13,
		// ".file" = 14
		// ".loc" = 15
		LITERAL_mcount = 16,
		LITERAL___gcov_merge_add = 17,
		String = 18,
		Option = 19,
		Int = 20,
		Hex = 21,
		STAR = 22,
		COLON = 23,
		LPAREN = 24,
		RPAREN = 25,
		Reg = 26,
		RelativeLocation = 27,
		MINUS = 28,
		NOT = 29,
		DIV = 30,
		PERCENT = 31,
		SHIFTLEFT = 32,
		SHIFTRIGHT = 33,
		AND = 34,
		OR = 35,
		XOR = 36,
		PLUS = 37,
		DOLLAR = 38,
		// "%al" = 39
		// "%bl" = 40
		// "%cl" = 41
		// "%dl" = 42
		// "%ah" = 43
		// "%bh" = 44
		// "%ch" = 45
		// "%dh" = 46
		// "%ax" = 47
		// "%bx" = 48
		// "%cx" = 49
		// "%dx" = 50
		// "%si" = 51
		// "%di" = 52
		// "%sp" = 53
		// "%bp" = 54
		// "%eax" = 55
		// "%ebx" = 56
		// "%ecx" = 57
		// "%edx" = 58
		// "%esi" = 59
		// "%edi" = 60
		// "%esp" = 61
		// "%ebp" = 62
		// "%st" = 63
		// "%cs" = 64
		// "%ds" = 65
		// "%es" = 66
		// "%fs" = 67
		// "%gs" = 68
		// "%ss" = 69
		// "%cr0" = 70
		// "%cr2" = 71
		// "%cr3" = 72
		// "%cr4" = 73
		// "%db0" = 74
		// "%db1" = 75
		// "%db2" = 76
		// "%db3" = 77
		// "%db4" = 78
		// "%db5" = 79
		// "%db6" = 80
		// "%db7" = 81
		LITERAL_lock = 82,
		LITERAL_rep = 83,
		LITERAL_repnz = 84,
		LITERAL_call = 85,
		LITERAL_popf = 86,
		LITERAL_popfl = 87,
		LITERAL_popfd = 88,
		LITERAL_pushf = 89,
		LITERAL_pushfl = 90,
		LITERAL_pushfd = 91,
		LITERAL_lgdt = 92,
		LITERAL_lgdtl = 93,
		LITERAL_lgdtd = 94,
		LITERAL_sgdt = 95,
		LITERAL_sgdtl = 96,
		LITERAL_sgdtd = 97,
		LITERAL_lidt = 98,
		LITERAL_lidtl = 99,
		LITERAL_lidtd = 100,
		LITERAL_sidt = 101,
		LITERAL_sidtl = 102,
		LITERAL_sidtd = 103,
		LITERAL_ljmp = 104,
		LITERAL_lds = 105,
		LITERAL_les = 106,
		LITERAL_lfs = 107,
		LITERAL_lgs = 108,
		LITERAL_lss = 109,
		LITERAL_clts = 110,
		LITERAL_hlt = 111,
		LITERAL_cli = 112,
		LITERAL_sti = 113,
		LITERAL_lldt = 114,
		LITERAL_lldtl = 115,
		LITERAL_lldtd = 116,
		LITERAL_sldt = 117,
		LITERAL_sldtl = 118,
		LITERAL_sldtd = 119,
		LITERAL_ltr = 120,
		LITERAL_ltrl = 121,
		LITERAL_ltrd = 122,
		LITERAL_str = 123,
		LITERAL_strl = 124,
		LITERAL_strd = 125,
		LITERAL_inb = 126,
		LITERAL_inw = 127,
		LITERAL_inl = 128,
		LITERAL_outb = 129,
		LITERAL_outw = 130,
		LITERAL_outl = 131,
		LITERAL_invlpg = 132,
		LITERAL_iret = 133,
		LITERAL_iretl = 134,
		LITERAL_iretd = 135,
		LITERAL_lret = 136,
		LITERAL_cpuid = 137,
		LITERAL_wrmsr = 138,
		LITERAL_rdmsr = 139,
		LITERAL_int = 140,
		// "ud2" = 141
		LITERAL_invd = 142,
		LITERAL_wbinvd = 143,
		LITERAL_smsw = 144,
		LITERAL_smswl = 145,
		LITERAL_smswd = 146,
		LITERAL_lmsw = 147,
		LITERAL_lmswl = 148,
		LITERAL_lmswd = 149,
		LITERAL_arpl = 150,
		LITERAL_lar = 151,
		LITERAL_lsl = 152,
		LITERAL_rsm = 153,
		LITERAL_pop = 154,
		LITERAL_popl = 155,
		LITERAL_popd = 156,
		LITERAL_popb = 157,
		LITERAL_popw = 158,
		LITERAL_push = 159,
		LITERAL_pushl = 160,
		LITERAL_pushd = 161,
		LITERAL_pushb = 162,
		LITERAL_pushw = 163,
		LITERAL_mov = 164,
		LITERAL_movl = 165,
		LITERAL_movd = 166,
		LITERAL_movb = 167,
		LITERAL_movw = 168,
		ASTMacroDef = 169,
		ASTMacroDefParams = 170,
		ASTInstruction = 171,
		ASTInstructionPrefix = 172,
		ASTSensitive = 173,
		ASTBasicBlock = 174,
		ASTCommand = 175,
		ASTDbgCommand = 176,
		ASTDefaultParam = 177,
		ASTRegisterDisplacement = 178,
		ASTRegisterBaseIndexScale = 179,
		ASTRegisterIndex = 180,
		ASTDereference = 181,
		ASTSegment = 182,
		ASTRegister = 183,
		ASTNegative = 184,
		ASTGprof = 185,
		ASTGcov = 186,
		DOT = 187,
		AT = 188,
		HASH = 189,
		LBRACKET = 190,
		RBRACKET = 191,
		LCURLY = 192,
		RCURLY = 193,
		Whitespace = 194,
		AsmComment = 195,
		Comment = 196,
		CCommentBegin = 197,
		CCommentEnd = 198,
		CComment = 199,
		CPPComment = 200,
		Letter = 201,
		Digit = 202,
		Name = 203,
		StringEscape = 204,
		IA32_call = 205,
		IA32_popf = 206,
		IA32_pushf = 207,
		IA32_lgdt = 208,
		IA32_sgdt = 209,
		IA32_lidt = 210,
		IA32_sidt = 211,
		IA32_ljmp = 212,
		IA32_lds = 213,
		IA32_les = 214,
		IA32_lfs = 215,
		IA32_lgs = 216,
		IA32_lss = 217,
		IA32_clts = 218,
		IA32_hlt = 219,
		IA32_cli = 220,
		IA32_sti = 221,
		IA32_lldt = 222,
		IA32_sldt = 223,
		IA32_ltr = 224,
		IA32_str = 225,
		IA32_in = 226,
		IA32_out = 227,
		IA32_invlpg = 228,
		IA32_iret = 229,
		IA32_lret = 230,
		IA32_cpuid = 231,
		IA32_wrmsr = 232,
		IA32_rdmsr = 233,
		IA32_int = 234,
		IA32_ud2 = 235,
		IA32_invd = 236,
		IA32_wbinvd = 237,
		IA32_smsw = 238,
		IA32_lmsw = 239,
		IA32_arpl = 240,
		IA32_lar = 241,
		IA32_lsl = 242,
		IA32_rsm = 243,
		IA32_pop = 244,
		IA32_push = 245,
		IA32_mov = 246,
		NULL_TREE_LOOKAHEAD = 3
	};
#ifdef __cplusplus
};
#endif
ANTLR_END_NAMESPACE
#endif /*INC_AsmParserTokenTypes_hpp_*/

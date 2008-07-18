// This file defines the grammar for all architectures.
// The code of different architectures is intermingled and selected by the
// preprocessor using ARCH_... macros. This is not a clean separation of
// code, and the file might grow inacceptably due to that. On the other
// hand, there seems to be sufficient overlap in the asm syntax of different
// architectures to warrant the trouble.
// Note that "cross-afterburning" is explicitly NOT an argument against
// this code structure. The afterburner is a small, self-contained program,
// it can be built multiple times for different architectures.
// Using the antlr grammar inheritance mechanism could be another
// possibility.
//
// Note that the grammar treats almost any reasonable string as instruction
// name if necessary, only filtering out special ones.
//
// This file is preprocessed with the normal C preprocessor. Be careful not
// to cause any troubles in this stage. Use the PREPROCESS_FIRST_MASK macro
// to hide directives from this first preprocessing stage. Also note that
// tree construction commands re-use the # sign, so be especially careful
// here. Normally things should work out fine if you use sane names.

header "pre_include_hpp" {
    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before antlr generated includes in the header file.
    PREPROCESS_FIRST_MASK(#include <iostream>)
}
header "post_include_hpp" {
    // Inserted after antlr generated includes in the header file
    // outside any generated namespace specifications
}
header "pre_include_cpp" {
    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    // Inserted before the antlr generated includes in the cpp file
}
header "post_include_cpp" {
    // Inserted after the antlr generated includes in the cpp file
}

options {
    language = "Cpp";
    namespace = "Asm";
    namespaceStd = "std";
    namespaceAntlr = "antlr";
    genHashLines = true;
}

{
#if !defined (ARCH_amd64) && !defined (ARCH_ia32)
#error "Not ported to this architecture!"
#endif
    // Global stuff in the cpp file.
}

class AsmParser extends Parser;
options {
    buildAST = true;
    k = 2;
    defaultErrorHandler = false;
}
{
    // Additional methods and members.
}

// Rules
asmFile: asmBlocks ;

asmBlocks: (asmAnonymousBlock)? (asmBasicBlock)* ;

asmAnonymousBlock: (asmStatement)+
    { ## = #([ASTBasicBlock, "basic block"], ##); }
    ;

asmBasicBlock: (asmLabel)+ (asmStatement)+ 
    { ## = #([ASTBasicBlock, "basic block"], ##); }
    ;

asmEnd: Newline! | SEMI! ;

asmStatement
    : asmInstrPrefix (asmInstr)? asmEnd
    | asmInstr asmEnd
    | asmCommand asmEnd
    | asmEnd
    | asmAssignment asmEnd
    ;

asmAssignment : ID ASSIGN^ commandParam ;
asmLabel : Label | LocalLabel ;

asmMacroDefParams
    : (ID (COMMA! ID)*)? 
      { ## = #([ASTMacroDefParams, "macro parameters"], ##); }
    ;

asmMacroDef
    : ".macro"! ID asmMacroDefParams asmEnd!
               asmBlocks
      ".endm"!
      { ## = #([ASTMacroDef, "macro definition"], ##); }
    ;

asmCommand
    : asmMacroDef
    | Command commandParams	{ ## = #([ASTCommand, "command"], ##); }
    | f:".file" simpleParams	{ #f->setType(Command); 
    				  ## = #([ASTDbgCommand, "command"], ##); }
    | l:".loc" simpleParams	{ #l->setType(Command);
    				  ## = #([ASTDbgCommand, "command"], ##); }
    ;

asmInstr { bool sensitive=false; }
    : asmGprof 
    | ( asmInnocuousInstr (sensitive=instrParams)?  
      | asmSensitiveInstr (instrParams)? { sensitive=true; } 
      )
      {
        if( sensitive )
	    ## = #([ASTSensitive, "sensitive instruction"], ##);
        else
	    ## = #([ASTInstruction, "instruction"], ##);
      }
    ;

asmGprof :
#if defined (ARCH_ia32) || defined (ARCH_amd64)
      ia32_call m:"mcount"		{ ## = #([ASTGprof, "gprof"], ##); }
    | ia32_call a:"__gcov_merge_add"	{ ## = #([ASTGcov, "gprof"], ##); }
#endif
    ;

simpleParams: (simpleParam)* ;
simpleParam: String | Option | Int | Hex | Command;

commandParams
    // Some commands have optional parameters anywhere in the parameter
    // list, and use the commas to position the specified parameters.
    // Example: .p2align 4,,15
    : commandParam (COMMA! (commandParam | defaultParam) )* 
    | defaultParam (COMMA! (commandParam | defaultParam) )+
    | ! /* nothing */
    ;
defaultParam: /* nothing */ { ## = #[ASTDefaultParam, "default"]; } ;
commandParam: String | Option | instrParam;

instrParams returns [bool s] { s=false; bool t; }
    : s=instrParam (COMMA! t=instrParam { s|=t; })* ;
instrParam returns [bool s] { s=false; } : s=regExpression ;

regExpression returns [bool s] { s=false; } : s=regDereferenceExpr ;

regDereferenceExpr returns [bool sensitive] { sensitive=false; }
    : (s:STAR^			{#s->setType(ASTDereference);} )?
      sensitive=regSegmentExpr
    ;

// XXX These are (somewhat) x86 specific.
regSegmentExpr returns [bool s] { s=false; }
    : (asmSegReg c:COLON^	{#c->setType(ASTSegment);} )?
      s=regDisplacementExpr
    ;

regDisplacementExpr returns [bool s] { s=false; }
    // section:disp(base, index, scale)  
    // where section, base, and index are registers.
    : s=expression 
      ((regOffsetBase
          {## = #([ASTRegisterDisplacement, "register displacement"], ##);}
       )?
#ifdef ARCH_amd64
      | regRipRel {## = #([ASTRipRelative, "rip relative"], ##);}
#endif
      )
    ;

regRipRel : LPAREN! "%rip"! RPAREN!;

regOffsetBase
    : (  LPAREN! defaultParam 
               COMMA!  (asmReg | defaultParam) 
	       COMMA!  (Int | defaultParam) RPAREN!
       | LPAREN! asmReg 
               (COMMA! (asmReg | defaultParam) 
	       (COMMA! (Int | defaultParam))? )? RPAREN!
       ) {## = #([ASTRegisterBaseIndexScale, "register base index scale"], ##);}
    ;
// End architecture specific.

primitive returns [bool s] { s=false; }
    : ID | Int | Hex | Command | asmReg | Reg | RelativeLocation
    | (asmSegReg | asmSensitiveReg) { s=true; }
    | (asmFpReg LPAREN) => asmFpReg LPAREN! Int RPAREN! 
          { ## = #([ASTRegisterIndex, "register index"], ##);}
    | asmFpReg
    | (LPAREN (asmReg|COMMA)) => regOffsetBase
    | LPAREN! s=expression RPAREN!
     // The user can use IDs that overlap with the instruction name space
    | asmInstrPrefixTokens
    ;

signExpression returns [bool s] { s=false; }
    : (m:MINUS^ {#m->setType(ASTNegative);})? s=primitive
    ;
notExpression returns [bool s] { s=false; }
    : (NOT^)? s=signExpression
    ;
multiplyingExpression returns [bool s] { s=false; bool t; }
    : s=notExpression
        ((STAR^|DIV^|PERCENT^) t=notExpression { s|=t; })*
    ;
shiftingExpression returns [bool s] { s=false; bool t; }
    : s=multiplyingExpression
        ((SHIFTLEFT^ | SHIFTRIGHT^) t=multiplyingExpression { s|=t; })*
    ;
bitwiseExpression returns [bool s] { s=false; bool t; }
    : s=shiftingExpression ((AND^|OR^|XOR^) t=shiftingExpression { s|=t; })*
    ;
addingExpression returns [bool s] { s=false; bool t; }
    : s=bitwiseExpression 
        ((PLUS^|MINUS^) t=bitwiseExpression { s|=t; })*
    ;
makeConstantExpression returns [bool s] { s=false; }
    : (DOLLAR^)? s=addingExpression
    ;

expression returns [bool s] { s=false; } : s=makeConstantExpression ;

asmReg
    : (
#if defined (ARCH_ia32) || defined (ARCH_amd64)
        "%al" | "%bl" | "%cl" | "%dl"
      | "%ah" | "%bh" | "%ch" | "%dh"
      | "%ax" | "%bx" | "%cx" | "%dx" | "%si" | "%di" | "%sp" | "%bp"
      | "%eax" | "%ebx" | "%ecx" | "%edx" | "%esi" | "%edi" | "%esp" | "%ebp"
#endif
#ifdef ARCH_amd64
      | "%rax" | "%rbx" | "%rcx" | "%rdx" | "%rsi" | "%rdi" | "%rsp" | "%rbp"
      | "%sil" | "%dil" | "%bpl" | "%spl"
      | "%r8"  | "%r9"  | "%r10" | "%r11" | "%r12" | "%r13" | "%r14" | "%r15"
      | "%r8b"  | "%r9b"  | "%r10b" | "%r11b"
      | "%r12b" | "%r13b" | "%r14b" | "%r15b"
      | "%r8w"  | "%r9w"  | "%r10w" | "%r11w"
      | "%r12w" | "%r13w" | "%r14w" | "%r15w"
      | "%r8d"  | "%r9d"  | "%r10d" | "%r11d"
      | "%r12d" | "%r13d" | "%r14d" | "%r15d"
#endif
      )
        { ##->setType(ASTRegister); }
    ;

// XXX These are (somewhat) x86 specific.
asmFpReg : "%st" { ##->setType(ASTRegister); };
asmSegReg
    : ("%cs" | "%ds" | "%es" | "%fs" | "%gs" | "%ss")
      { ##->setType(ASTRegister); }
    ;
// End architecture specific.

asmSensitiveReg
    : (
#if defined (ARCH_ia32) || defined (ARCH_amd64)
        "%cr0" | "%cr2" | "%cr3" | "%cr4" 
      | "%db0" | "%db1" | "%db2" | "%db3" | "%db4" | "%db5" | "%db6" | "%db7"
#endif
#ifdef ARCH_amd64
      | "%cr8" // task priority register
      | "%db8" | "%db9" | "%db10" | "%db11" | "%db12"
      | "%db13" | "%db14" | "%db15"
#endif
      )
      { ##->setType(ASTRegister); }
    ;

asmInstrPrefixTokens
    : ("lock" | "rep" | "repnz")
      { ##->setType(ID); }
    ;
asmInstrPrefix
    : asmInstrPrefixTokens 
      { ## = #([ASTInstructionPrefix, "prefix"], ##); } 
    ;

asmInnocuousInstr: ID
#if defined (ARCH_amd64) || defined (ARCH_ia32)
    | ia32_pop | ia32_push | ia32_mov | ia32_call
#endif
    ;

// We want instruction tokens, but via the string hash table, so 
// build the tokens manually.  Recall that tokens start with a capital letter.
#if defined (ARCH_ia32) || defined (ARCH_amd64)
ia32_call : ("call"
#ifdef ARCH_amd64
	    | "callq"
#endif
	    )					{ ##->setType(IA32_call); } ;

ia32_popf : ( "popf"  | "popfl"  | "popfd"
#ifdef ARCH_amd64
            | "popfq"
#endif
            )	{ ##->setType(IA32_popf); } ;

ia32_pushf: ( "pushf" | "pushfl" | "pushfd"
#ifdef ARCH_amd64
            | "pushfq"
#endif
            )	{ ##->setType(IA32_pushf); } ;

ia32_lgdt : ("lgdt"  | "lgdtl"  | "lgdtd")	{ ##->setType(IA32_lgdt); } ;
ia32_sgdt : ("sgdt"  | "sgdtl"  | "sgdtd")	{ ##->setType(IA32_sgdt); } ;
ia32_lidt : ("lidt"  | "lidtl"  | "lidtd")	{ ##->setType(IA32_lidt); } ;
ia32_sidt : ("sidt"  | "sidtl"  | "sidtd")	{ ##->setType(IA32_sidt); } ;
ia32_ljmp : ("ljmp")				{ ##->setType(IA32_ljmp); } ;
ia32_lds  : ("lds")				{ ##->setType(IA32_lds); } ;
ia32_les  : ("les")				{ ##->setType(IA32_les); } ;
ia32_lfs  : ("lfs")				{ ##->setType(IA32_lfs); } ;
ia32_lgs  : ("lgs")				{ ##->setType(IA32_lgs); } ;
ia32_lss  : ("lss")				{ ##->setType(IA32_lss); } ;
ia32_clts : ("clts")				{ ##->setType(IA32_clts); } ;
ia32_hlt  : ("hlt")				{ ##->setType(IA32_hlt); } ;
ia32_cli  : ("cli")				{ ##->setType(IA32_cli); } ;
ia32_sti  : ("sti")				{ ##->setType(IA32_sti); } ;
ia32_lldt : ("lldt" | "lldtl" | "lldtd")	{ ##->setType(IA32_lldt); } ;
ia32_sldt : ("sldt" | "sldtl" | "sldtd" )	{ ##->setType(IA32_sldt); } ;
ia32_ltr  : ("ltr"  | "ltrl" | "ltrd" )		{ ##->setType(IA32_ltr); } ;
ia32_str  : ("str"  | "strl" | "strd" )		{ ##->setType(IA32_str); } ;

ia32_in   : ( "inb"  | "inw"  | "inl" | "ins" | "insw"
#ifdef ARCH_amd64
            | "inq"
#endif
            )		{ ##->setType(IA32_in); } ;

ia32_out  : ( "outb" | "outw" | "outl" | "outs" | "outsw"
#ifdef ARCH_amd64
            | "outq"
#endif
	    )		{ ##->setType(IA32_out); } ;

ia32_invlpg : ("invlpg")			{ ##->setType(IA32_invlpg); } ;
ia32_iret : ("iret" | "iretl" | "iretd")	{ ##->setType(IA32_iret); } ;
ia32_lret : ("lret")				{ ##->setType(IA32_lret); } ;
ia32_cpuid : ("cpuid")				{ ##->setType(IA32_cpuid); } ;
ia32_wrmsr : ("wrmsr")				{ ##->setType(IA32_wrmsr); } ;
ia32_rdmsr : ("rdmsr")				{ ##->setType(IA32_rdmsr); } ;
ia32_int   : ("int")				{ ##->setType(IA32_int); } ;
ia32_ud2   : ("ud2")				{ ##->setType(IA32_ud2); } ;
ia32_invd  : ("invd")				{ ##->setType(IA32_invd); } ;
ia32_wbinvd : ("wbinvd")			{ ##->setType(IA32_wbinvd); } ;
ia32_smsw  : ("smsw" | "smswl" | "smswd")	{ ##->setType(IA32_smsw); } ;
ia32_lmsw  : ("lmsw" | "lmswl" | "lmswd")	{ ##->setType(IA32_lmsw); } ;
ia32_arpl  : ("arpl")				{ ##->setType(IA32_arpl); } ;
ia32_lar   : ("lar")				{ ##->setType(IA32_lar); } ;
ia32_lsl   : ("lsl")				{ ##->setType(IA32_lsl); } ;
ia32_rsm   : ("rsm")				{ ##->setType(IA32_rsm); } ;

ia32_pop : ( "pop"  | "popl"  | "popd" | "popb" | "popw"
#ifdef ARCH_amd64
           | "popq"
#endif
           ) { ##->setType(IA32_pop); };

ia32_push : ( "push" | "pushl" | "pushd" | "pushb" | "pushw"
#ifdef ARCH_amd64
            | "pushq"
#endif
	    ) { ##->setType(IA32_push); } ;

ia32_mov : ( "mov"  | "movl"  | "movd" | "movb" | "movw"
#ifdef ARCH_amd64
           | "movq"
#endif
	   ) { ##->setType(IA32_mov); } ;
#endif

/* XXX TODO swapgs? fxsave/fxrstor? */

asmSensitiveInstr :
#if defined (ARCH_ia32) || defined (ARCH_amd64)
      ia32_popf | ia32_pushf | ia32_lgdt | ia32_sgdt | ia32_lidt | ia32_sidt
    | ia32_ljmp | ia32_lds | ia32_les | ia32_lfs | ia32_lgs | ia32_lss
    | ia32_clts | ia32_hlt | ia32_cli | ia32_sti | ia32_lldt | ia32_sldt
    | ia32_ltr  | ia32_str | ia32_in  | ia32_out
    | ia32_invlpg | ia32_iret | ia32_lret | ia32_cpuid 
    | ia32_wrmsr | ia32_rdmsr | ia32_int | ia32_ud2 | ia32_invd | ia32_wbinvd
    | ia32_smsw | ia32_lmsw | ia32_arpl | ia32_lar | ia32_lsl | ia32_rsm
#endif
    ;

astDefs
    : ASTMacroDef
    | ASTMacroDefParams
    | ASTInstruction
    | ASTInstructionPrefix
    | ASTSensitive
    | ASTBasicBlock
    | ASTCommand
    | ASTDbgCommand
    | ASTDefaultParam
    | ASTRegisterDisplacement
    | ASTRegisterBaseIndexScale
    | ASTRegisterIndex
    | ASTDereference
    | ASTSegment
    | ASTRegister
    | ASTNegative
    | ASTGprof
    | ASTGcov
#ifdef ARCH_amd64
    | ASTRipRelative
#endif
    ;


{
    // Global stuff in the cpp file.
}

class AsmLexer extends Lexer;
options {
    k = 3;
    caseSensitive = true;
    testLiterals = false;
    // Specify the vocabulary, to support inversions in a scanner rule.
    charVocabulary = '\u0000'..'\u00FF';
    defaultErrorHandler = false;
}

{
    // Additional methods and members.
}

// Rules
COMMA	: ',' ;
SEMI	: ';' ;
COLON   : ':' ;
PERCENT : '%' ;
protected DOT     : '.' ;
protected AT      : '@' ;
protected HASH	  : '#' ;

LPAREN		: '(' ;
RPAREN		: ')' ;
LBRACKET	: '[' ;
RBRACKET	: ']' ;
LCURLY		: '{' ;
RCURLY		: '}' ;
ASSIGN		: '=' ;

XOR		: '^' ;
OR		: '|' ;
AND		: '&' ;
NOT		: '~' ;
SHIFTLEFT	: '<''<' ;
SHIFTRIGHT	: '>''>' ;

PLUS		: '+' ;
MINUS		: '-' ;
DOLLAR		: '$' ;
STAR		: '*' ;
DIV		: '/' ;

Newline
    : '\r' '\n'	{ newline(); } // DOS
    | '\n'	{ newline(); } // Sane systems
    ;

Whitespace
    : (' ' | '\t' | '\014')
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;

/*
Preprocessor
    // #APP and #NO_APP surround inlined assembler.
    : h:HASH { (h->getColumn() == 1) }? (~('\n'))*
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;
*/

/* XXX This is (somewhat) architecture specific. */
AsmComment
    : '#' ( ~('\n') )* 	// Don't swallow the newline.
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;
// End architecture specific.

Comment
    : ((CCommentBegin) => CComment | CPPComment)
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;
protected CCommentBegin : '/''*';
protected CCommentEnd   : '*''/' ;
protected CComment
    : CCommentBegin (options {greedy=false;}
                     : '\n' { newline(); } | ~('\n') )* 
      CCommentEnd ;
protected CPPComment: '/''/' ( ~('\n') )* ;	// Don't swallow the newline.

protected Letter : 'a'..'z' | 'A'..'Z' | '_';
protected Digit  : '0'..'9';

protected Name   : Letter (Letter | Digit | DOT)* ;
String : '"' ( StringEscape | ~('\\'|'"') )* '"' ;
protected StringEscape : '\\' . ;

// Note: For all literals that we wish to lookup in the hash table, there
// must be a Lexer rule that can match it, with the testLiterals option
// enabled.

// Note: We use left factoring for picking out labels amongst the 
// IDs and commands.
ID options {testLiterals=true;}
    : Name (COLON {$setType(Label);})? ;

Int : (Digit)+ (COLON {$setType(Label);} |
                ('f'|'b' {$setType(RelativeLocation);}))?
    ;
Hex : '0' 'x' (Digit | 'a'..'f' | 'A'..'F')+ (COLON {$setType(Label);})? ;

Command options {testLiterals=true;}
    : DOT (Letter | Digit | DOT)* (COLON {$setType(LocalLabel);})? ;

Reg options {testLiterals=true;}
    : PERCENT Name ;

Option options {testLiterals=true;}
    : AT Name ;

{
    // Global stuff in the cpp file.
}

class AsmTreeParser extends TreeParser;
options {
    defaultErrorHandler = false;
}
{
protected:
    unsigned pad;
    unsigned label_cnt, gprof_cnt, gcov_cnt;
    bool     in_macro;
    bool     prefixed;
    bool     annotate_sensitive;
    bool     burn_gprof, burn_gcov;

    const char* datastr;
    const char* section;
    std::string prefix;
    unsigned align;

public:
    void init( bool do_annotations, bool do_gprof, bool do_gcov, bool m32)
    {
        // We need a class init method because C++ constructors are useless.
        pad = 0;
	in_macro = false;
	prefixed = false;	
	label_cnt = 0;
	annotate_sensitive = do_annotations;
	burn_gprof = do_gprof;
	burn_gcov = do_gcov;
	gprof_cnt = gcov_cnt = 0;

#if defined (ARCH_ia32)
	datastr = ".long";
	section = ".afterburn";
	align = 4;
#endif
#if defined (ARCH_amd64)
	if (m32)
	{
	    datastr = ".long";
	    section = ".afterburn32";
	    align = 4;
	}
	else
	{
	    datastr = ".quad";
	    section = ".afterburn";
	    align = 8;
	}
#endif
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
	std::cout << '\t';
	if (this->prefixed)
	{
	   std::cout << this->prefix;
	   std::cout << ';' ;
	}
	this->prefixed = false;
	std::cout << s->getText();
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
        std::cout << "\t.pushsection\t" << section << std::endl;
        std::cout << "\t.balign\t" << align << std::endl;
        if( this->in_macro ) {
            std::cout << "\t" << datastr << "\t9997b" << std::endl;
            std::cout << "\t" << datastr << "\t9998b" << std::endl;
        } else {
            std::cout << "\t" << datastr << "\t.L_sensitive_" << begin << std::endl;
            std::cout << "\t" << datastr << "\t.L_sensitive_" << end << std::endl;
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
	std::cout << ".L_burnprof_" << gprof_cnt << ": " << datastr << " 0" << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << "\t.pushsection\t.burn_prof_addr" << std::endl;
	std::cout << "\t" << datastr << " .L_burnprof_instr_" << gprof_cnt << std::endl;
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
	std::cout << ".L_burncov_" << gcov_cnt << ": " << datastr << " 0" << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << "\t.pushsection\t.burn_cov_addr" << std::endl;
	std::cout << "\t" << datastr << " .L_burncov_instr_" << gcov_cnt << std::endl;
        std::cout << "\t.popsection" << std::endl;

	std::cout << ".L_burncov_instr_" << gcov_cnt << ":" << std::endl;
	std::cout << "\taddl\t$1, .L_burncov_" << gcov_cnt << std::endl;

	gcov_cnt++;
    }
}

asmFile: asmBlocks;

asmBlocks: (asmBlock)* ;

asmBlock: #( ASTBasicBlock (asmLabel)*  (asmStatementLine)* );

asmLabel
    : l:Label 		{ std::cout << l->getText() << std::endl; }
    | ll:LocalLabel 	{ std::cout << ll->getText() << std::endl; }
    ;

asmStatementLine: asmStatement { std::cout << std::endl; } ;

asmStatement
    : (asmInstrPrefix)? asmInstr
    | asmCommand
    | asmAssignment
    ;

asmAssignment
    : #(ASSIGN l:.  { std::cout << l->getText() << '='; } expr) 
    ;

asmCommand
    : asmMacroDef
    | #(ASTCommand c:Command
	    {
	      if( c->getText() == ".afterburn_noannotate" )
	        annotate_sensitive = false;
	      else if( c->getText() == ".afterburn_annotate" )
	        annotate_sensitive = true;
              else
	        std::cout << '\t' << c->getText();
            }
        (commandParams)?
       )
    | #(ASTDbgCommand d:Command	{ std::cout << '\t' << d->getText(); }
        (dbgCommandParams)?
       )
    ;

asmMacroDef
    : #( ASTMacroDef i:ID	{ std::cout << ".macro " << i->getText(); }
         asmMacroDefParams	{ std::cout << std::endl; }
	 { this->in_macro=true; } 
	 asmBlocks 		
	 { this->in_macro=false; }
	 			{ std::cout << ".endm" << std::endl; }
       )
    ;
asmMacroDefParams
    : #( ASTMacroDefParams (
            (i:ID		{ std::cout << ' ' << i->getText();   } )
            (ii:ID		{ std::cout << ", " << ii->getText(); } )*
        )? ) 
    ;

asmInstrPrefix
    : #( ASTInstructionPrefix i:.	{this->prefixed = true; this->prefix = i->getText() ;}
       );

asmInstr {antlr::RefAST r;}
    : #( ASTInstruction i:. { 
			std::cout << '\t';
			if( this->prefixed ) 
			{
			   std::cout << this->prefix;
			   std::cout << ';' ;
			}
			std::cout << i->getText();
			this->prefixed = false;
			}
         (instrParams)? )
    | #( ASTSensitive r=asmSensitiveInstr  { startSensitive(r); }
         (instrParams)? { endSensitive(r); } )
    | asmGprof
    ;

asmGprof
    : #( ASTGprof IA32_call "mcount" )			{ emit_gprof(); }
    | #( ASTGcov IA32_call "__gcov_merge_add" )		{ emit_gcov();  }
    ;

commandParams
    : { std::cout << '\t'; } commandParam
      ({ std::cout << ',' << ' '; } commandParam)*
    ;
dbgCommandParams
    : { std::cout << '\t'; } commandParam
      ({ std::cout << ' '; } commandParam)*
    ;
commandParam
    : s:String		{ print(s); }
    | o:Option 		{ print(o); }
    | ASTDefaultParam
    | instrParam
    ;

instrParams
    : { std::cout << '\t'; } instrParam 
      ({ std::cout << ',' << ' '; } instrParam)*
    ;
instrParam
    : regExpression
    ;

regExpression
    // section:disp(base, index, scale)  
    // where section, base, and index are registers.
    : expr
    ;

// XXX This is (somewhat) x86 specific.
regOffsetBase
    : { std::cout << '('; }
      (ASTDefaultParam | r1:ASTRegister {print(r1);}) 
      ({ std::cout << ','; } (ASTDefaultParam | r2:ASTRegister {print(r2);}) 
       ({ std::cout << ','; } (i:Int {print(i);} | ASTDefaultParam))?
      )? 
      { std::cout << ')'; }
    ;
// End architecture specific.

primitive
    : i:ID 		{ print(i); }
    | n:Int 		{ print(n); }
    | h:Hex		{ print(h); }
    | c:Command		{ print(c); }
    | r:ASTRegister	{ print(r); }
    | rl:RelativeLocation { print(rl); }
    ;

subexpr
    : (primitive) => expr
    | { print('('); }    expr    { print(')'); }
    ;

expr
    : #(p:PLUS  expr ({ print(p); } expr)+)
    | #(m:MINUS expr ({ print(m); } expr)+)
    | #(o:OR   subexpr ({ print(o); } subexpr)+)
    | #(n:NOT {print(n);} subexpr)
    | #(ASTNegative { print('-'); } subexpr)
    | #(s:STAR   subexpr ({ print(s); } subexpr)+)
    | #(d:DIV    subexpr ({ print(d); } subexpr)+)
    | #(a:AND    subexpr ({ print(a); } subexpr)+)
    | #(x:XOR    subexpr ({ print(x); } subexpr)+)
    | #(sl:SHIFTLEFT  subexpr ({ print(sl); } subexpr)+ )
    | #(sr:SHIFTRIGHT subexpr ({ print(sr); } subexpr)+ )
    | #(p2:PERCENT  subexpr ({ print(p2); } subexpr)+)
    | #(D:DOLLAR { print(D); } subexpr)
    | primitive
    | #(ASTDereference { std::cout << '*'; } expr)
    | #(ASTSegment r:ASTRegister { std::cout << r->getText() << ':'; } expr)
    | #(ASTRegisterDisplacement subexpr expr)
    | #(ASTRegisterBaseIndexScale regOffsetBase)
    | #(ASTRegisterIndex ri:ASTRegister in:Int 
             { std::cout << ri->getText() << '(' << in->getText() << ')'; } )
#ifdef ARCH_amd64
    | #(ASTRipRelative subexpr) { std::cout << "(%rip)"; }
#endif
    ;

asmSensitiveInstr returns [antlr::RefAST r] { pad=8; r=_t; } :
#if defined (ARCH_ia32) || defined (ARCH_amd64)
    // XXX in amd64 mode, use different padding as necessary
      IA32_popf		{pad=21;}
#ifdef ARCH_ia32
    | IA32_pushf	{pad=5;}
#else
    | IA32_pushf	{pad=6;}
#endif
    | IA32_lgdt		{pad=9;}
    | IA32_sgdt
    | IA32_lidt		{pad=9;}
    | IA32_sidt
    | IA32_ljmp		{pad=9;}
    | IA32_lds		{pad=16;}
    | IA32_les		{pad=16;}
    | IA32_lfs		{pad=16;}
    | IA32_lgs		{pad=16;}
    | IA32_lss		{pad=16;}
    | IA32_clts		{pad=14;}
    | IA32_hlt		{pad=6;}
#ifdef ARCH_ia32
    | IA32_cli		{pad=7;}
#else
    | IA32_cli
#endif
    | IA32_sti		{pad=23;}
    | IA32_lldt		{pad=16;}
    | IA32_sldt		{pad=6;}
    | IA32_ltr		{pad=16;}
    | IA32_str		{pad=9;}
#ifdef ARCH_ia32
    | IA32_in		{pad=13;}
#else
    | IA32_in		{pad=14;}
#endif
    | IA32_out		{pad=16;}
    | IA32_invlpg 	{pad=6;}
    | IA32_iret		{pad=4;}
    | IA32_lret		{pad=4;}
    | IA32_cpuid
    | IA32_wrmsr
    | IA32_rdmsr
    | IA32_int		{pad=11;}
    | IA32_ud2
    | IA32_invd
    | IA32_wbinvd
    | IA32_smsw
    | IA32_lmsw
    | IA32_arpl
    | IA32_lar
    | IA32_lsl
    | IA32_rsm
    | IA32_pop		{pad=5;}
    | IA32_push		{pad=5;}
    | IA32_mov		{pad=12;}
#endif
    ;



header "pre_include_hpp" {
    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.

    #include <iostream>
}

header "pre_include_cpp" {
    // Copyright 2006 University of Karlsruhe.  See LICENSE.txt
    // for licensing information.
}

options {
    language = "Cpp";
    namespace = "VmiCalls";
    namespaceStd = "std";
    namespaceAntlr = "antlr";
    genHashLines = true;
}

class VmiCallsParser extends Parser;
options {
    buildAST = true;
    k = 3;
}

vmiCallsFile
    : "#define"! "VMI_CALLS"! (vmiDef)*
	{ ## = #([ASTVmiCalls, "vmi calls"], ##); }
    ;

vmiDef
    : vmi_space
    | vmi_func
    | vmi_proc
    | vmi_jump
    ;

vmi_space
    : "VMI_SPACE"! LPAREN! Name RPAREN! 
	{ ## = #([ASTVmiSpace, "vmi space"], ##); }
    ;

vmi_func
    : "VMI_FUNC"! LPAREN! Name COMMA! vmiReturnVal COMMA! vmiArgs 
      COMMA! vmiOutputs COMMA! vmiInputs
      COMMA! vmiClobber COMMA! vmiAttributes
      RPAREN!
	{ ## = #([ASTVmiFunc, "vmi func"], ##); }
    ;

vmi_jump
    : "VMI_JUMP"! LPAREN! Name COMMA! vmiArgs
      COMMA! vmiOutputs COMMA! vmiInputs
      COMMA! vmiClobber COMMA! vmiAttributes
      RPAREN!
	{ ## = #([ASTVmiJump, "vmi jump"], ##); }
    ;

vmi_proc
    : "VMI_PROC"! LPAREN! Name COMMA! vmiArgs
      COMMA! vmiOutputs COMMA! vmiInputs
      COMMA! vmiClobber COMMA! vmiAttributes
      RPAREN!
	{ ## = #([ASTVmiProc, "vmi proc"], ##); }
    ;

vmiReturnVal: vmiType;

vmiClobber
    : "VMI_ASM_CLOBBER"		{ ##->setType(ASTVmiAsmClobber); }
    | "VMI_MEM_CLOBBER"		{ ##->setType(ASTVmiMemClobber); }
    ;

vmiAttributes
    : "VMI_ATTRIBUTES"! LPAREN! (vmiAttributeList)? RPAREN!
	{ ## = #([ASTVmiAttributes, "vmi attributes"], ##); }
    ;
vmiAttributeList: vmiAttribute (COMMA! vmiAttribute)* ;
vmiAttribute
    : "VMI_NOT_FLAT" 		{ ##->setType(ASTVmiNotFlat); }
    | "VMI_FLAT" 		{ ##->setType(ASTVmiFlat); }
    | "VMI_VOLATILE" 		{ ##->setType(ASTVmiVolatile); }
    | "VMI_FLAT_SS"		{ ##->setType(ASTVmiFlatSS); }
    ;

vmiType
    : ("VMI_DTR"
      | "VMI_SELECTOR"
      | "VMI_PTE"
      | "VMI_PAE_PTE"
      | "VMI_BOOL"
      | "VMI_INT"
      | "VMI_INT8"
      | "VMI_UINT8"
      | "VMI_UINT16"
      | "VMI_UINT32" 
      | "VMI_UINT64" 
      )				{ ##->setType(ASTVmiType); }
    ;

cType
    : ("int" 
      | "char"
      | "void"
      )
	{ ##->setType(ASTCType); }
    ;

vmiArgs
    : "VMI_ARGS"! LPAREN! (vmiArgList) RPAREN!
	{ ## = #([ASTVmiArgs, "vmi args"], ##); }
    ;
vmiArgList: vmiArg (COMMA! vmiArg)* ;
vmiArg
    : ((vmiType | cType) (STAR)? Name)
	{ ## = #([ASTVmiArg, "vmi arg"], ##); }
    | "void"!
    ;

vmiInputs
    : "VMI_INPUT"! LPAREN! (vmiInputList)? RPAREN!
	{ ## = #([ASTVmiInput, "vmi input"], ##); }
    ;
vmiInputList: vmiInput (COMMA! vmiInput)*;
vmiInput
    : AsmInputRegSpecifier LPAREN! expr RPAREN!
	{ ## = #([ASTAsmInput, "asm input"], ##); }
    ;

vmiOutputs
    : "VMI_OUTPUT"! LPAREN! (vmiOutputList)? RPAREN!
	{ ## = #([ASTVmiOutput, "vmi output"], ##); }
    ;
vmiOutputList: vmiOutput (COMMA! vmiOutput)*;
vmiOutput
    : AsmOutputRegSpecifier LPAREN! (STAR)? Name RPAREN!
	{ ## = #([ASTAsmOutput, "asm output"], ##); }
    ;

expr: shiftExpr ;

shiftExpr: castExpr (RSHIFT^ castExpr)* ;
castExpr
    : (LPAREN vmiType RPAREN) => LPAREN! vmiType RPAREN! primitive
	{ ## = #([ASTCast, "cast"], ##); }
    | primitive
    ;

primitive
    : Name
    | Int
    | Hex
    | LPAREN! expr RPAREN!
    ;

astDefs
    : ASTCast
    | ASTAsmOutput
    | ASTAsmInput
    | ASTVmiSpace
    | ASTVmiFunc
    | ASTVmiProc
    | ASTVmiJump
    | ASTVmiCalls
    | ASTVmiAsmClobber
    | ASTVmiMemClobber
    | ASTVmiAttributes
    | ASTVmiNotFlat
    | ASTVmiFlat
    | ASTVmiFlatSS
    | ASTVmiVolatile
    | ASTVmiArgs
    | ASTVmiArg
    | ASTVmiInput
    | ASTVmiOutput
    | ASTVmiType
    | ASTCType
    ;

class VmiCallsLexer extends Lexer;
options {
    // Specify the vocabulary, to support inversions in a scanner rule.
    charVocabulary = '\u0000'..'\u00FF';
    k = 2;
}

COMMA	: ',' ;
LPAREN	: '(' ;
RPAREN	: ')' ;
STAR	: '*' ;
RSHIFT	: '>''>' ;
protected BACKSLASH : '\\';

protected Newline
    : '\r' '\n'		{ newline(); } // DOS
    | '\n'		{ newline(); } // Sane systems
    ;

Whitespace
    : (' ' | '\t' | Newline | BACKSLASH)
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;

Comment
    : ((CCommentBegin) => CComment | CPPComment)
	{ $setType(ANTLR_USE_NAMESPACE(antlr)Token::SKIP); }
    ;
protected CCommentBegin : '/''*';
protected CCommentEnd   : '*''/';
protected CComment
    : CCommentBegin (options {greedy=false;}
			: '\n' { newline(); }
			| ~('\n')
                    )*
      CCommentEnd
    ;
protected CPPComment: '/''/' ( ~('\n') )* ;

protected Letter : 'a'..'z' | 'A'..'Z' | '_';
protected Digit  : '0'..'9';

// Note: For all literals that we wish to lookup in the hash table, there
// must be a Lexer rule that can match it, with the testLiterals option 
// enabled.

Name options {testLiterals=true;}: Letter (Letter | Digit)* ;
CPP  options {testLiterals=true;}: '#' Name ;
Int : (Digit)+ ;
Hex : '0' 'x' (Digit | 'a'..'f' | 'A'..'F')+ ;

AsmInputRegSpecifier
    : '"' Digit '"'
    | '"' ('a' | 'b' | 'c' | 'd' | 'A' | 'D' | 'S') '"'
    ;
AsmOutputRegSpecifier
    : '"' ('=' | '+') ('a' | 'b' | 'c' | 'd' | 'A' | 'D' | 'S') '"'
    ;



class VmiCallsHtmlEmitter extends TreeParser;
{
protected:
    void out( char *s )
        { std::cout << s; }
    void out( unsigned u )
        { std::cout << u; }
    void out( antlr::RefAST n )
        { std::cout << n->getText(); }
    void td( antlr::RefAST n )
	{ std::cout << "<td>" << n->getText() << "</td>"; }
    void td( char *s )
	{ std::cout << "<td>" << s << "</td>"; }
}

vmiCalls
    : #(ASTVmiCalls		{ out("<table>\n"); }
        (vmiDefs)?		{ out("</table>\n"); }
       )
    ;

vmiDefs
    : ( { out("<tr style=\"background:lightgreen;\">"); } 
        vmiDef
	{ out("</tr>\n"); }
      )+
    ;

vmiDef
    : vmiProc
    | vmiFunc
    | vmiJump
    | vmiSpace
    ;

vmiSpace
    : #(ASTVmiSpace n:Name)	{ out("<td>Space "); out(n); out("</td>"); }
    ;

vmiProc
    : #(ASTVmiProc n:Name		{ out("<td> void "); out(n); }
        vmiArgs vmiOutputs vmiInputs vmiClobber vmiAttributes
       { out("</td>"); } )
    ;

vmiJump
    : #(ASTVmiJump n:Name		{ out("<td> Jump "); out(n); }
        vmiArgs vmiOutputs vmiInputs vmiClobber vmiAttributes
       { out("</td>"); } )
    ;

vmiFunc
    : #(ASTVmiFunc n:Name r:ASTVmiType	
		{ out("<td>"); out(r); out(" "); out(n); }
        vmiArgs vmiOutputs vmiInputs vmiClobber vmiAttributes
       { out("</td>"); } )
    ;

vmiArgs
    : #(ASTVmiArgs {out("(");} 
        (vmiArg ({out(", ");} vmiArg)*
	 | {out("void");}
	)
        {out(")");} )
    ;

vmiArg
    : #(ASTVmiArg
        (v:ASTVmiType {out(v);} | c:ASTCType {out(c);}) 
	(s:STAR		{out(s);} )? 
	n:Name		{out(" "); out(n);}
       )
    ;

vmiOutputs
    : #(ASTVmiOutput ({out("</tr>\n<tr><td>Outputs: ");} 
        asmOutput ({out(", ");} asmOutput)* {out("</td>");})?
       )
    ;

asmOutput
    : #(ASTAsmOutput r:AsmOutputRegSpecifier	{out(r); out("(");}
        (s:STAR {out(" "); out(s);} )?
	n:Name					{out(n); out(")");}
       )
    ;

vmiInputs
    : #(ASTVmiInput ({out("</tr>\n<tr><td>Inputs: ");} 
        asmInput ({out(", ");} asmInput)* {out("</td>");})?
       )
    ;

asmInput
    : #(ASTAsmInput r:AsmInputRegSpecifier	{out(r); out("(");}
        expr					{out(")");}
       )
    ;

primitive
    : n:Name	{ out(n); }
    | i:Int 	{ out(i); }
    | h:Hex	{ out(h); }
    ;
subexpr: {out("(");}  expr  {out(")");};
expr
    : #(ASTCast {out("(");} 
          (v:ASTVmiType {out(v);} | c:ASTCType {out(c);} ) {out(")");} 
	  subexpr)
    | #(r:RSHIFT expr ({out(r);} expr)+)
    | primitive
    ;

vmiClobber: ASTVmiAsmClobber | ASTVmiMemClobber;

vmiAttributes
    : #(ASTVmiAttributes ({out("</tr>\n<tr><td>Attributes: ");}
        vmiAttribute ({out(", ");} vmiAttribute)*  {out("</td>");})?
       )
    ;
vmiAttribute
    : ASTVmiVolatile	{ out("volatile"); }
    | ASTVmiFlat	{ out("flat"); }
    | ASTVmiNotFlat	{ out("not flat"); }
    | ASTVmiFlatSS	{ out("flat SS+CS"); }
    ;


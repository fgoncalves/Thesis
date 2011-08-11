%{
#include <stdio.h>

#include "y.tab.h"
#include "macros.h"
%}

%x COMMENT ELEMENT TEXT

%option yylineno

digit [0-9]
character [a-zA-Z_\-.]
ws [ \t\r\n]
word ({character}|{digit})+
attr_value ({word}|[/])+
%%
"<"                                                   {debug(I, "LT\n"); BEGIN(ELEMENT); return LT;}
<ELEMENT>">"                                          {debug(I, "GT\n"); BEGIN(INITIAL); return GT;}
<ELEMENT>"="                                          {debug(I, "EQ\n"); return EQ;}
<ELEMENT>"/"                                          {debug(I, "SLASH\n"); return SLASH;}
<ELEMENT>"?"                                          {debug(I, "QUESTION_MARK\n"); return QUESTION_MARK;}
<ELEMENT>"\""                                         {BEGIN(TEXT); debug(I, "DOUBLE_QUOTE\n"); return DOUBLE_QUOTE;}
<ELEMENT>"'"                                          {BEGIN(TEXT); debug(I, "QUOTE\n"); return QUOTE;}
<ELEMENT>({character}|{digit})+                       {debug(I, "ID = |%s|\n", yytext); yylval.words = strdup(yytext); return IDENTIFIER;}                        
<ELEMENT>{ws}+                                        

"<!--"                                       {BEGIN(COMMENT);}
<COMMENT>{
"-->"                                        {BEGIN(INITIAL);}
[^\-\n]+                                     
"-"                                          
\n                                           {yylineno++;}
}

<TEXT>{
{attr_value}								{debug(I, "ATTR_VALUE = |%s|\n", yytext); yylval.words = strdup(yytext); return IDENTIFIER;}
\"											{BEGIN(ELEMENT); debug(I, "DOUBLE_QUOTE\n"); return DOUBLE_QUOTE;}
\'											{BEGIN(ELEMENT); debug(I, "QUOTE\n"); return QUOTE;}
}

{ws}
({word}[ \t\n\r]*)+                         {debug(I, "Text block |%s|\n", yytext); yylval.words = strdup(yytext); return TEXT_BLOCK;}
%%
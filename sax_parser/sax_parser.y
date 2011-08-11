%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "attr_list.h"
#include "string_buffer.h"
#include "macros.h"
#include "sax_parser_callbacks.h"

extern int yylex(void);
extern int yyparse(void);
extern int yylineno;
extern char* yytext;
extern FILE* yyin;

void yyerror(const char *str)
{
  fprintf(stderr,"error at:%d: %s at '%s'\n", yylineno, str, yytext);
}

int yywrap()
{
  return 1;
}

typedef struct selem{
 char* tag;
 attr_list* attrs;
}elem;

sbuffer *__current_attr_value = NULL;
parser_cbs *__parser_callbacks = NULL;

void parse_file(char* filename){
  FILE* holder;
  struct yy_buffer_state* bs;

  holder = yyin = fopen(filename, "r");
  if(yyin == NULL){
    log(F, "Unable to open file %s for reading.\n", filename);
    exit(1);
  }

  yylineno = 1;
  yyparse();
  yylex_destroy();
  if(holder) 
    fclose(holder);
}

void parse_string(const char* str){
  int len = strlen(str);
  char* internal_cpy = alloc(char, len + 2);

  memcpy(internal_cpy, str, len);

  internal_cpy[len] = '\0';
  internal_cpy[len + 1] = '\0';

  if(!yy_scan_buffer(internal_cpy, len + 2)){
    log(F, "Flex could not allocate a new buffer to parse the string.\n");
    exit(1);
  }
  yylineno = 1;
  yyparse();
  yylex_destroy();
  free(internal_cpy);
}
%}

%union{
  char* words;
  struct sattr_list* attributes;
  struct attr_node* attribute;
  struct selem* el;
 }

%token LT GT EQ SLASH QUESTION_MARK DOUBLE_QUOTE QUOTE
%token <words> IDENTIFIER TEXT_BLOCK  

%type <attributes> attrs
%type <attribute> attr
%type <el> start_element end_element

%start s

%%

s: LT QUESTION_MARK IDENTIFIER attrs QUESTION_MARK GT s       {if(!strcmp($3, "xml")){
								yyerror("Tag should be xml");
								exit(-1);
                                                         }
							 free_attr_list($4);
							 free($3);
						        }
 | complex_element
 | simple_element
 ;

element: 
       | element complex_element
       | element simple_element 
       ;

complex_element: start_element element end_element      
               ;

simple_element: LT IDENTIFIER attrs SLASH GT             {if(!__parser_callbacks->start($2, $3, __parser_callbacks->user_data)){
							  free_attr_list($3);
							  yyerror("Start callback returned status error");
							  exit(-1);
							 }
							 free_attr_list($3);
                                                         if(!__parser_callbacks->end($2, __parser_callbacks->user_data)){
							  yyerror("End callback returned status error");
							  exit(-1);
							 }
							 free($2);
                                                        }

              | TEXT_BLOCK                              {
                           	                         if(!__parser_callbacks->text($1, strlen($1), __parser_callbacks->user_data)){
							  yyerror("Text callback returned status error");
							  exit(-1);
							 }
                                                        }
             ;

start_element: LT IDENTIFIER attrs GT                   {
							 if(!__parser_callbacks->start($2, $3, __parser_callbacks->user_data)){
							  free($2);
							  free_attr_list($3);
							  yyerror("Start callback returned status error");
							  exit(-1);
							 }
							 free($2);
							 free_attr_list($3);
							}
             ;

end_element: LT SLASH IDENTIFIER GT                     {
							 if(!__parser_callbacks->end($3, __parser_callbacks->user_data)){
							  free($3);
							  yyerror("End callback returned status error");
							  exit(-1);
							 }
							 free($3);
 							}
           ;

attr_value:                                             {if(!__current_attr_value)
                                                             __current_attr_value = new_string_buffer();
                                                        }
          | attr_value IDENTIFIER                       {append_to_buffer(__current_attr_value, $2);
          												 free($2);}
          ;

attrs:                                                 {$$ = new_attr_list();}
      | attrs attr                                     {$$ = $1;
							add_attr($$, $2);
						       }
      ;

attr: IDENTIFIER EQ QUOTE attr_value QUOTE             {$$ = new_attr($1, __current_attr_value->buffer); 
							clean_buffer(__current_attr_value);
							free($1);
						       }
    | IDENTIFIER EQ DOUBLE_QUOTE attr_value DOUBLE_QUOTE     {$$ = new_attr($1, __current_attr_value->buffer); 
							clean_buffer(__current_attr_value);
							free($1);
					       	       }
    ;
%%

/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_YY_BUILD_PARSER_TAB_HPP_INCLUDED
# define YY_YY_BUILD_PARSER_TAB_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 1 "misc/parser.ypp"

  #include "../inc/assembler.hpp"

  extern int yylex();
  extern int yyparse();
  extern FILE *yyin;

  void yyerror(const char *s);

#line 58 "build/parser.tab.hpp"

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    ENDL = 258,
    GLOBAL = 259,
    EXTERN = 260,
    SECTION = 261,
    WORD = 262,
    SKIP = 263,
    ASCII = 264,
    EQU = 265,
    END = 266,
    HALT = 267,
    INT = 268,
    IRET = 269,
    CALL = 270,
    RET = 271,
    JMP = 272,
    BEQ = 273,
    BNE = 274,
    BGT = 275,
    PUSH = 276,
    POP = 277,
    XCHG = 278,
    ADD = 279,
    SUB = 280,
    MUL = 281,
    DIV = 282,
    NOT = 283,
    AND = 284,
    OR = 285,
    XOR = 286,
    SHL = 287,
    SHR = 288,
    LD = 289,
    ST = 290,
    CSRRD = 291,
    CSRWR = 292,
    R0 = 293,
    R1 = 294,
    R2 = 295,
    R3 = 296,
    R4 = 297,
    R5 = 298,
    R6 = 299,
    R7 = 300,
    R8 = 301,
    R9 = 302,
    R10 = 303,
    R11 = 304,
    R12 = 305,
    R13 = 306,
    R14 = 307,
    R15 = 308,
    STATUS = 309,
    HANDLER = 310,
    CAUSE = 311,
    LIT = 312,
    SYMB = 313
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 19 "misc/parser.ypp"

  std::string* arg; //lexer ovo prosledjuje pojedinacno
  Args* args; //parser spaja argumente linije koje lexer posalje

#line 133 "build/parser.tab.hpp"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_BUILD_PARSER_TAB_HPP_INCLUDED  */

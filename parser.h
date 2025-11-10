extern int E(void);
extern void T(void);
extern void R(void);
extern void Q(void);
extern void F(void);

extern char lexeme[]; // definido no lexer.c
extern int lookahead; // definido no parser.c
extern int gettoken(FILE *); // definido no lexer.c
extern int mybc(void); // definido no parser.c
extern int match(int expected); // definido no parser.c
extern int cmd(void);

extern FILE *source;

//extern int lineno;
//pois parser.c já inclui lexer.h, que tem token_lineno e token_colno

#define ERRTOKEN -0x1000000
//ALUNOS: João Bastasini RA221152067, Julia Amadio RA211151041

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <tokens.h>
#include <lexer.h>

/*=================== GLOBAIS DE ESTADO E RASTREAMENTO DE POSIÇÃO ===================
  A lógica de rastreamento de erro depende desta máquina de estado
  de 5 variáveis globais:

  1. lineno: O contador persistente de linhas. É incrementado em next_char() 
  e decrementado em unread_char(). NUNCA é resetado pelo main.c.

  2. colno: A coluna atual. É resetada para 1 em cada '\n' e
  incrementada/decrementada pelos wrappers.

  3. token_lineno / 4. token_colno:
  Armazenam a posição (linha/coluna) do *início* do token atual. São "travadas" no 
  início do gettoken() e usadas pelo match() para reportar erros.

  5. colno_before_newline: Variável de estado crucial. Salva o 'colno' da linha 
  anterior *antes* que next_char() o resete para 1. Isto permite que unread_char() 
  restaure a coluna correta se o parser precisar "devolver" um '\n', garantindo 
  que erros no fim da linha (ex: "5 +") reportem a coluna correta.
*/

int lineno = 1;
int colno = 1; //var global que conta a posição do caractere na linha atual

int token_lineno = 1; //armazenam onde o token começou
int token_colno = 1;

/*Percebi que em um caso de fazer '(6 + 3', sem fechar os parenteses,
  o output era 
  		token mismatch error at line 1, column 1
		found (
		) but expected ())
  isso acontece pois ao apertar enter ele começaria a contagem de colunas por uma nova linha
  ele precisa guardar qual era a coluna na contagem antes do newline (enter).*/
int colno_before_newline = 1;

/*Wrapper para getc()
  Esta função LÊ um caractere e ATUALIZA as globais lineno/colno.
  Esta é a ÚNICA função que deve chamar getc().
 */
int next_char(FILE *tape)
{
	int ch = getc(tape);

	if (ch == '\n') {
		lineno++;
		colno_before_newline = colno; //SALVA O ESTADO
		colno = 1; //Reseta a coluna na nova linha
	} else if (ch == '\t') { //tab
		colno += 4; //Ou 8, mas 4 é um bom padrão
	} else if (ch != EOF) {
		colno++;
	}
	return ch;
}

/*
  Wrapper para ungetc()
  Esta função DEVOLVE um caractere e REVERTE o colno.
  Esta é a ÚNICA função que deve chamar ungetc().
 */
void unread_char(int ch, FILE *tape)
{
	ungetc(ch, tape);

	//Lógica segura para reverter a coluna E LINHA
	if (ch == '\n') {
		lineno--;
		colno = colno_before_newline; //RESTAURA O ESTADO
		//(Não podemos saber qual era o colno anterior,
		//mas o lineno está correto agora)
	} else if (ch == '\t') {
		colno -= 4; //Reverte o TAB
	} else if (ch != EOF) {
		colno--;
	}
}

char lexeme[MAXIDLEN + 1];

/* Versão extendida de identificador Pascal
 * ID = [A-Za-z][A-Za-z0-9]*
 */
int isID(FILE *tape)
{
	if ( isalpha(lexeme[0] = next_char(tape)) ) {
		int i = 1;
		while ( isalnum( lexeme[i] = next_char(tape) ) ){ 
			i++;
		}
		unread_char(lexeme[i], tape);
		lexeme[i] = 0;

		if (strcmp(lexeme, "exit") == 0) {
			return EXIT;
		}
		if (strcmp(lexeme, "quit") == 0){
			return QUIT;
		}

		return ID;
	}

	unread_char(lexeme[0], tape);
	lexeme[0] = 0;
	return 0;
}

/*
 * DEC = [1-9][0-9]* | '0'
 *                           ------------------------------------------
 *                          |                      digit               |
 *                          |                    --------              |
 *                          |                   |        |             |
 *               digit      |     not zero      V        |  epsilon    V
 * -->(is DEC)--------->(is ZERO)---------->(isdigit)-------------->((DEC))
 *       |
 *       | epsilon
 *       |
 *       V
 *     ((0))
 */
int isDEC(FILE *tape)
{
	if ( isdigit(lexeme[0] = next_char(tape)) ) {
		if (lexeme[0] == '0') {
			return DEC;
		}
		int i = 1;
		while ( isdigit(lexeme[i] = next_char(tape)) ) i++;
		unread_char(lexeme[i], tape);
		lexeme[i] = 0;
		return DEC;
	}

	unread_char(lexeme[0], tape);
	lexeme[0] = 0;
	return 0;
}

// fpoint = DEC\.[0-9]* | \.[0-9][0-9]*
// flt = fpoint EE? | DEC EE
// EE = [eE]['+''-']?[0-9][0-9]*
// test input: 3e+
//             012

int isEE(FILE *tape)
{
	int i = strlen(lexeme);

	if ( toupper(lexeme[i] = next_char(tape)) == 'E' ) {
		i++;
		
		// check optional signal
		int hassign;
		if ( (lexeme[i] = next_char(tape)) == '+' || lexeme[i] == '-' ) {
			hassign = i++;
		} else {
			hassign = 0;
			unread_char(lexeme[i], tape);
		}
		
		// check required digit following
		if ( isdigit(lexeme[i] = next_char(tape)) ) {
			i++;
			while( isdigit(lexeme[i] = next_char(tape)) ) i++;
			unread_char(lexeme[i], tape);
			lexeme[i] = 0;
			return FLT;
		}
		unread_char(lexeme[i], tape);
		i--;
		if (hassign) {
			unread_char(lexeme[i], tape);
			i--;
		}
	}

	unread_char(lexeme[i], tape);
	lexeme[i] = 0;	
	return 0;
}

int isNUM(FILE *tape)
{
	int token = isDEC(tape);
	if (token == DEC) {
		int i = strlen(lexeme);
		if ( (lexeme[i] = next_char(tape)) == '.' ) {
			i++;
			while ( isdigit( lexeme[i] = next_char(tape) ) ) i++;
			unread_char(lexeme[i], tape);
			lexeme[i] = 0;
			token = FLT;
		} else {
			unread_char(lexeme[i], tape);
			lexeme[i] = 0;
		}
	} else {
		if ( (lexeme[0] = next_char(tape)) == '.' ) {
			if ( isdigit( lexeme[1] = next_char(tape) ) ) {
				token = FLT;
				int i = 2;
				while ( isdigit( lexeme[i] = next_char(tape) ) ) i++;
			} else {
				unread_char(lexeme[1], tape);
				unread_char(lexeme[0], tape);
				lexeme[0] = 0;
				return 0; // not a number
			}
		} else {
			unread_char(lexeme[0], tape);
			lexeme[0] = 0;
			return 0; // not a number
		}
	}
	
	if (isEE(tape)) {
		token = FLT;
	}

	return token;
}

int isASGN(FILE *tape)
{
	lexeme[0] = next_char(tape);
	if (lexeme[0] == ':') {
		lexeme[1] = next_char(tape);
		if (lexeme[1] == '=') {
			lexeme[2] = 0;
			return ASGN;
		}
		unread_char(lexeme[1], tape);
	}
	unread_char(lexeme[0], tape);
	return lexeme[0] = 0;
}

/*
 * OCT = '0'[0-7]+
 */
int isOCT(FILE *tape)
{
	if ( (lexeme[0] = next_char(tape)) == '0') {
		int i = 1;
		if ((lexeme[i] = next_char(tape)) >= '0' && lexeme[i] <= '7') {
			i = 2;
			while ((lexeme[i] = next_char(tape)) >= '0' && lexeme[i] <= '7') i++;
			unread_char(lexeme[i], tape);
			lexeme[i] = 0;
			return OCT;
		}
		unread_char(lexeme[1], tape);
		unread_char(lexeme[0], tape);
	} else {
		unread_char(lexeme[0], tape);
	}
	return 0;
}

/*
 * HEX = '0'[Xx][0-9A-Fa-f]+
 *
 * isxdigit == [0-9A-Fa-f]
 */
int isHEX(FILE *tape)
{
	// Para ter um hexa, é necessário que venha o prefixo "0[xX]" seguido de um hexa digito
	if ( (lexeme[0] = next_char(tape)) == '0' ) {
		if ( toupper(lexeme[1] = next_char(tape)) == 'X' ) {
			if ( isxdigit(lexeme[2] = next_char(tape)) ) {
				int i = 3;
				while ( isxdigit(lexeme[i] = next_char(tape)) ) {
					i++;
				}
				unread_char(lexeme[i], tape);
				lexeme[i] = 0;
				return HEX;
			}
			unread_char(lexeme[2], tape);
			unread_char(lexeme[1], tape);
			unread_char(lexeme[0], tape);
			lexeme[0] = 0;
			return 0;
		}
		unread_char(lexeme[1], tape);
		unread_char(lexeme[0], tape);
		lexeme[0] = 0;
		return 0;
	}
	unread_char(lexeme[0], tape);
	lexeme[0] = 0;
	return 0;
}

// Skip spaces
void skipspaces(FILE *tape)
{
	int head;

	//&& head != '\n'
	while ( isspace(head = next_char(tape)) && head != '\n' )
	{
		//O loop continua vazio, e isso está CORRETO.
		//Ele só vai rodar para espaços (' ') e tabs ('\t').
	}

	//Devolve o caractere que parou o loop 
	//(que será o '\n' ou um caractere não-espaço)
	unread_char(head, tape);
}

int gettoken(FILE *source)
{
	int token;

	skipspaces(source);

	//salva a posição DE INÍCIO do token
    token_lineno = lineno;
    token_colno = colno;

	if ( (token = isID(source)) ) return token;
	if ( (token = isOCT(source)) ) return token;
	if ( (token = isHEX(source)) ) return token;
	if ( (token = isNUM(source)) ) return token;
	if ( (token = isASGN(source)) ) return token;

	lexeme[0] = token = next_char(source);
	lexeme[1] = 0;

	return token;
}
//ALUNOS: João Bastasini RA221152067, Julia Amadio 211151041

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <tokens.h>
#include <parser.h>
#include <lexer.h>
/**********************************************************************************************************
 ********************** Compilador de expressões Pascal para expressões pós-fixas *************************
 **********************************************************************************************************/

int lookahead; // O olho do parser, é por este operador que o parser enxerga antecipadamente os tokens
	       	   // provenientes do lexer

// como é a seção bss, o array será inicializado com zero
#define STACKSIZE 256
double stack[STACKSIZE];
// stack pointer (sp)
int sp = -1;// -1 significa pilha vazia
// acumulador (accumulator)
double acc;

#define VMEMSIZE 4096
double vmem[VMEMSIZE];
char symtab[VMEMSIZE][MAXIDLEN+1];
int symtab_next_entry = 0;

//array com os nomes dos tokens, para retornar ao invés do número associado
static const char *token_names[] = {
	"ID",
	"OCT",
	"HEX",
	"DEC",
	"FLT",
	"ASGN",
	"EXIT",
	"QUIT"
};

static const char *get_token_name(int token_code) {
	if (token_code == '\n') return "'\\n'"; //tratamento especial para nova linha
    if (token_code == EOF) return "EOF"; //tratamento especial para EOF

    //para tokens ASCII imprimíveis (códigos 1 a 255), não entra no loop se for um caractere não-imprimível (ex: '\n')
    if (token_code > 0 && token_code <= 255 && isprint(token_code)) {
		//cria uma string temporária para retornar o caractere entre aspas simples
        static char temp_name[4]; 
		// \ é escape para imprimir aspas simples
        temp_name[0] = '\'';
        temp_name[1] = (char)token_code;
        temp_name[2] = '\'';
        temp_name[3] = '\0';
        return temp_name;
    }

    //para tokens definidos em tokens.h (código >= 1024)
    if (token_code >= ID && token_code <= QUIT) {
        //ID é 1024, no array é 0, a posição no array é 'token_code - ID'
        return token_names[token_code - ID];
    }

    //para EOF, que é -1 (parser.c já lida com ele, mas não custa nada)
    if (token_code == EOF) {
        return "EOF";
    }

    //se for um caractere de controle, não-imprimível ou um código desconhecido
    return "UNKNOWN_TOKEN";
}


//fazer na mão ou criar pseudo-comandos
//LVAL := RVAL
int cursor;
double recall(char *symbol) //vai buscar na tabela vmem o nome, usado como RVAL
{
	for ( cursor = symtab_next_entry - 1; cursor > -1; cursor-- ){
		if (strcmp(symtab[cursor], symbol) == 0){
			return vmem[cursor];
		}
	}
	//se chegar aqui, não encontrou nada, cursor = -1
	strcpy(symtab[symtab_next_entry], symbol);
	cursor = symtab_next_entry++; //expr atribuição
	return 0.0e+000; //ponto flutuante
}
void store(char *symbol, double lval) //reversa do recall, usado como LVAL
{
	recall(symbol);
	vmem[cursor] = lval;
}

//Interpretador de comandos é um laço infinito (enquanto dure)
//que é interrompido ao avistar EOF ou o comando quit ou exit

/*mybc -> cmd { CMDSEP cmd } EOF
  cmd -> E | exit | quit | <epsilon>
  CMDSEP = ; | \n //definido no lexer*/
/*Esta versão é feita para ser chamada pelo novo main.c (com fgets).
  Ela NÃO é um loop infinito. Ela processa a linha atual (que está
  na fonte 'source' em memória) e então retorna.*/
int mybc(void)
{
	//O loop while(1) agora está no main.c.
	//Este loop while(lookahead != EOF) vai processar
	//a linha inteira que o fmemopen() criou.
	int status;

	while (lookahead != EOF) {
		//Tratamento para 'exit' e 'quit'
		if (lookahead == EXIT || lookahead == QUIT) {
			match(lookahead);
			exit(0); //encerrar o programa (apenas quando há intenção, não por erro)
		}

		//Processa o comando (ex: "5+6" ou "a := 10")
		status = cmd();

		//propaga erro (se houve erro no cmd, retorna ERRTOKEN para main.c)
		if (status == ERRTOKEN) {
			return ERRTOKEN;
		}

		//Após processar um comando, esperamos um separador (;)
		//ou um fim de linha (\n) ou o fim da fita (EOF).
		
		if (lookahead == ';') {
			status = match(';');
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			//Se for ';', o loop continua para o próximo comando
			continue;
		} 
		else if (lookahead == '\n') {
			status = match('\n');
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			//Se for '\n', o loop continua (caso haja algo
			//inesperado depois, como em "5+5 4")
			continue; 
		}
		else if (lookahead == EOF) {
			break; //A linha acabou, saia do loop.
		}
		else {
			//Se chegou aqui, é um erro. (ex: "5 5")
			//O lookahead é um token inesperado (ex: DEC).
			//Forçamos o erro de "esperava \n"
			status = match('\n');
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
		}
	}
	
	//Fim da linha (EOF) foi alcançado.
	return 0; //sem erros
}

int cmd(void)
{
	int status = 0;
	switch(lookahead){
		case EXIT:
		case QUIT:
			exit(0);
		//aqui é o FIRST(E):
		case '+':
		case '-':
		case '(':
		case ID:
		case OCT:
		case HEX:
		case FLT:
		case DEC:
			status = E();
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			/**/printf("%lg\n", acc);
			break;
		//Palavra nula
		default:
			;
	}
	return status;
}

// /**/ = AÇÃO SEMÂNTICA

/*
 * E -> [ ominus ] T { oplus T }
 *  input: "a * b"
 *  token: ID '*' ID
 */
int E(void) 
{
	/**/char variable[MAXIDLEN + 1];
	/**/int oplus_flg = 0;/**/
	/**/int otimes_flg = 0;/**/
	/**/int ominus_flg = 0;/**/

	int status = 0;

	// [ ominus ]; ominus = '+'|'-'
	if (lookahead == '+' || lookahead == '-') {
		/**/if (lookahead == '-') {
			ominus_flg = lookahead;
		}/**/
		status = match(lookahead);
		if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
	}
_Tbegin:
_Fbegin:
	switch(lookahead) {
		//xpr entre parênteses
		case '(':
			status = match('(');
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			status = E();
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			status = match(')');
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
			break;

		//constantes
		case HEX:
			/**/ acc = strtol(lexeme, NULL, 0);/**/
			status = match(HEX); break;
		case OCT:
			/**/ acc = strtol(lexeme, NULL, 0);/**/
			status = match(OCT); break;
		case DEC:
			/**/ acc = atoi(lexeme);/**/
			status = match(DEC); break;
		case FLT:
			/**/ acc = atof(lexeme);/**/
			status = match(FLT); break;
		
		//variáveis
		default:
			/**/strcpy(variable, lexeme);/**/ 
			status = match(ID);
			if (status == ERRTOKEN) return ERRTOKEN; //propaga erro

			// cheque operador atribuição, ":=" => ASGN
			if (lookahead == ASGN) {
				// variable := express
				status = match(ASGN);
				if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
				status = E();
				if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
				/**/store (variable, acc);/**/
			} else { //se não vier operador atribuição
				//recall previously stored floating value
				/**/acc = recall(variable);/**/
			}
	}

//pega erro dos matches de constantes ou ID, ao invés de fazer em cada case
if (status == ERRTOKEN) return ERRTOKEN; 

	// se chegou operador multiplicativo é porque já houve uma entrada no acc como 1° operando
	/**/if (otimes_flg) {
		if (otimes_flg == '*') {
			acc = stack[sp] * acc;
		} else {
			acc = stack[sp] / acc;
		}
		sp--;
		otimes_flg = 0;// turnoff the otimes flag
	}/**/
	// { otimes T }; otimes = '*'|'/'
	if (lookahead == '*' || lookahead == '/') {
		/**/otimes_flg = lookahead;/**/
		/**/
		// salva na pilha porque há de vir um próximo operando
		// push(acc);
		sp++;
		stack[sp] = acc;
		/**/
		status = match(lookahead);
		if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
		goto _Fbegin;
	}
	/**/if (ominus_flg) {
		acc = -acc;
		ominus_flg = 0;// turnoff ominus flag
	}/**/
	// se chegou operador multiplicativo é porque já houve uma entrada no acc como 1° operando
	/**/if (oplus_flg) {
		if (oplus_flg == '+') {
			acc = stack[sp] + acc;
		} else {
			acc = stack[sp] - acc;
		}
		sp--;
		oplus_flg = 0;// turnoff the oplus flag
	}/**/
	if (lookahead == '+' || lookahead == '-') {
		/**/oplus_flg = lookahead;/**/
		// salva na pilha porque há de vir um próximo operando
		// push(acc);
		/**/
		sp++;
		stack[sp] = acc;
		/**/
		status = match(lookahead);
		if (status == ERRTOKEN) return ERRTOKEN; //propaga erro
		goto _Tbegin;
	}
	return 0; //sem erros
}

/*** A principal função (procedimento) interface do parser é match: */

int match(int expected_token)
{
	if (lookahead == expected_token) {
		lookahead = gettoken(source);
	} else {
		const char *found_name = get_token_name(lookahead); //token que achou
        const char *expected_name = get_token_name(expected_token); //token que esperava

		fprintf(stderr, "token mismatch error at line %d, column %d\n", token_lineno, token_colno);
    	fprintf(stderr, "found (%s) but expected (%s)\n", found_name, expected_name);
    	return ERRTOKEN;
	}
	return 0;
}
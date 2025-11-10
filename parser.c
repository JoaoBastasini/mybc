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
void mybc(void)
{
	//O loop while(1) agora está no main.c.
	//Este loop while(lookahead != EOF) vai processar
	//a linha inteira que o fmemopen() criou.
	while (lookahead != EOF) {
		
		//Tratamento para 'exit' e 'quit'
		if (lookahead == EXIT || lookahead == QUIT) {
			match(lookahead);
			exit(0);
		}

		//Processa o comando (ex: "5+6" ou "a := 10")
		cmd();

		//Após processar um comando, esperamos um separador (;)
		//ou um fim de linha (\n) ou o fim da fita (EOF).
		
		if (lookahead == ';') {
			match(';');
			//Se for ';', o loop continua para o próximo comando
			continue;
		} 
		else if (lookahead == '\n') {
			match('\n');
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
			match('\n'); 
		}
	}
	
	//Fim da linha (EOF) foi alcançado.
	//A função agora termina e retorna para o loop while(1) no main.c.
	return;
}

void cmd(void)
{
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
			E();
			/**/printf("%lg\n", acc);
			break;
		//Palavra nula
		default:
			;
	}
}

// /**/ = AÇÃO SEMÂNTICA

/*
 * E -> [ ominus ] T { oplus T }
 *  input: "a * b"
 *  token: ID '*' ID
 */
void E(void) 
{
	/**/char variable[MAXIDLEN + 1];
	/**/int oplus_flg = 0;/**/
	/**/int otimes_flg = 0;/**/
	/**/int ominus_flg = 0;/**/
	// [ ominus ]; ominus = '+'|'-'
	if (lookahead == '+' || lookahead == '-') {
		/**/if (lookahead == '-') {
			ominus_flg = lookahead;
		}/**/
		match(lookahead);
	}
_Tbegin:
_Fbegin:
	switch(lookahead) {
		//xpr entre parênteses
		case '(':
			match('('); E(); match(')');
			break;
		//constantes
		case HEX:
			/**/ acc = strtol(lexeme, NULL, 0);/**/
			match(HEX); break;
		case OCT:
			/**/ acc = strtol(lexeme, NULL, 0);/**/
			match(OCT); break;
		case DEC:
			/**/ acc = atoi(lexeme);/**/
			match(DEC); break;
		case FLT:
			/**/ acc = atof(lexeme);/**/
			match(FLT); break;
		default:
		//variáveis
			/**/strcpy(variable, lexeme);/**/ 
			match(ID);
			// cheque operador atribuição, ":=" => ASGN
			if (lookahead == ASGN) {
				// variable := express
				match(ASGN);
				E();
				/**/store (variable, acc);/**/
			} else { //se não vier operador atribuição
				//recall previously stored floating value
				/**/acc = recall(variable);/**/
			}
	}

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
		match(lookahead);
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
		match(lookahead);
		goto _Tbegin;
	}
}

/*** A principal função (procedimento) interface do parser é match: */

void match(int expected_token)
{
	if (lookahead == expected_token) {
		lookahead = gettoken(source);
	} else {
		fprintf(stderr, "token mismatch error at line %d, column %d\n", token_lineno, token_colno);
    	fprintf(stderr, "found (%s) but expected (%c)\n", lexeme, expected_token);
    	exit(ERRTOKEN);
	}
}
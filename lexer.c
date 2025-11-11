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
	//Tenta ler o primeiro caractere. IDs devem começar com uma letra.
	if ( isalpha(lexeme[0] = next_char(tape)) ) {

		int i = 1; //Inicializa o índice do lexema (posição 0 já foi lida).

		//Continua lendo (consumindo) caracteres enquanto forem alfanuméricos
		while ( isalnum( lexeme[i] = next_char(tape) ) ){ 
			i++;
		}

		//O loop parou. O último caractere lido (em lexeme[i]) não é alfanumérico.
		unread_char(lexeme[i], tape);
		lexeme[i] = 0;  //Add o terminador nulo ('\0') para fechar a string 'lexeme'

		//Agora que temos a string completa, vemos se é uma palavra reservada
		if (strcmp(lexeme, "exit") == 0) {
			return EXIT;
		}
		if (strcmp(lexeme, "quit") == 0){
			return QUIT;
		}

		//Se não for uma palavra-chave, é um ID de variável comum
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
	//Tenta ler o primeiro caractere, deve ser um dígito
	if ( isdigit(lexeme[0] = next_char(tape)) ) {

		//Caso especial: Se o número for apenas '0'.
		if (lexeme[0] == '0') {
			return DEC;
		}

		//Se o primeiro dígito for [1-9], continuamos lendo
		int i = 1; //Posição 0 já foi lida
		while ( isdigit(lexeme[i] = next_char(tape)) ) i++; //Consome (lê) todos os dígitos seguintes.

		//O loop parou, então lexeme[i] é um caractere não-dígito.
		unread_char(lexeme[i], tape); //Devolve o caractere "não-dígito" à fita
		lexeme[i] = 0; //Add o terminador nulo para fechar a string 'lexeme'
		return DEC; //Retorna sucesso
	}

	//Se o if inicial falhou, o primeiro caractere não era um dígito
	unread_char(lexeme[0], tape);	//Devolve o caractere lido à fita
	lexeme[0] = 0;					//Limpa o lexema
	return 0;						//Retorna 0 (falso), não é DEC
}

// fpoint = DEC\.[0-9]* | \.[0-9][0-9]*
// flt = fpoint EE? | DEC EE
// EE = [eE]['+''-']?[0-9][0-9]*
// test input: 3e+
//             012
//Esta função verifica o sufixo de notação científica (ex: "e+10", "E-5").
//Ela é chamada DEPOIS que a primeira parte de um número (ex: "1.5")
//já foi lida e está em 'lexeme'.
int isEE(FILE *tape)
{
	//Pega o comprimento do que já foi lido (ex: "1.5", strlen=3)
    //'i' será o índice para anexar novos caracteres
	int i = strlen(lexeme);

	//Tenta ler um 'e' ou 'E'.
	if ( toupper(lexeme[i] = next_char(tape)) == 'E' ) {
		i++; //Avança índice do lexema
		
		//Verifica por um sinal opcional ('+' ou '-').
		int hassign;
		if ( (lexeme[i] = next_char(tape)) == '+' || lexeme[i] == '-' ) {
			hassign = i++; //Sim, encontramos um sinal. Avança o índice
		} else {
			hassign = 0;  //Não, não encontramos um sinal.
			unread_char(lexeme[i], tape);  //Devolve o caractere (que não era sinal) à fita
		}
		
		//Verifica se há um DÍGITO OBRIGATÓRIO em seguida
        //(ex: "1.5e+" não é válido, "1.5e+1" é)
		if ( isdigit(lexeme[i] = next_char(tape)) ) {
			i++; //Avança o índice.

			//Consome (lê) quaisquer outros dígitos
			while( isdigit(lexeme[i] = next_char(tape)) ) i++;

			//Devolve o caractere que não era dígito (fim do número)
			unread_char(lexeme[i], tape);
			lexeme[i] = 0; 	//Fecha a string
			return FLT;		//Sucesso: é FLT
		}

		//CASCATA DE FALHA: se chegamos aqui, o 'isdigit' falhou (ex: "1.5e+" ou "1.5e")

		//1. Devolve o caractere que não era dígito
		unread_char(lexeme[i], tape);
		i--;

		//2. Se também lemos um sinal ('+'), devolve ele também.
		if (hassign) {
			unread_char(lexeme[i], tape);
			i--;
		}
	}

	//Se chegamos aqui, o primeiro if falhou (não era 'E'), ou a cascata de falha
    //acima foi executada.
	unread_char(lexeme[i], tape); 	//Devolve o 'E' (ou o que tenhamos lido primeiro)
	lexeme[i] = 0;					//Limpa o lixo que anexamos ao lexema (ex: "1.5e")
	return 0;						//Retorna 0 (falso), não é notação científica
}

//isNUM tenta reconhecer um DEC, um FLT, ou um FLT científico
int isNUM(FILE *tape)
{
	//Tenta primeiro reconhecer um número decimal (ex: "123")
    //A função isDEC() já preenche o 'lexeme' e devolve o não-dígito
	int token = isDEC(tape);

	//Caminho 1: começou com um dígito (isDEC retornou DEC).
	if (token == DEC) {
		int i = strlen(lexeme);  //Pega o fim do que 'isDEC' já leu (ex: "123")

		//Verifica se o próximo caractere é um ponto (ex: "123.")
		if ( (lexeme[i] = next_char(tape)) == '.' ) {
			i++; //Avança o índice

			//Consome todos os dígitos que vêm DEPOIS do ponto
			while ( isdigit( lexeme[i] = next_char(tape) ) ) i++;
			unread_char(lexeme[i], tape); 	//Devolve o caractere que não é dígito
			lexeme[i] = 0;					//Fecha a string do lexema (ex: "123" ou "123.45")
			token = FLT;					//Se teve um ponto, é um Ponto Flutuante (FLT)
		} else {
			//Se não for um ponto, devolve o caractere (ex: "123" + ESPAÇO)
			unread_char(lexeme[i], tape);
			lexeme[i] = 0; //Garante que o lexema termine onde 'isDEC' parou.
		}
	} else { //Caminho 2: não começou com um dígito (isDEC retornou 0).
		//Tenta ver se é um número que começa com '.' (ex: ".123")
		if ( (lexeme[0] = next_char(tape)) == '.' ) {
			//Verifica se HÁ um dígito OBRIGATÓRIO após o ponto.
			if ( isdigit( lexeme[1] = next_char(tape) ) ) {
				token = FLT; //Sucesso, é ponto flutuante.
				int i = 2;   //Começa a consumir do 2º caractere em diante

				//Consome o resto dos dígitos
				while ( isdigit( lexeme[i] = next_char(tape) ) ) i++;
				//(O 'unread_char' e 'lexeme[i] = 0' já são feitos por isEE ou na próxima etapa)
			} else {
				//Se for um '.' seguido por um não-dígito (ex: um '.' solitário)
				unread_char(lexeme[1], tape); //Devolve o não-dígito
				unread_char(lexeme[0], tape); //Devolve o ponto
				lexeme[0] = 0;				  //Limpa o lexema
				return 0; 					  //Não é um número.
			}
		} else {
			//Se não começou com dígito NEM com ponto
			unread_char(lexeme[0], tape);  	//Devolve o que quer que tenha lido.
			lexeme[0] = 0;
			return 0; 						//not a number
		}
	}
	
	//Caminho 3: verificação de Notação Científica (sufixo 'e')
    //Esta função é chamada DEPOIS de "123" ou "123.45" ou ".123" ser lido
	if (isEE(tape)) { //isEE() tentará anexar "e+10" ao lexema
		token = FLT;  //Se 'isEE' for sucesso, o token é FORÇADO a ser FLT.
	}

	return token;     //Retorna o tipo final (DEC, FLT, ou 0 se falhou lá em cima)
}

//Verifica se os próximos dois tokens são o operador de atribuição (:=)
int isASGN(FILE *tape)
{
	//Tenta ler o primeiro caractere (lookahead de 1)
	lexeme[0] = next_char(tape);
	if (lexeme[0] == ':') { 			//Verifica se o primeiro caractere é ':'.
		lexeme[1] = next_char(tape);	//Se for, tenta ler o segundo caractere (lookahead de 2)
		if (lexeme[1] == '=') {			//Verifica se o segundo caractere é '='.
			//Sucesso! Temos ":="
			lexeme[2] = 0; //Fecha a string "lexeme" com ":="
			return ASGN;   //Retorna o token de atribuição (assignment)
		}

		//Falha: era um ':' mas não um '=' (ex: "a : 5")
		unread_char(lexeme[1], tape); //Devolve o segundo caractere à fita
	}

	//Falha: o primeiro caractere não era ':' (ou falhou no passo anterior)
	unread_char(lexeme[0], tape);	//Devolve o primeiro caractere à fita
	return lexeme[0] = 0;			//Limpa o lexema, eetorna 0 (falso). Não é uma atribuição.
}

/*
 * OCT = '0'[0-7]+
 * (Um '0' seguido por UM ou MAIS dígitos de 0 a 7)
 */
int isOCT(FILE *tape)
{
	//Tenta ler o primeiro caractere, que deve ser '0'
	if ( (lexeme[0] = next_char(tape)) == '0') {
		int i = 1; //Posição 0 já foi lida.

		//Tenta ler o SEGUNDO caractere. Ele DEVE ser um dígito octal ([0-7])
        //Esta é a parte '+' da regra (pelo menos um)
		if ((lexeme[i] = next_char(tape)) >= '0' && lexeme[i] <= '7') {
			i = 2; //Posição 1 já foi lida, agora preenchemos a partir da 2.

			// Consome (lê) o resto dos dígitos octais ([0-7])
			while ((lexeme[i] = next_char(tape)) >= '0' && lexeme[i] <= '7') i++;

			//O loop parou, lexeme[i] é um caractere não-octal
			unread_char(lexeme[i], tape);	//Devolve o caractere à fita.
			lexeme[i] = 0;					//Fecha a string do lexema
			return OCT;						//Sucesso! É um número Octal.
		}

		//Se o segundo caractere não for [0-7] (ex: "0" ou "09").
		unread_char(lexeme[1], tape);	//Devolve o segundo caractere (ex: '9')
		unread_char(lexeme[0], tape);	// Devolve o '0' inicial
	} else {
		//Se o primeiro caractere nem era '0'
		unread_char(lexeme[0], tape);	//Devolve o caractere.
	}

	//Se chegou aqui, é porque falhou em algum ponto
    return 0; //Retorna 0 (falso), não é um Octal.
}

/*
 * HEX = '0'[Xx][0-9A-Fa-f]+
 *
 * isxdigit == [0-9A-Fa-f]
 */
int isHEX(FILE *tape)
{
	//Para ter um hexa, é necessário que venha o prefixo "0[xX]" seguido de um hexa digito
	if ( (lexeme[0] = next_char(tape)) == '0' ) {
		//Tenta ler o segundo caractere, que deve ser 'x' ou 'X'.
		if ( toupper(lexeme[1] = next_char(tape)) == 'X' ) {
			//Tenta ler o terceiro caractere, que DEVE ser um dígito hexadecimal
            //Esta é a parte '+' da regra (pelo menos um)
			if ( isxdigit(lexeme[2] = next_char(tape)) ) {
				int i = 3; //Posições 0, 1, e 2 já foram lidas.

				// Consome (lê) o resto dos dígitos hexadecimais
				while ( isxdigit(lexeme[i] = next_char(tape)) ) {
					i++; //Avança o índice
				}

				//O loop parou, lexeme[i] é um caractere não-hexadecimal
				unread_char(lexeme[i], tape); //Devolve o caractere à fita
				lexeme[i] = 0;				  //Fecha a string
				return HEX;					  //Sucesso! É um número Hexadecimal
			}

			//Se o terceiro caractere não for hexadecimal
			unread_char(lexeme[2], tape);
			unread_char(lexeme[1], tape);
			unread_char(lexeme[0], tape);	//Devolve primeiro, segundo e terceiro caracteres
			lexeme[0] = 0;					//Limpa o lexema
            return 0;      					//Retorna 0 (falso)
		}

		//Se o segundo caractere não for 'X'
		unread_char(lexeme[1], tape);		
		unread_char(lexeme[0], tape);		//Devolve primeiro e segundo caracteres
		lexeme[0] = 0; 						//Limpa o lexema
        return 0;      						//Retorna 0 (falso)
	}

	//Se o primeiro caractere não for '0'.
	unread_char(lexeme[0], tape);			//Devolve o caractere.
	lexeme[0] = 0;							//Limpa o lexema
	return 0;								//Retorna 0 (falso).
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
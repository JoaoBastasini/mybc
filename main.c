//ALUNOS: João Bastasini RA221152067, Julia Amadio 211151041

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    //Para fmemopen, strlen
#include <errno.h>     //Inclui biblioteca para manipulação de erros (Ctrl C)
#include "parser.h"    //Inclui tudo (lexer.h, tokens.h, etc.)
#include "linenoise.h"

/*================ TRATAMENTO DO USO DAS SETAS NO LEITOR DO TERMINAL ================
  A ideia é usar uma biblioteca de edição de linha (como a GNU ReadLine), 
  mas que seja tão pequena que possamos compilar ela junto com o nosso código, 
  ao invés de depender que ela esteja instalada no sistema.

  A biblioteca perfeita para isso se chama linenoise.
  É uma biblioteca minúscula (basicamente um único arquivo .c e um .h) que nos dará:
	- Edição de linha com setas.
    - Histórico (seta para cima/baixo).
    - E o mais importante: zero dependências, zero sudo. 
	  Simplesmente jogamos os arquivos na pasta e eles funcionam.
*/

/*====================== ARQUITETURA DE EXECUÇÃO (REPL) =============================
  Este interpretador funciona em uma arquitetura REPL (Read-Eval-Print Loop)
  desacoplada, onde o "Front-End" (coleta de entrada) é separado do 
  "Back-End" (análise e execução).
  

  O fluxo de dados de uma única linha é:
  
  1. [main.c] -> [linenoise]
  O 'linenoise("mybc> ")' é chamado. Ele lida com a captura da linha, edição 
  (setas, backspace) e histórico (seta para cima).
  Ele nos retorna uma string limpa (ex: "5 + a").
  
  2. [main.c] -> "Ponte com fmemopen"
  Nosso Lexer (lexer.c) foi escrito para ler de um FILE*. Para não reescrevê-lo, 
  criamos uma "ponte":  
  	a. Adicionamos um '\n' na string (ex: "5 + a\n").
	b. 'fmemopen()' cria um FILE* falso em memória a partir da string.
	c. A variável global 'source' (usada pelo lexer) aponta para este FILE* em memória.

  3. [main.c] -> [parser.c] -> [lexer.c]
  	a. Resetamos os contadores de coluna (mas não o de linha).
  	b. Chamamos 'gettoken()' 1x para inicializar o 'lookahead'.
  	c. Chamamos 'mybc()'.

  4. [parser.c] / [lexer.c]
  'mybc()' começa a ser executado. Quando o 'lexer' (via 'next_char')
  pede caracteres, ele os lê do FILE* em memória (via 'source'),
  sem saber (ou se importar) que a fonte não é o teclado.

  5. [main.c]
  Quando 'mybc()' retorna (ao atingir EOF na fonte de memória),
  o 'main.c' fecha (fclose) e libera (free) os buffers daquela linha
  e volta ao passo 1 para pedir a próxima linha.
*/

//Globais que precisam ser resetadas
FILE *source;
extern int lineno;
extern int colno;
extern int colno_before_newline;
extern int lookahead;

int main(void)
{
    char *line_from_user; //Buffer do linenoise
    char *line_with_newline; //Buffer para o fmemopen
    int status; //status de retorno das funções (sucesso ou erro)
    
    printf("Mini-BC Interpretador. Digite 'quit' ou 'exit' para sair.\n");

    //Este é o Loop Principal (REPL - Read-Eval-Print-Loop)
    while (1) {
        line_from_user = linenoise("mybc> ");
        if (line_from_user == NULL) {
            // errno é EAGAIN quando Ctrl C é apertado
            if (errno == EAGAIN) {
                // acaba com o input atual e vai para um próximo limpo (sem dar a resposta ou o erro)
                continue;
            }
            // Ctrl D ou outro erro ainda encerra o programa
            break;
        }

        //Se a linha estiver vazia, apenas continue
        if (line_from_user[0] == 0) {
            linenoiseFree(line_from_user); //linenoise usa sua própria free
            continue;
        }

        //Adiciona a linha ao histórico (para a Seta Cima)
        linenoiseHistoryAdd(line_from_user);


        //A "Ponte" para o Lexer
        //-----------------------------------------------------------------
		
		//O parser/lexer espera um '\n' no final de cada comando.
		line_with_newline = malloc(strlen(line_from_user) + 2); //+1 para \n, +1 para \0
		sprintf(line_with_newline, "%s\n", line_from_user);
		
		//Verifica se a chamada de fmemopen() falhou (ex: por falta de memória).
		source = fmemopen(line_with_newline, strlen(line_with_newline), "r");
		if (source == NULL) {
			//perror() é uma função útil que imprime sua mensagem ("fmemopen")
    		//seguida pela mensagem de erro oficial do sistema (ex: "Cannot allocate memory").
			perror("fmemopen");
			//Sai do programa imediatamente com um código de erro, pois
    		//não podemos continuar se a fonte de entrada não puder ser criada.
			exit(1);
		}


		//Resetando e executando o código
		//-----------------------------------------------------------------

		//Reset estado do lexer para cada nova linha
		colno = 1;
		colno_before_newline = 1;

		//Puxa o primeiro token da nossa fonte em memória
		lookahead = gettoken(source);

		//Chama o parser e verifica se teve erro
		status = mybc();
		
/*
		if (status == ERRTOKEN) {
			//aconteceu um erro. A mensagem já foi mostrada, continua para limpeza e próx linha
			continue;
		}
*/


		//Limpeza
		//-----------------------------------------------------------------
		fclose(source);
		free(line_with_newline);
		linenoiseFree(line_from_user); //Usa a free do linenoise
	}

	//O usuário pressionou Ctrl+D
	printf("\nSaindo.\n");
	return 0;
}
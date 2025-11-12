# Interpretador Mini-BC (mybc)
Implementação de um interpretador simples para expressões aritméticas, baseado no utilitário `bc` do Unix. Foi desenvolvido como um projeto para a disciplina de Compiladores (2º semestre de 2025).

Ele suporta operações aritméticas básicas, precedência de operadores, parênteses, atribuição de variáveis e números em formato decimal, hexadecimal e octal.

## Recursos
- REPL interativo: Um shell `mybc>` que permite entrada interativa.
- Edição de linha: Suporte completo a setas (esquerda, direita), backspace e delete, graças à biblioteca `linenoise`.
- Histórico de comandos: Pressione a seta cima/baixo para navegar pelos comandos anteriores.
- Aritmética: Suporta `+`, `-`, `*`, `/` e `( )` com precedência correta.
- Variáveis: Atribuição com `:=` (ex: `a := 10`) e uso posterior (ex: `a * 2`).
- Formatos numéricos: Entende 123 (DEC), 0x1F (HEX) e 077 (OCT).
- Rastreamento de erro: Reporta token mismatch errors com o número exato da linha e coluna onde o erro ocorreu.

## Requisitos
* Um ambiente de compilação C padrão (ex: `gcc` ou `clang`).
* A ferramenta `make` (presente na maioria dos sistemas Linux/Unix).
* `git` para a aquisição do código-fonte.

O projeto não possui dependências de bibliotecas *externas* que necessitem de instalação prévia (como a `GNU Readline`), pois a biblioteca `linenoise` (para edição de linha) é compilada localmente junto ao código-fonte.

## Compilação e Execução
O processo de compilação e execução deve ser realizado através de um terminal (seja um terminal de sistema ou um terminal integrado de uma IDE).
1. Aquisição do código (`clone`): primeiro, clone o repositório do projeto para sua máquina local:
    ```
    git clone https://github.com/JoaoBastasini/mybc.git
    ```
2. Navegação de diretório: para que o Makefile encontre os arquivos-fonte, navegue para dentro do diretório recém-clonado:
    ```
    cd mybc
    ```
3. Compilação: execute o comando `make` para compilar o projeto. O comando `make clean` é recomendado para garantir que não haja arquivos-objeto (`.o`) antigos. Isso irá compilar todos os módulos (`lexer.c`, `parser.c`, `linenoise.c`, `main.c`) e linká-los no executável final `mybc`.
    ```
    make clean && make
    ```
4. Execução: após a compilação bem-sucedida, execute o interpretador:
    ```
    ./mybc
    ```
5. O prompt `mybc>` será exibido, indicando que o interpretador está pronto para receber comandos. Escreva quaisquer expressões que desejar, podendo utilizar ponto e vírgula (;) como separador caso queira avaliar duas ou mais expressões de uma só vez. Em seguida, pressione Enter.
    ```
    mybc> 10 + 5
    15
    mybc> a := 0x10
    16
    mybc> a * 2 ; 2 + 4
    32
    6
    ```
6. Utilize os tokens `quit` ou `exit` para encerrar a execução.
    ```
    mybc> quit
    Saindo.
    ```

## Arquitetura e fluxo de execução
O fluxo de dados é desacoplado em um "Front-End" (que lida com o input do usuário) e um "Back-End" (que processa a linguagem).

### Fase 1: O "Front-End" (REPL em main.c + linenoise)**
1. O main.c inicia um loop while(1).
2. linenoise("mybc> ") é chamado. A linenoise assume controle total do terminal.
Ela captura as teclas do usuário, lida com as setas (movendo o cursor) e o histórico (seta para cima).
3. Quando o usuário pressiona Enter, a linenoise retorna uma string limpa (ex: "a := 10 + 5").

### Fase 2: A "Ponte" (Conectando Front e Back)**
<br>O "Back-End" (lexer.c) foi projetado para ler de um FILE*, mas o "Front-End" (linenoise) nos deu uma string. O main.c faz a "ponte" entre eles:
1. Uma nova string é alocada, e um \n é adicionado ao final (ex: "a := 10 + 5\n"). Isso é crucial, pois o parser.c espera um \n para finalizar um comando.
2. A função fmemopen() é chamada. Esta é a "mágica": ela cria um FILE* falso que, em vez de ler de um disco, lê da nossa string na memória.
3. A variável global source (que o lexer.c usa) é apontada para este FILE* em memória.

### Fase 3: O "Back-End" (análise em parser.c e lexer.c)
1. O main.c "prepara" o parser chamando gettoken(source) uma vez para carregar o lookahead e chama mybc().
2. O parser (mybc(), cmd(), E(), etc.) começa a pedir tokens. Quando o parser chama gettoken(), o lexer entra em ação.
3. O lexer chama next_char(source). next_char() lê, caractere por caractere, do FILE* em memória (a "ponte").
4. O lexer agrupa os caracteres (ex: isHEX(), isID()) e retorna os tokens (HEX, ID) para o parser.
5. Quando o lexer (via next_char) lê o \n que adicionamos, ele incrementa o contador global lineno.
5. O parser processa a expressão e imprime o resultado.
6. Quando o lexer atinge o fim da string na memória, ele retorna EOF. O mybc() vê o EOF e retorna (sai da função).

### Fase 4: limpeza e repetição
1. O controle volta para o main.c.
2. O main.c chama fclose(source) (para fechar o FILE* da memória) e free() nos buffers de string.
3. O loop while(1) do main.c repete, e voltamos à Fase 1.

## Visão geral dos arquivos
- **main.c**: Lida com o REPL (Read-Eval-Print Loop), o `linenoise`, e a "ponte" `fmemopen`.
- **linenoise.c / .h**: Biblioteca de Front-End. Lida com a interface do usuário (setas, histórico).
- **parser.c / .h**: Contém a lógica da gramática (`mybc`, `cmd`, E), a Tabela de Símbolos (`symtab`) e a função `match()`.
- **lexer.c / .h**: Contém o analisador léxico (`gettoken`, `isID`, `isHEX`, etc.) e o sistema de rastreamento de posição (`next_char`, `unread_char`, `colno`, `lineno`).
- **tokens.h**: O "dicionário". Um enum que define os tokens (ex: `ID`, `HEX`, `ASGN`) para que o lexer e o parser falem a mesma língua.
- **Makefile**: Instruções de compilação.

## Créditos e agradecimentos
Este projeto utiliza a biblioteca `linenoise` para fornecer uma experiência de terminal interativa com edição de linha e histórico. Ela encontra-se disponível no repositório [antirez/linenoise](https://github.com/antirez/linenoise).

A `linenoise` é uma alternativa leve e de arquivo único à `GNU Readline`, criada por **Salvatore Sanfilippo ("antirez")** e **Pieter Noordhuis ("pnoordhuis")**. Ela é distribuída sob a licença BSD 2-Clause e foi incluída diretamente no código-fonte deste projeto para garantir a portabilidade e eliminar dependências externas.
CFLAGS = -g -I$(MYINCLUDE)

MYINCLUDE = .

reloc-list = lexer.o parser.o main.o linenoise.o

#Regra explícita para compilar arquivos .c em .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#Como estamos usando atof() e matemática de ponto flutuante, 
#precisamos linkar a biblioteca de matemática (-lm)
mybc: $(reloc-list)
	$(CC) -o $@ $^ -lm

clean:
	$(RM) $(reloc-list)

mostlyclean: clean
	$(RM) *~

#Entrega
targz:
	tar zcvf mybc_grp1_`date "+%Y%m%d"`.tar.gz Makefile *.[ch] ./versioned
#Se o .tar.gz ainda não existir no diretório: make targz
#Pra extrair: tar zxvf mybc.tar.gz
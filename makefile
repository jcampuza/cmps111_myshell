myshell: myshell.o lex.yy.o
	cc -o myshell myshell.o lex.yy.o -lfl

myshell.o: myshell.c
	cc -c myshell.c

lex.yy.c: shell.l
	flex shell.l

spotless: 
	rm myshell lex.yy.c lex.yy.o myshell.o

clean:
	rm lex.yy.c lex.yy.o myshell.o

add:
	git add myshell.c

CFLAGS = -static-libstdc++ -static-libgcc -w -O2 -Wno-write-strings
#-g表示调试
LEX=flex
YACC=bison
CC=g++
OBJECT=minic

$(OBJECT): lex.yy.o  yacc.tab.o ly.o func.o
	$(CC) $(CFLAGS) lex.yy.o yacc.tab.o ly.o func.o -o $(OBJECT) $(shell pkg-config --cflags --libs libgvc)
	@rm -f  *.o *.tab.c *.tab.h *.yy.c

func.o: func.cpp
	$(CC) $(CFLAGS) -c func.cpp

lex.yy.o: lex.yy.c  yacc.tab.h ly.h
	$(CC) $(CFLAGS) -c lex.yy.c

yacc.tab.o: yacc.tab.c ly.h
	$(CC) $(CFLAGS) -c yacc.tab.c

ly.o: ly.cpp ly.h
	$(CC) $(CFLAGS) -c ly.cpp

yacc.tab.c  yacc.tab.h: yacc.y
	$(YACC) -d yacc.y -Wno-conflicts-sr

lex.yy.c: lex.l
	$(LEX) lex.l

clean:
	@rm -f $(OBJECT)  *.o *.tab.c *.tab.h *.yy.c *.png sym.txt

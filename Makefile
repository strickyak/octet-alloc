all:
	cc -g octet_test.c octet.c && ./a.out

format:
	clang-format --style=Google -i *.h *.c

clean:
	rm -f a.out

ci:
	mkdir -p RCS
	ci -l -m/dev/null -t/dev/null -q *.h *.c Makefile

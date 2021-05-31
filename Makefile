all:
	cc -g octet_test.c octet.c && ./a.out

clean:
	rm -f a.out

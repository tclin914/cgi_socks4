all: ../../public_html/cgi-bin/hw4.cgi

../../public_html/cgi-bin/hw4.cgi: hw4.c
	gcc -o $@ $< 

clean:
	rm ../../public_html/cgi-bin/hw4.cgi

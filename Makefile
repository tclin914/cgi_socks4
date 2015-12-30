all: ../../public_html/cgi-bin/hw3.cgi

../../public_html/cgi-bin/hw3.cgi: hw3.c
	gcc -o $@ $< 

clean:
	rm ../../public_html/cgi-bin/hw3.cgi

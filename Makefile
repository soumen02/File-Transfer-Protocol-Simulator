shell: serverFTP.o clientFTP.o
	gcc  serverFTP.o -o serverFTP
	gcc clientFTP.o -o clientFTP

serverFTP.o: serverFTP.c 
	gcc -c serverFTP.c

clientFTP.o: clientFTP.c 
	gcc -c clientFTP.c

clean:
	rm *.o shell



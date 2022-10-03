#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>   
#include <unistd.h>
#include <stdlib.h>


#define clear() printf("\033[H\033[J")
#define PORT 9007
#define BUFFER_SIZE 2048
#define INPUT_MAX 1024

#define TBUFFER_SIZE 20
#define TOKEN_DELIM " \'\n\"" //Token delimiters for parsing the user input

//helper function to read the input character by character
char *readInput()
{
	int i, j;

	char *line = (char *)malloc(sizeof(char) * INPUT_MAX); // Dynamically Allocate Buffer for the line read
	char c;												   // character that is read at a time
	int indexPosition = 0, bufferSize = 100;
	if (!line) // Buffer allocation failed
	{
		printf("\nFailed allocating Buffer");
		exit(EXIT_FAILURE);
	}
	int len = strlen(line);
		for (int i = 0; i < len; i++)
		{
			if (line[0] == '\'' && line[len - 1] == '\"')
			{
				line[len - 1] = '\0';
				memmove(line, line + 1, len - 1);
			}
		}
	while (1) // Reading all input
	{
		c = getchar();			   // Getting the character from the input and storing it into the variable
		if (c == EOF || c == '\n') // If there is an end of file or a new line, then we overwite it with a the null character
		{

			line[indexPosition] = '\0'; // Replace the character at that possiton with Null
			return line;				// Returning the line
		}								// So basically untill the end of line do everything below or else put the null and return the line string
		else
		{
			line[indexPosition] = c; // line character array (string) at that postion is equal to the character read
		}
		indexPosition++;				 // Move over the character to the next one so its the nexr indexPosition
		if (indexPosition >= bufferSize) // If buffer is exceeded
		{
			bufferSize += INPUT_MAX;						 // Increase the buffer again with (1024)
			line = realloc(line, sizeof(char) * bufferSize); // Reallocate the dynamic memory
			if (!line)										 // Buffer allocation failed
			{
				printf("\nFailed allocating Buffer");
				exit(EXIT_FAILURE);
			}
		}
	}

}

//helper function to parse over the input ardument and split them into strings based on TOKEN_DELIM
char **tokenizer(char *line) // Using the line (read line) passed as an argument 
{
  int bufferSize = TBUFFER_SIZE, indexPosition = 0; // TBUFFER_SIZE here is 32 indicating the number the arguments read in 
  char **tokens = malloc(bufferSize * sizeof(char*)); // Array of tokens dynamically allocated
  char *token; // Each token

  if (!tokens) { // If tokens memory wasnt allocated 
    fprintf(stderr, "Token memory allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOKEN_DELIM); // Strtok breaks string line into a series of tokens using the token delimiters provided as Macro
  while (token != NULL) { // Till the last is not null 
    tokens[indexPosition] = token; // place the token into the array of tokens
    indexPosition++; // Increment the index postion for the token to be saved in the array

    if (indexPosition >= bufferSize) { // If index position is greater than the buffer 
      bufferSize += TBUFFER_SIZE; // Increase the buffer
      tokens = realloc(tokens, bufferSize * sizeof(char*)); // Reallocate more memory 
      if (!tokens) { //If tokens memory wasnt allocated 
        fprintf(stderr, "Token memory allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOKEN_DELIM); // Strtok NULL tells the function to continue tokenizing the string that was passed first
  }

  tokens[indexPosition] = NULL; // last postion to be Null 

  return tokens; // return the array of tokens (arguments)
}

// port variable that is global and keeps track of the port assigned by the user or OS
int ftp_port;

//helper function to let system sleep for a few milliseconds. 
//We used this command between two consecutive send() functions because, the later send() does not send the data correctly half the time if there is no delay
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

//helper function to execute local commands like pwd, list, cwd
void commandrunner(char* command, char** tokens)
{   
    if (strcmp(tokens[0],"!PWD") == 0)
    {
        system("pwd");
    }

    if (strcmp(tokens[0],"!LIST") == 0)
    {
        system("ls");
    }
    if (strcmp(tokens[0],"!CWD") == 0)
    {   
        if (chdir(tokens[1])==-1)
        {
            perror(" Error changing directory");
        }
        else{
            printf("200 directory changed to \n");
            system("pwd");
        }
    }
}


int main()
{       
    char bufferc[BUFFER_SIZE];

    int loginFlag = 0;

    //clears stdout 
    clear();

    printf("\n ----------------| FTP Client |----------------");
	
    //create a socket for control connection
	int network_socket;
	network_socket = socket(AF_INET , SOCK_STREAM, 0);

	//check for error in socket creation
	if (network_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

	//setsockopt allows the OS to reuse PORTS and prevent bind errors
	int value  = 1;
	setsockopt(network_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
	
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	//connect to server on PORT and check for error
    if(connect(network_socket,(struct sockaddr*)&server_address,sizeof(server_address))<0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
	printf("\n Successfully connected to server ...\n");
	char buffer[1024];

    // code snippet that finds a port that is free to use and assigns it to the global FTP port variable
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(network_socket, (struct sockaddr *)&sin, &len) == -1)
        perror("getsockname");
    else
        ftp_port = htons(sin.sin_port);

	while(1)
	{
        char* command;
        int rcv_flag = 1;
        int quitflag = 0;

		//request user for command
        printf("\n ftp>");
         // If the command is empty meaning the user has pressed enter then we continue and loop back
        command = readInput();
        if (command[0] == '\0'){
            continue;
        }
      

        char *rawcommand = malloc(strlen(command) + 1);
        strcpy(rawcommand, command);

        //tokenize command entered by user
        char **tokens = tokenizer(command);

        //code for commands starting with "!" are redirected to commandexecute function()
        if (strstr(tokens[0], "!") == tokens[0]) {
            if (!(strcmp(tokens[0],"!CWD")==0) && !(strcmp(tokens[0],"!PWD")==0) && !(strcmp(tokens[0],"!LIST")==0))
            {
                printf("202 Command not implemented\n");
                continue;
            }
            commandrunner(command, tokens);
            continue;
        }
        
        //USER and PASS commands to authenticate the client 
        if((strcmp(tokens[0],"USER")==0) || (strcmp(tokens[0],"PASS")==0))
        {
            send(network_socket,rawcommand,strlen(rawcommand)+1,0);
        }
        //PORT command to explicitly set the port
        else if (strcmp(tokens[0],"PORT")==0)
        {    
            ftp_port = atoi(tokens[1]);
            printf("\n200 PORT command successful.");
            rcv_flag = 0;
        }

        //STOR command stores a file from the local directory to the server's directory
        else if ((strcmp(tokens[0],"STOR")==0) && (loginFlag == 1))
        {
            
            //open file and check for its validity
            char filename[256];
            strcpy(filename, tokens[1]);
            FILE* file = fopen(filename, "rb");//file is opened and read in binary mode so any type of fine can be transferred
            if(file == NULL){
                printf(" File does not exist.\n");
                rcv_flag = 0;

            }
            else{
                
                //port is incremented and changed to string to send to server
                char ftp_port_str[16];
                ftp_port+=1;

                sprintf(ftp_port_str, "%d", ftp_port); 
                
                // printf("%d", ftp_port);

                //send the command to the user
                send(network_socket,rawcommand,strlen(rawcommand)+1,0);
                msleep(50); //sleep so the packets are not misplaced bertween sends()
                //sending port
                send(network_socket, ftp_port_str, 16, 0);

                //receive port command successful and print
                char portBuff[256];
                bzero(portBuff, 256);                        // Clearing the buffer back to the buffer size
                recv(network_socket, portBuff, sizeof(portBuff), 0); // Client receiving the buffer output from the server
                printf(" %s\n", portBuff);   
                
                //create FTP socket and check for error
                int FTP_socket = socket(AF_INET,SOCK_STREAM,0);
                if(FTP_socket<0)
                {
                    perror("socket:");
                    exit(EXIT_FAILURE);
                }

                //setsockopt allows the OS to reuse PORTS and prevent bind errors
                int value  = 1;
                setsockopt(FTP_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
                
                //define server address structure
                struct sockaddr_in server_address;
                bzero(&server_address,sizeof(server_address));
                server_address.sin_family = AF_INET;
                server_address.sin_port = htons(ftp_port);
                server_address.sin_addr.s_addr = INADDR_ANY;

                //bind to the port sent to the server
                if(bind(FTP_socket, (struct sockaddr*)&server_address,sizeof(server_address))<0)
                {
                    perror("bind failed");
                    exit(EXIT_FAILURE);
                }

                //listen for server connection
                if(listen(FTP_socket,5)<0)
                {
                    perror("listen failed");
                    close(FTP_socket);
                    exit(EXIT_FAILURE);
                }

                //accept connection
                int server_sd = accept(FTP_socket,0,0);

                //when connection is established, file transfer begins
                while (1)
                {
                    unsigned char storBuff[1024]={0};
                    int bytesRead = fread(storBuff, 1, 1024, file);

                    if(bytesRead > 0)
                    {
                        //receive bytes and store in buffer
                        write(server_sd, storBuff, bytesRead);
                    }

                    //once bytes read does not fill buffer, it is the last portion of file
                    if (bytesRead < 1024)
                    {
                        break;
                    }
                }

                //closing files, sockets, and clearing stdout
                fclose(file);
                close(server_sd);
                close(FTP_socket);
                fflush(stdout);
            }

        }
        //RETR to retrieve a file from the server to client directory
        else if ((strcmp(tokens[0],"RETR")==0)&& (loginFlag == 1))
        {   
            //port is incremented and changed to string to send to server
            char ftp_port_str[16];
            ftp_port += 1;
            sprintf(ftp_port_str, "%d", ftp_port); 
            
            // printf("$$$ %s", ftp_port_str);

            //send the command to the user
            send(network_socket,rawcommand,strlen(rawcommand)+1,0);
            msleep(50); //sleep so the packets are not misplaced between sends()
            //sending port
            send(network_socket, ftp_port_str, 16, 0);

            //receive port command successful and print
            char portBuff[256];
            bzero(portBuff, 256);                        // Clearing the buffer back to the buffer size
            recv(network_socket, portBuff, sizeof(portBuff), 0); // Client receiving the buffer output from the server
            printf(" %s\n", portBuff);
            
            //create FTP socket and check for error
            int FTP_socket = socket(AF_INET,SOCK_STREAM,0);
            
            //check for fail error
            if(FTP_socket<0)
            {
                perror("socket:");
                exit(EXIT_FAILURE);
            }

            //setsockopt allows the OS to reuse PORTS and prevent bind errors
            int value  = 1;
            setsockopt(FTP_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
            
            //define server address structure
            struct sockaddr_in server_address;
            bzero(&server_address,sizeof(server_address));
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(ftp_port);
            server_address.sin_addr.s_addr = INADDR_ANY;

            //bind to the port sent to the server
            if(bind(FTP_socket, (struct sockaddr*)&server_address,sizeof(server_address))<0)
            {
                perror("bind failed");
                close(FTP_socket);
                exit(EXIT_FAILURE);
            }

            //listen for server connection
            if(listen(FTP_socket,5)<0)
            {
                perror("listen failed");
                close(FTP_socket);
                exit(EXIT_FAILURE);
            }

            //accept connection
            int server_sd = accept(FTP_socket,0,0);
            printf("Server conected for FTP ...\n");
            

            char filehealth[256];
            bzero(filehealth, 256);                        // Clearing the buffer back to the buffer size
            recv(network_socket, filehealth, sizeof(filehealth), 0); // Client receiving the buffer output from the server
            // printf(" %s\n", filehealth);

            if (strcmp(filehealth,"notfound")!=0)
            {
            //open file and check for its validity
            char filename[256];
            strcpy(filename, tokens[1]);
            FILE* file;

            int bytesRead=0;
            char readBuff[1024];
            memset(readBuff, '0', sizeof(readBuff));
            file=fopen(filename, "wb"); //file is opened and read in binary mode so any type of fine can be transferred

            if(!file){
                printf("Could not open file");
                fclose(file);
                close(server_sd);
                remove(tokens[1]);
                close(FTP_socket);
                exit(1);
            }
            else
            {
                //file is written as long as chunks of data is received
                while ((bytesRead = read(server_sd, readBuff, 1024)) >0)
                {
                    fflush(stdout);
                    fwrite(readBuff, 1, bytesRead, file);
                    // printf("%d", bytesRead);
                }
                
            }
            fclose(file);

            }
            
            //closing files, sockets, and clearing stdout
            close(server_sd);
            close(FTP_socket);
                
        }
        //LIST is used to list the directory contents of the server 
        else if ((strcmp(tokens[0],"LIST")==0) && (loginFlag == 1))
        {
            //port is incremented and changed to string to send to server
            char ftp_port_str[16];
            ftp_port += 1;

            sprintf(ftp_port_str, "%d", ftp_port); 
            
            // printf("~%s", ftp_port_str);
            
            fflush(stdout);

            //send the command to the user
            send(network_socket,rawcommand,strlen(rawcommand)+1,0);
            msleep(50);//sleep so the packets are not misplaced between sends()
            //sending port
            send(network_socket, ftp_port_str, 16, 0);
            
            //receive port command successful and print
            char portBuff[256];
            bzero(portBuff, 256);                        // Clearing the buffer back to the buffer size
            recv(network_socket, portBuff, sizeof(portBuff), 0); // Client receiving the buffer output from the server
            printf(" %s\n", portBuff); 
            
            //create FTP socket and check for error
            int FTP_socket = socket(AF_INET,SOCK_STREAM,0);
            
            //check for fail error
            if(FTP_socket<0)
            {
                perror("socket:");
                exit(EXIT_FAILURE);
            }

            //setsockopt allows the OS to reuse PORTS and prevent bind errors
            int value  = 1;
            setsockopt(FTP_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
            
            //define server address structure
            struct sockaddr_in server_address;
            bzero(&server_address,sizeof(server_address));
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(ftp_port);
            server_address.sin_addr.s_addr = INADDR_ANY;


            //bind to the port sent to the server
            if(bind(FTP_socket, (struct sockaddr*)&server_address,sizeof(server_address))<0)
            {
                perror("bind failed");
                exit(EXIT_FAILURE);
            }

            //listen for server connection
            if(listen(FTP_socket,5)<0)
            {
                perror("listen failed");
                close(FTP_socket);
                exit(EXIT_FAILURE);
            }

            //accept connection
            int server_sd = accept(FTP_socket,0,0);
                printf("Server conected for FTP ...\n");

            char list_message[256]; //to receive server's message 
            bzero(list_message,sizeof(list_message));
            printf("\r");
            while(recv(server_sd,list_message,sizeof(list_message),0) != 0)
            {
                //read message from socket and print
                printf("%s\n",list_message);
                fflush(stdout);
            }

            //close socket connections 
            close(server_sd);
            close(FTP_socket);
        }

        //send command to server for CWD and PWD
        else if((strcmp(tokens[0],"CWD")==0) || (strcmp(tokens[0],"PWD")==0) )
        {
            send(network_socket,rawcommand,strlen(rawcommand)+1,0);
        }

        //QUIT to safely terminate the client connection to server
        else if(strcmp(tokens[0],"QUIT")==0)
        {
            
            send(network_socket,rawcommand,strlen(rawcommand)+1,0);
            quitflag = 1;
        }

        //If input does not match any commands
        else
        {
            if ((loginFlag == 0) && ( (strcmp(tokens[0],"STOR")==0) || (strcmp(tokens[0],"RETR")==0) || (strcmp(tokens[0],"LIST")==0)))
            {
                printf(" 530 Not logged in.\n");
                continue;
            }
            rcv_flag = 0;
            printf(" 202 Command not implemented.\n");
        }

        //if any effective command was received
        //prevents client from getting stuck
        if (rcv_flag == 1)  
        {
            bzero(bufferc, BUFFER_SIZE);                        // Clearing the buffer back to the buffer size
            recv(network_socket, bufferc, sizeof(bufferc), 0); // Client receiving the buffer output from the server
            
            if (strcmp(bufferc,"230 User logged in, proceed.")==0)
            {
                loginFlag = 1;
            }
            
            printf(" %s\n", bufferc);                              // print the buffer from the server on the client screen
            bufferc[0] = '\0';

        }
        //executed on QUIT
        if (quitflag == 1)
        {
            close(network_socket);
            exit(EXIT_SUCCESS);
        }

        //free dynamically allocated variables
        free(command);
        free(tokens);
        free(rawcommand);
	}

	return 0;
}
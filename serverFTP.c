#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>   

#define clear() printf("\033[H\033[J")
#define PORT 9007
#define MAXUSER 100
#define MAX_BUFFER 2048
#define INPUT_MAX 1024
#define BUFFER_SIZE 20
#define TOKEN_DELIM " \'\n\"" //Token delimiters for parsing the user input

//helper function to parse over the input ardument and split them into strings based on TOKEN_DELIM
char **tokenizer(char *line) // Using the line (read line) passed as an argument 
{
  int bufferSize = BUFFER_SIZE, indexPosition = 0; // BUFFER_SIZE here is 32 indicating the number the arguments read in 
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
      bufferSize += BUFFER_SIZE; // Increase the buffer
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

//helper function to let system sleep for a few milliseconds.
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

//global integer to keep track of number of users in the record (parsed from Authentication file)
int listSize = 0;

//Sturct to keep track of user variables and flags
struct User
{
	char username[16];
	char password[16];
	int usernameFlag;	//flag for checking if user has passed username authentication
	int loginFlag;
	int userFD;
	char path[256];		//full path for user's current working directory

};

//List of users kept in a stuct variable
struct User userList[MAXUSER];

//helper funciton to assign every user in the Auth file, (similar to a constructor)
struct User assignUser(char* username, char* password)
{
	struct User tempUser;
	char cwd[256];

	//default username and login state is 0, default FD is -1
	tempUser.usernameFlag = 0;
	tempUser.loginFlag = 0;
	tempUser.userFD = -1;

	//server path assigned by default
	if (getcwd(cwd, sizeof(cwd)) == NULL){
		perror("Error getting working directory..");	
	}
	else{
		strcpy(tempUser.path,cwd);
	}

	strcpy(tempUser.username,username);
	strcpy(tempUser.password,password);

	return tempUser;
}

//helper function to retrieve usernames and passwords from file and create the structure list
void getAuth()
{
	FILE *file = fopen("user.txt", "r"); 
    char *line = NULL; 
    size_t len = 0; 
    ssize_t lineSize;

	//error check for opening file
	if (file == NULL) { 
        perror("fopen"); 
        exit(EXIT_FAILURE); 
    }

	//iterate through lines
	while ((lineSize = getline(&line, &len, file)) != -1) {
		char **arg;
		arg = tokenizer(line);
		userList[listSize] = assignUser(arg[0], arg[1]);
		listSize++;
    }

	printf("\n Loaded authentication file ...");
}

//function for authorizing username
int userAuth(const char* username, int usernumber)
{
	//iterate through usernames in user list to find matches
	for (int i=0; i<listSize; i++)
	{	
		//username found
		if ((strcmp(username,userList[i].username)==0)&&(userList[i].userFD == -1))	//(FD==-1) is to ensure user is not already logged in
		{																			//(avoid multiple logins to same acccount)
			userList[i].userFD = usernumber;
			userList[i].usernameFlag = 1;
			printf("\n User found..");
			
			return 1;
		}
	}
		printf("\n User not found..");
		return 0;
}

//function for authorizing password
int passAuth(const char* password, int userNum)
{
	if (strcmp(password,userList[userNum].password)==0)
	{
		userList[userNum].loginFlag = 1;
		printf("\n Password correct..");
		return 1;
	}
	else
	{
		printf("\n Incorrect password..");
		return 0;
	}
}

//helper function for sending data (helpful for testing and debugging in a single line)
void send_to_client(int fd, char* message)
{
	int size = strlen(message) + 1;
	send(fd, message, size, 0);
}

//helpper function for debugging by printing the records
void printRecords(){
	for(int i = 0; i < listSize; ++i)
	{
			printf("\n || User: %s | Pass: %s | USER: %d | PASS: %d | FD: %d||", userList[i].username, userList[i].password, userList[i].usernameFlag, userList[i].loginFlag, userList[i].userFD);
			printf("\n PATH : %s", userList[i].path);
	}
	fflush(stdout);
}

int main()
{	
	clear();
	printf("\n ----------------| FTP Server |----------------");
	printf("\n Waiting for Client to join...");

	//read user.txt file
	getAuth();

	//get the default path for assignemnt
	char defaultPath[256];
	if (getcwd(defaultPath, sizeof(defaultPath)) == NULL){
		perror("Error getting working directory..");	
	}
	
	//create server socket and check for failure
	int server_socket = socket(AF_INET,SOCK_STREAM,0);
	if(server_socket<0)
	{
		perror("socket:");
		exit(EXIT_FAILURE);
	}

	//setsockopt allows the OS to reuse PORTS and prevent bind errors
	int value  = 1;
	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
	
	//define server address structure
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	//bind for binding the server to port
	if(bind(server_socket, (struct sockaddr*)&server_address,sizeof(server_address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	//listen for client connection to  port
	if(listen(server_socket,5)<0)
	{
		perror("listen failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}
	

	//DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
	fd_set all_sockets;
	fd_set ready_sockets;


	//zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	//adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_socket,&all_sockets);

	while(1)
	{	

		ready_sockets = all_sockets;
		if(select(FD_SETSIZE,&ready_sockets,NULL,NULL,NULL)<0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}

		//iterate through FDs to check which are ready
		for(int fd = 0 ; fd < FD_SETSIZE; fd++)
		{
			//check to see if that fd is SET
			if(FD_ISSET(fd,&ready_sockets))
			{
				//fd is server socket, new connection to accept
				if(fd==server_socket)
				{
					//accept new connection
					int client_sd = accept(server_socket,0,0);
					printf("\n Client Connected on file descriptor %d ...",client_sd);
					
					//add new socket to set of all sockets monitored
					FD_SET(client_sd,&all_sockets);
					fflush(stdout);
				}

				//socket already in all_sockets fd_set, reading data from socket only (no new connection)
				else
				{
					char buffer[MAX_BUFFER];
					bzero(buffer,sizeof(buffer));
					int bytes = recv(fd, buffer,sizeof(buffer), 0);
					printf("\n SOCKET %d : %s", fd, buffer);
					fflush(stdout);	

					//flags for every client instance
					int user_flag = 0, login_flag=0, recordNum = -1;
					
					//loop to iterate records and check if client is already connected to one of the records
					//above declared flags are updated on record found
					for (int i=0; i<listSize; i++){

						if (userList[i].userFD == fd)
						{
							// username linked to client 
							recordNum = i;							
							userList[recordNum].usernameFlag = 1;
							user_flag = 1;

							if (userList[recordNum].loginFlag == 1)
							{
								login_flag = 1;
							}
						} 
					}

					// if bytes are 0, client has quit 
					if (bytes!=0)
					{
					
					chdir(userList[recordNum].path);
					char *buffer_cpy = malloc(strlen(buffer) + 1);
    				strcpy(buffer_cpy, buffer);

					//tokenize buffer to separate command items
					char** commandToken = tokenizer(buffer_cpy);
					
					//first check if the user is logged in, go straight to if and allow commands execution
					if (login_flag == 0)
					{
						//if user flag is 0, username is not connected to any record, else, ask for password
						if(user_flag == 0)
						{
							if (strcmp(commandToken[0], "USER") == 0)
							{
								int user_status = userAuth(commandToken[1], fd);
								
								if (user_status == 1)
								{
									user_flag = 1;
									send_to_client(fd, "331 Username OK, need passsword.");
								}
								else if (user_status == 0)
								{
									send_to_client(fd, "530 Not logged in.");
								}
							}
							else{
								send_to_client(fd, "530 Not logged in.");
							}
						}
						else
						{
							if (strcmp(commandToken[0], "PASS") == 0)
							{
								//first check if username auth is passsed and pass in user number
								int pass_status = passAuth(commandToken[1], recordNum);	

								if (pass_status == 1)
								{
									login_flag = 1;
									send_to_client(fd, "230 User logged in, proceed.");
								}
								else if (login_flag == 0)
								{
									send_to_client(fd, "530 Not logged in.");
								}							}
							else{
								send_to_client(fd, "530 Not logged in.");
							}
						}
					}
					
					else
					{	
						//CWD to change server directory 
						if (strcmp(commandToken[0], "CWD") == 0)
						{
							if (chdir(commandToken[1])==-1)
							{
								perror("\n chdir: ");
								send_to_client(fd, " Error changing server directory");
							}
							else
							{
								char tempPath[256];
								if (getcwd(tempPath, sizeof(tempPath)) == NULL){
									perror("\ngetcwd:");
								}
								else{
									strcpy(userList[recordNum].path, tempPath);
        							printf("\n Server changed directory ...");
									char send[128] = "200 directory changed to \n";
									strcat(send, userList[recordNum].path);
									send_to_client(fd, send);
								}
							}
						}
						//PWD to print working directory of the server
						else if (strcmp(commandToken[0], "PWD") == 0)
						{	
							//directly sends saved record directory to the client
							send_to_client(fd, userList[recordNum].path);
						}
						//STOR allows the client to send a file to the server
						else if (strcmp(commandToken[0], "STOR") == 0)
						{
							//initialize port variables to accept port from server
							int ftp_port;
							char ftp_port_str[16];
							bzero(ftp_port_str, 16);  
							recv(fd, ftp_port_str, sizeof(ftp_port_str), 0);
							ftp_port = atoi(ftp_port_str);
							send_to_client(fd, "200 PORT command successful.");

							//create child process for FTP
							pid_t child = fork();
							if (child == 0)
							{
								
								//create FTP socket and check for failure
								int FTP_server_socket;
								FTP_server_socket = socket(AF_INET , SOCK_STREAM, 0);
								if (FTP_server_socket == -1) {
									printf("socket creation failed..\n");
									exit(EXIT_FAILURE);
								}

								//setsockopt allows the OS to reuse PORTS and prevent bind errors
								int value  = 1;
								setsockopt(FTP_server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
								
								//define FTP server address structure
								struct sockaddr_in FTP_server_address;
								bzero(&FTP_server_address,sizeof(FTP_server_address));
								FTP_server_address.sin_family = AF_INET;
								FTP_server_address.sin_port = htons(ftp_port);
								FTP_server_address.sin_addr.s_addr = INADDR_ANY;

								//sleep for 250 milliseconds to give client enough time to bind to its port
								msleep(250);
								
								//connect to the port client is listening to
								if(connect(FTP_server_socket,(struct sockaddr*)&FTP_server_address,sizeof(server_address))<0)
								{
									perror("connect");
									exit(EXIT_FAILURE);
								}

								// create file variables and open file in binary mode
								char filename[256];
								strcpy(filename, commandToken[1]);
								int bytesRead = 0;
								char readBuffer[1024];
								FILE* file = fopen(filename, "wb");

								//error check for opening file
								if(!file){
									printf("\nCould not open file..");
									send_to_client(fd, "550 No such file or directory.");
									fclose(file);
									close(FTP_server_socket);
									fflush(stdout);
									exit(1); 
								}
								else
								{
									//keep writing to file until no new data is available
									while ((bytesRead=read(FTP_server_socket, readBuffer, 1024)) > 0)
									{
										fflush(stdout);
										fwrite(readBuffer, 1, bytesRead, file);
									}

									// send_to_client(fd, "226 Transfer completed.");
								}

								printf("\nFile creation complete.");
								send_to_client(fd, "226 Transfer completed ...");

								//close file, sockets and exit child process
								fclose(file);
								close(FTP_server_socket);
								fflush(stdout);
								exit(1);

							}
						}
						else if (strcmp(commandToken[0], "RETR") == 0)
						{
							//initialize port variables to accept port from server
							int ftp_port;
							char ftp_port_str[16];
							bzero(ftp_port_str, 16);  
							recv(fd, ftp_port_str, 16, 0);
							ftp_port = atoi(ftp_port_str);
							send_to_client(fd, "200 PORT command successful.");
							
								//create child process for FTP
								pid_t child = fork();
								if (child == 0)
								{
									//create FTP socket and check for failure
									int FTP_server_socket;
									FTP_server_socket = socket(AF_INET , SOCK_STREAM, 0);
									if (FTP_server_socket == -1) {
										printf("socket creation failed..\n");
										exit(EXIT_FAILURE);
									}

									//setsockopt allows the OS to reuse PORTS and prevent bind errors
									int value  = 1;
									setsockopt(FTP_server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
								
									//define FTP server address structure
									struct sockaddr_in FTP_server_address;
									bzero(&FTP_server_address,sizeof(FTP_server_address));
									FTP_server_address.sin_family = AF_INET;
									FTP_server_address.sin_port = htons(ftp_port);
									FTP_server_address.sin_addr.s_addr = INADDR_ANY;



									//sleep for 250 milliseconds to give client enough time to bind to its port
									msleep(250);

									//connect to the port client is listening to
									if(connect(FTP_server_socket,(struct sockaddr*)&FTP_server_address,sizeof(server_address))<0)
									{
										perror("connect");
										exit(EXIT_FAILURE);
									}

									// create file variables and open file in binary mode
									char filename[256];
            						strcpy(filename, commandToken[1]);
									FILE* file = fopen(filename, "rb");
									if(file==NULL){
										printf("\n File does not exist..");
										send_to_client(fd, "notfound");
										close(FTP_server_socket);
										msleep(50);
										char* response = "550 No such file or directory.";
										send_to_client(fd, response);
										exit(1);


									} //file found, begin retreival
									else
									{
										send_to_client(fd, " ");
										while(1)
										{
											unsigned char buff[1024]={0};
											int bytesRead = fread(buff, 1, 1024, file);
											
											//read the file in chunks and write to socket
											if(bytesRead>0)
											{
												write(FTP_server_socket, buff, bytesRead);
											}
											
											if (bytesRead < 1024)	//buffer not filled, last chunk of data sent
											{
												break;
											}
										}
										//close open sockets and files
										close(FTP_server_socket);
										fclose(file);		
									
									}	                        

									//send success message and exit child
									send_to_client(fd, "226 Transfer Completed ...");
									exit(1);
								}


						}
						else if (strcmp(commandToken[0], "LIST") == 0)
						{
							//initialize port variables to accept port from server
							int ftp_port;
							char ftp_port_str[16];
							bzero(ftp_port_str, 16);  
							recv(fd, ftp_port_str, 16, 0);
							fflush(stdout);
							ftp_port = atoi(ftp_port_str);
							send_to_client(fd, "200 PORT command successful.");

							printf("\n connected port:%d \n", ftp_port);
							
							//create child process for FTP
							pid_t child = fork();

							//only child process handles data transfer
							if (child == 0)
							{
								//create FTP socket and check for failure
								int FTP_server_socket;
								FTP_server_socket = socket(AF_INET , SOCK_STREAM, 0);
								if (FTP_server_socket == -1) {
									printf("socket creation failed..\n");
									exit(EXIT_FAILURE);
								}

								//setsockopt allows the OS to reuse PORTS and prevent bind errors
								int value  = 1;
								setsockopt(FTP_server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value));
								
								//define FTP server address structure
								struct sockaddr_in FTP_server_address;
								bzero(&FTP_server_address,sizeof(FTP_server_address));
								FTP_server_address.sin_family = AF_INET;
								FTP_server_address.sin_port = htons(ftp_port);
								FTP_server_address.sin_addr.s_addr = INADDR_ANY;

								//sleep for 250 milliseconds to give client enough time to bind to its port
								msleep(250);

								//connect to the port client is listening to
								if(connect(FTP_server_socket,(struct sockaddr*)&FTP_server_address,sizeof(server_address))<0)
								{
									char* send = "unable to connect to ";
									strcat(send, ftp_port_str);
									send_to_client(fd, send);
									perror("connect");
									exit(EXIT_FAILURE);
								}


								char file_line[256];
								//creating a pipe to send back the list of files in the server directory to the client
								FILE *list_file = popen("ls", "r");
								if (list_file)
								{
									while (fgets(file_line, sizeof(file_line), list_file))
									{
										if (send(FTP_server_socket,file_line,strlen(file_line),0) < 0)
										{
											perror("Error Sending file..\n");
											return 0;
										}
										bzero(&file_line,sizeof(file_line));
										fflush(stdout);
										
									}
									pclose(list_file);
								}
								//send success message, close socket and terminate child 
								send_to_client(fd,"226 Transfer completed...");
								
								close(FTP_server_socket);
								exit(1);
							}
						}
						//QUIT safely closes client control connection and resets record variables
						else if (strcmp(commandToken[0], "QUIT") == 0)
						{
							send_to_client(fd, " 221 Service closing control connection...");
							printf("\n Client on sock %d disconnected...", fd);
						
							userList[recordNum].loginFlag = 0;
							userList[recordNum].usernameFlag = 0;
							userList[recordNum].userFD = -1;
							strcpy(userList[recordNum].path,defaultPath);

							close(fd);

							//once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
							FD_CLR(fd,&all_sockets);

						}
						else
						{
							send_to_client(fd, "202 Command not implemented.");
						}
					}

					//free the pointer variables
					free(buffer_cpy); 	
					free(commandToken);

					// printRecords();
					
					fflush(stdout);
				}

				else   //client has closed the connection using ^C, do the same as QUIT
				{
					printf("\n Client on sock %d disconnected... \n", fd);
					
					userList[recordNum].loginFlag = 0;
					userList[recordNum].usernameFlag = 0;
					userList[recordNum].userFD = -1;
					strcpy(userList[recordNum].path,defaultPath);

					//we are done, close fd
					close(fd);

					//once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
					FD_CLR(fd,&all_sockets);
				}				
			}
		}
	}
}

	//close server socket
	close(server_socket);
	return 0;
}

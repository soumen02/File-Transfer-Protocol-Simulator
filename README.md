# File-Transfer-Protocol-Simulator
### A simple FTP implementation between a simulated server and a client that can transfer files locally.

This computer netwroks project involved building an FTP application that uses real FTP commands for communication with an FTP server. Once logged in, it allows the user to store and retrieve files from the server's directory. The port used can also be set manually. 

_auth.txt_ contains the usernames and passwords that the server accepts. 

Multiple clients can requests for files at the same time. 

#### How to run the program 
- Install GNU C++
- Use `make clean` and then `make` to recompile all the files.
- Run the executable created using `./filename` where filename is *Server* and *Client*

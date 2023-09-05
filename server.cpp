#include <functional>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>    
#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <iostream>
#include <fstream>

using namespace std;
constexpr int max_buffer_size = 1024;
constexpr int listen_port = 8822;
char cmd[max_buffer_size];
char buffer[max_buffer_size];

int main(int argc, char *argv[]){
	string machine_number = argv[1];
	if(argc != 2){
		perror("FATAL: please assign machine number!");
		exit(1);
	}
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
		perror("FATAL: socket build fail");
		exit(1);
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=listen_port;
	srv.sin_addr.s_addr=htonl(INADDR_ANY);
	if((bind(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		perror("FATAL: socket bind fail");
		exit(1);
	}
	if(listen(fd,10)<0) {
		perror("FATAL: socket listen fail");
		exit(1);
	}
	struct sockaddr_in cli;
	int clifd;
	socklen_t cli_len = sizeof(cli);
	printf("Server %s lisening on port: %d\n", argv[1], listen_port);
	while(1){
		clifd=accept(fd, (struct sockaddr*) &cli, &cli_len);//block until accept connection
		if(clifd<0){
			perror("FATAL: socket accept fail");
			exit(1);
		}
		int nbytes;
		if((nbytes=recv(clifd, cmd, sizeof(cmd),0))<0){//"read" will block until receive data 
			perror("FATAL: socket read fail");
			exit(1);
		}
		// excute the command the save the result to a temporary file
		unique_ptr<FILE, decltype(&pclose)> pipe(popen(strcat(cmd, (" machine."+ machine_number + ".log").c_str()), "r"), pclose);
		if (!pipe) {
			perror("FATAL: popen() failed!");
			exit(0);
		}
		int line_cnt = 0;
		ofstream tempLogFile("grep_tmp_"+ machine_number + ".log");
		while (fgets(buffer, max_buffer_size, pipe.get()) != nullptr) {
			line_cnt++;
			tempLogFile << buffer;
		}
		tempLogFile.close();
		sprintf(buffer, "%d", line_cnt);
		// send the number of lines first
		if((nbytes=send(clifd, buffer, sizeof(buffer),0))<0){
			perror("FATAL: socket write fail");
		}
		ifstream readLogFile("grep_tmp_"+ machine_number + ".log");
		if(!readLogFile){
			perror("FATAL: cannot open temp file");
		}
		//send the grep result
		while (readLogFile.getline(buffer, max_buffer_size)) {
			if((nbytes=send(clifd, buffer, sizeof(buffer),0))<0){
				throw("FATAL: socket write fail");
			}
			cout << "send " << buffer << endl;;
			memset(buffer,0,sizeof(buffer));
		}
		close(clifd);
	}
	close(fd);
	return 0;
}



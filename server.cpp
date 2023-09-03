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
constexpr int max_buffer_size = 1e4 + 7;
constexpr int listen_port = 8821;
char cmd[max_buffer_size];
char buffer[max_buffer_size];

int main(int argc, char *argv[]){
	string machine_number = argv[1];
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
		perror("socket build fail");
		exit(1);
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=listen_port;
	srv.sin_addr.s_addr=htonl(INADDR_ANY);
	if((bind(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		perror("socket bind fail");
		exit(1);
	}
	if( listen(fd,10)<0 ){
		perror("socket listen fail");
		exit(1);
	}
	struct sockaddr_in cli;
	int clifd;
	socklen_t cli_len=sizeof(cli);
	while(1){
		clifd=accept(fd, (struct sockaddr*) &cli, &cli_len);//block until accept connection
		if(clifd<0){
			perror("socket accept fail");
			exit(1);
		}
		int nbytes;
		if((nbytes=read(clifd, cmd, sizeof(cmd)))<0){//"read" will block until receive data 
			perror("socket read fail");
		}

		
		unique_ptr<FILE, decltype(&pclose)> pipe(popen(strcat(cmd, (" machine."+ machine_number + ".log").c_str()), "r"), pclose);
		if (!pipe) {
			throw std::runtime_error("popen() failed!");
		}
		int line_cnt = 0;
		ofstream tempLogFile("grep_tmp_"+ machine_number + ".log");
		while (fgets(buffer, max_buffer_size, pipe.get()) != nullptr) {
			line_cnt++;
			tempLogFile << buffer;
		}
		tempLogFile.close();
		sprintf(buffer, "%d", line_cnt);
		if((nbytes=write(clifd, buffer, sizeof(buffer)))<0){// send the number of lines first
			perror("socket write fail");
		}
		ifstream readLogFile("grep_tmp_"+ machine_number + ".log");
		if(!readLogFile){
			throw("cannot open temp file");
		}
		string str;
		while (readLogFile.getline(buffer, max_buffer_size)) {
			if((nbytes=write(clifd, buffer, sizeof(buffer)))<0){//send the content
				perror("socket write fail");
			}
			cout << "send " << buffer << endl;;
		}

		// if((nbytes=write(clifd, "-", sizeof("-")))<0){
		// 		perror("socket write finish fail");
		// }
		puts("grep success!");
		close(clifd);
	}
	close(fd);
	return 0;
}



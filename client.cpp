#include<functional>
#include<sys/socket.h> 
#include<netinet/in.h> 
#include<arpa/inet.h> 
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include <cstring>
#include<sys/types.h>
#include<sys/stat.h>    
#include<fcntl.h>
#include<sys/timeb.h>
#include<netdb.h>
#include<unistd.h>
#include <fstream>
#include <iostream>
using namespace std;

constexpr int time_out=1000;
vector<string> machines = {"fa23-cs425-2401.cs.illinois.edu"};
constexpr int server_port = 8821;
constexpr int max_buffer_size = 1e4 + 7;

char cmd[max_buffer_size];// read only in send_grep_request
char buf[max_buffer_size];
void send_grep_request(int machine_idx){
	const auto curMachine = machines[machine_idx];
	ofstream tempLogFile(curMachine + ".log");
	
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		throw("socket build fail");
		exit(1);
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=server_port;
	srv.sin_addr.s_addr=inet_addr(curMachine.c_str());
	if(srv.sin_addr.s_addr==-1){
		struct hostent *host=new struct hostent;
		host=gethostbyname(curMachine.c_str());
		srv.sin_addr=*((struct in_addr *)host->h_addr);
	}
	if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){//connect the socket
		throw("timeout when connect to" + curMachine); 
	}

	int nbytes;
	if((nbytes=write(fd, cmd, sizeof(cmd)))<0){ 
		throw("socket write fail"); 
	}
	if((nbytes=read(fd, buf, sizeof(buf))) < 0){
		throw("socket read fail"); 
	}
	cout << "line nbytes: " << nbytes << endl;
	int line_cnt = stoi(buf);
	printf("grep %d lines\n", line_cnt);
	for(int i=1;i<=line_cnt;i++) {
		nbytes = read(fd, buf, sizeof(buf));
		if(nbytes < 0){
			throw("socket read fail"); 
		}
		else if(nbytes == 0)
			break;
		else{
			for(int i=0;i<=20;i++)cout << buf[i] << ", ";
			cout << endl;
			cout << nbytes << endl;
			tempLogFile << buf;
			cout << "get: " << buf << endl;
		}
	}

	tempLogFile.close();

	/*string myText;
	ifstream MyReadFile(curMachine + ".log");
	while (getline (MyReadFile, myText)) 
		cout << myText << endl;
	MyReadFile.close();*/

	close(fd);	
}

int main(int argc, char *argv[]){
	while(true){
		cin.getline(cmd, max_buffer_size);
		cout << "cmd: " << cmd << endl;
		for(int idx = 0; idx < machines.size(); idx++){
			try{
				send_grep_request(idx);
			} catch (const char* msg) {
				cerr << msg << endl;
			}catch (exception &e) {
				cerr << "error: " << e.what() << endl;
			}
			// const auto curMachine = machines[idx];
			// string myText;
			// ifstream MyReadFile(curMachine + ".log");
			// while (getline (MyReadFile, myText)) 
			// 	cout << myText << endl;
			// MyReadFile.close();
		}
	}

}
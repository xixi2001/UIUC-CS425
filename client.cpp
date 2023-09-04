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
#include <sys/timeb.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <thread>
using namespace std;

constexpr int machine_number = 10;
const vector<string> machines = {
	"fa23-cs425-2401.cs.illinois.edu",
	"fa23-cs425-2402.cs.illinois.edu",
	"fa23-cs425-2403.cs.illinois.edu",
	"fa23-cs425-2404.cs.illinois.edu",
	"fa23-cs425-2405.cs.illinois.edu",
	"fa23-cs425-2406.cs.illinois.edu",
	"fa23-cs425-2407.cs.illinois.edu",
	"fa23-cs425-2408.cs.illinois.edu",
	"fa23-cs425-2409.cs.illinois.edu",
	"fa23-cs425-2410.cs.illinois.edu"
};
constexpr int server_port = 8822;
constexpr int max_buffer_size = 1e4 + 7;
constexpr int time_out_second = 2;

char cmd[max_buffer_size];// read only in send_grep_request, no mutex needed
void send_grep_request(int machine_idx){
	const auto curMachine = machines[machine_idx];
	string error_title =  "machine " + to_string(machine_idx) + " ERROR: ";
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		throw(error_title + " socket build fail");
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
	if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		throw(error_title + " timeout when connecting" ); 
	}
	int nbytes;
	if((nbytes=write(fd, cmd, sizeof(cmd)))<0){ 
		throw(error_title + " socket write fail"); 
	}
	char buf[max_buffer_size];
	if((nbytes=read(fd, buf, sizeof(buf))) < 0){
		throw(error_title + " read line cnt fail"); 
	}
	int line_cnt = stoi(buf);
	ofstream temp_log("grep_receive_result_" + to_string(machine_idx)  + ".log");
	for(int i=1;i<=line_cnt;i++) {
		nbytes = read(fd, buf, sizeof(buf));
		if(nbytes < 0){
			throw(error_title + " read line " + to_string(line_cnt) + " fail"); 
		}
		else if(nbytes == 0)
			break;
		else{
			temp_log << buf << endl;
			// cout << "get: " << buf << endl;
		}
	}
	temp_log.close();
	close(fd);
	printf("INFO: Grep from machine %d finished\n", machine_idx);
}
bool is_alive[machine_number];// each handler only access its corresponding is_alive no mutex needed
void send_grep_request_handler(int machine_idx){
	try{
		send_grep_request(machine_idx);
	}catch ( string msg) {
		cerr << msg << endl;
		is_alive[machine_idx] = false;
	} catch (exception &e) {
		cerr << "ERROR: unexpected error" << e.what() << endl;
		return;
	}
}
int main(int argc, char *argv[]){
	memset(is_alive,1,sizeof(is_alive));
	while(true){
		cin.getline(cmd, max_buffer_size);
		for(int idx = 0; idx < machines.size(); idx++){
			thread request(send_grep_request_handler, idx);
			request.join();
		}
		int total_line_cnt = 0;
		
		for(int idx = 0; idx < machines.size(); idx++){
			if(!is_alive[idx]){
				printf("==================== machine %d is dead ====================\n", idx + 1);// VM idx starts from 1
				continue;
			}
			ifstream read_log("grep_receive_result_" + to_string(idx)  + ".log");
			if(!read_log){
				cerr << " ERROR: cannot open grep_receive_result_" << to_string(idx) << ".log" << endl;
				continue;
			}
			printf("==================== grep result from machine %d ====================\n", idx + 1);// VM idx starts from 1
			static char read_log_buffer[max_buffer_size];
			while (read_log.getline(read_log_buffer, max_buffer_size)) {
				printf("%s\n",read_log_buffer);
				total_line_cnt++;
			}
		}
		cout << "total "  << total_line_cnt << " lines grep." << endl; 
	}

}
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
#include <time.h>
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
constexpr int max_buffer_size = 1024;
constexpr int receive_buffer_size = 5e6;

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
	// send command to the server
	int nbytes;
	if((nbytes=send(fd, cmd, sizeof(cmd), 0))<0){ 
		throw(error_title + " socket write fail"); 
	}
	// // first read the numbers of lines to be received
	// if((nbytes=recv(fd, buf, sizeof(buf),0)) < 0){
	// 	throw(error_title + " read line cnt fail"); 
	// }
	// int line_cnt = stoi(buf);
	ofstream temp_log("grep_receive_result_" + to_string(machine_idx)  + ".log");
	// read the grep result and save to the temporary log
	char buf[receive_buffer_size];
	int packet_cnt = 0;
	while(read(fd, buf, sizeof(buf))) {
		temp_log << buf;
		memset(buf, 0, sizeof(buf));
		packet_cnt++;
	}
	temp_log.close();
	close(fd);
	// printf("INFO: Grep from machine %d finished\n", machine_idx);
}
bool is_alive[machine_number];// each handler only access its corresponding is_alive no mutex needed
void send_grep_request_handler(int machine_idx){
	try{
		send_grep_request(machine_idx);
	} catch (string msg) {
		cerr << msg << endl;
		is_alive[machine_idx] = false;
	} catch (exception &e) {
		cerr << "ERROR: unexpected error" << e.what() << endl;
		return;
	}
}
int main(int argc, char *argv[]){
	memset(is_alive,1,sizeof(is_alive));
	while(cin.getline(cmd, max_buffer_size)){
		auto start = clock();
		//send the request using thread
		for(int idx = 0; idx < machines.size(); idx++){
			thread request(send_grep_request_handler, idx);
			request.join();
		}
		int total_line_cnt = 0;
		ofstream result("grep_result.log");
		// read the result from the logs and merge the result
		for(int idx = 0; idx < machines.size(); idx++){
			if(!is_alive[idx]){
				printf("machine %d is dead\n", idx + 1);// VM idx starts from 1
				continue;
			}
			ifstream read_log("grep_receive_result_" + to_string(idx)  + ".log");
			if(!read_log){
				cerr << " ERROR: cannot open grep_receive_result_" << to_string(idx) << ".log" << endl;
				continue;
			}
			int cur_lines = count(istreambuf_iterator<char>(read_log), istreambuf_iterator<char>(), '\n');
			printf("grep %d lines from machine %d\n", cur_lines, idx + 1);// VM idx starts from 1
			total_line_cnt += cur_lines;
			if(cur_lines) {
				read_log.seekg(0, ios::beg);
				result << read_log.rdbuf();
				read_log.close();				
			}
		}
		cout << "total "  << total_line_cnt << " lines grep." << endl;
		auto end = clock();
		cout<< "time usage: "<<double(end-start)/CLOCKS_PER_SEC<<"s"<<endl;
	}

}

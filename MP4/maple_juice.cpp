#include "maple_juice.h"
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <fcntl.h>
#include <unistd.h>
using namespace std;

constexpr int mj_receiver_port = 1088;
constexpr int max_buffer_size = 1024;

void send_mj_message(){
    
}

void mj_message_receiver(){
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
        puts("File receiver build fail!");
        throw runtime_error("File receiver build fail!");
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=mj_receiver_port;
	srv.sin_addr.s_addr=htonl(INADDR_ANY);
	if((bind(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		puts("TCP message receiver bind fail!");
        throw runtime_error("TCP message receiver bind fail!");
	}
	if(listen(fd,10)<0) {
		puts("TCP message receiver listen fail!");
        throw runtime_error("TCP message receiver listen fail!");
	}
	struct sockaddr_in cli;
	int clifd;
	socklen_t cli_len = sizeof(cli);
	
    while(1){

		clifd=accept(fd, (struct sockaddr*) &cli, &cli_len);//block until accept connection
		if(clifd<0){
			puts("TCP message receiver accept fail!");
            throw runtime_error("TCP message receiver accept fail!");
		}
		int nbytes;
        char msg[max_buffer_size];
        memset(msg, 0, sizeof(msg));
		if((nbytes=recv(clifd, msg, sizeof(msg),0))<0){//"read" will block until receive data 
			puts("TCP message receiver read fail!");
            throw runtime_error("TCP message receiver read fail!");
		}

        string msg_str(msg);
        vector<string> info;
        info = tokenize(msg_str.substr(1), ' ');
        string filename = info[0];
        string ret;

        print_to_sdfs_log("Received a message: " + msg_str, false);
        
        switch(msg_str[0]){
            case 'M':
                // TODO: queue commands
                break;
            case 'm':
                // TODO: initiate work_maple_task
                break;
            default:
                puts("Message is in wrong format.");
                throw runtime_error("Message is in wrong format.");
        }
        close(clifd);
	}
	close(fd);
}

void command_queue_listener();

void work_maple_task(const string& cmd);

void maple_task_monitor(const string& cmd, const vector<string>& files);


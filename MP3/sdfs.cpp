#include "sdfs.h"
#include <unordered_map>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
using namespace std;

int machine_idx;
vector<int> last_alive_membership_list;

set<string> master_files; // files save as master; string is the file name
set<pair<string,int> > slave_files;  // files save as slave;string is file name; int is hash value
vector<int> slave_id;

constexpr int max_buffer_size = 1024;
constexpr int listen_port = 1022;

void tcp_message_receiver(){
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
		perror("FATAL: socket build fail");
		exit(1);
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=htons(listen_port);
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
		if((nbytes=recv(clifd, msg, sizeof(msg),0))<0){//"read" will block until receive data 
			puts("TCP message receiver read fail!");
            throw runtime_error("TCP message receiver read fail!");
		}

        string msg_str(msg);
        string filename = msg_str.substr(1);
        switch(msg_str[0]){
            case 'P': // assume message format is P{sdfs_filename}
                master_files.insert(filename);
                for(int i = 0; i < slave_id.size(); i++){
                    send_a_tcp_message(filename, slave_id[i]);
                }
                break;
            case 'p':
                // TODO: update slave files
                break;
            case 'D':
                // TODO:
                // delete corresponding master file 
                // send delete request to its slaves
                break;
            case 'd':
                // TODO: delete corresponding slave file
                break;
            case 'G':
                // TODO: send back corresponding master
                break;
            default:
                puts("Message is in wrong format.");
                throw runtime_error("Message is in wrong format.");
        }
	}
	close(fd);
}

void send_a_tcp_message(const string& str, int target_index){
    string target_ip = get_ip_address_from_index(target_index);
	
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		puts("TCP message sender build fail!");
        throw runtime_error("TCP message sender build fail!");
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=htons(listen_port);

    if (inet_pton(AF_INET, "target_ip", &srv.sin_addr) <= 0) {
        puts("Invalid address/ Address not supported!");
        throw runtime_error("Invalid address/ Address not supported!");
    }

	if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
        puts("Timeout when connecting!");
        throw runtime_error("Timeout when connecting!");
	}
	// send command to the server
	int nbytes;

	if((nbytes=send(fd, str.c_str(), str.size(), 0))<0){ 
        puts("Socket write fail!");
        throw runtime_error("Socket write fail!");
	}
	
	close(fd);
}

void membership_list_listener(){

}

int find_next_live_id(const set<int> &s,int x){
    do{
        x = (x + 1) % 10;
    } while(!s.count(x));
    return x;
}

vector<int> get_new_slave_id(){
    set<int> membership_set = get_current_live_membership_set();
    vector<int> res;
    int cur = find_next_live_id(s, machine_idx);
    while(res.size() <= 3 && cur != machine_idx){
        res.push_back(cur);
        cur = find_next_live_id(s, cur);
    }
    sort(res.begin(),res.end());
    return res;
}

int find_last_live_id(const set<int> &s,int x){
    do{
        x = (x + 9) % 10;
    } while(!s.count(x));
    return x;
}

vector<int> get_new_master_id(){
    set<int> membership_set = get_current_live_membership_set();
    vector<int> res;
    int cur = find_last_live_id(s, machine_idx);
    while(res.size() <= 3 && cur != machine_idx){
        res.push_back(cur);
        cur = find_last_live_id(s, cur);
    }
    sort(res.begin(),res.end());
    return res;
}

void handle_crash(int crash_idx){

}

void handle_join(int join_idx){

}

void print_to_sdfs_log(const string& str, bool flag){

}

int main(){
    // start_membership_service("172.22.94.78");
    // cout << get_ip_address_from_index(0) << endl;
}
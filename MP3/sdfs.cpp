#include "sdfs.h"
#include <thread>
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

mutex master_files_lock;
set<string> master_files; // files save as master; string is the file name
mutex slave_files_lock;
set<pair<string,int> > slave_files;  // files save as slave;string is file name; int is hash value
mutex slave_idx_set_lock;
set<int> slave_idx_set;

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
        string ret;
        
        switch(msg_str[0]){
            case 'P': // assume message format is P{sdfs_filename}
                master_files_lock.lock();
                master_files.insert(filename);
                master_files_lock.unlock();
                
                slave_idx_set_lock.lock();
                for(int slave : slave_idx_set)
                    thread(send_a_tcp_message, "p" + filename, slave).detach();
                slave_idx_set_lock.unlock();
                
                break;
            case 'p':
                slave_files_lock.lock();
                slave_files.insert({filename, hash_string(filename)});
                slave_files_lock.unlock();
                break;
            case 'D':
                master_files_lock.lock();
                master_files.erase(filename);
                master_files_lock.unlock();
                
                slave_idx_set_lock.lock();
                for(int slave : slave_idx_set)
                    thread(send_a_tcp_message, "d" + filename, slave).detach();
                slave_idx_set_lock.unlock();
                break;
            case 'd':
                slave_files_lock.lock();
                slave_files.erase({filename, hash_string(filename)});
                slave_files_lock.unlock();
                break;
            case 'G':
                if(master_files.find(filename) == master_files.end())
                    ret = "File doesn't exist!";
                else
                    ret = "File exists!";

                if((nbytes=send(clifd, ret.c_str(), ret.size() ,0))<0){
                    puts("Socket write fail!");
                    throw runtime_error("Socket write fail!");
                }
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
    set<int> last_alive_membership_set;
    while(true){
        set<int> new_memebership_set = get_current_live_membership_set();
        vector<int> join_list, leave_list;
        for(int x:last_alive_membership_set)
            if(!new_memebership_set.count(x))
                leave_list.push_back(x);
        for(int x:new_memebership_set)
            if(!last_alive_membership_set.count(x))
                join_list.push_back(x);
        if( leave_list.size() > 0 || join_list.size() > 0 ) {
            last_alive_membership_list = 
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    char ret[max_buffer_size];
    if(str[0]== 'G'){
        if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){//"read" will block until receive data 
        puts("TCP message receiver read fail!");
                    throw runtime_error("TCP message receiver read fail!");
        }
        cout << ret << endl;
    }

}

int find_next_live_id(const set<int> &s,int x){
    do{
        x = (x + 1) % 10;
    } while(!s.count(x));
    return x;
}

set<int> get_new_slave_id(const set<int> &membership_set){
    set<int> res;
    int cur = find_next_live_id(membership_set, machine_idx);
    while(res.size() <= 3 && cur != machine_idx){
        res.insert(cur);
        cur = find_next_live_id(s, cur);
    }
    return res;
}

int find_last_live_id(const set<int> &s,int x){
    do{
        x = (x + 9) % 10;
    } while(!s.count(x));
    return x;
}

set<int> get_new_master_id(const set<int> &membership_set){
    set<int> res;
    int cur = find_last_live_id(membership_set, machine_idx);
    while(res.size() <= 3 && cur != machine_idx){
        res.insert(cur);
        cur = find_last_live_id(membership_set, cur);
    }
    return res;
}

int hash_string(const string &str){
    unsigned int sum = 0;
    for(auto s:str)sum = sum*19260817 + s;
    return sum % 10;
}

int find_master(const set<int> &membership_set, int hash_value){
    return find_next_live_id(membership_set, hash_value);
}

void handle_crash(int crash_idx, const set<int> &new_membership_set, const set<int>& delta_slaves){

}

void handle_join(int join_idx, const set<int> &new_membership_set, const set<int> &new_slaves, const set<int> &new_masters){

}

void print_to_sdfs_log(const string& str, bool flag){

}

int main(){
    // start_membership_service("172.22.94.78");
    // cout << get_ip_address_from_index(0) << endl;
}
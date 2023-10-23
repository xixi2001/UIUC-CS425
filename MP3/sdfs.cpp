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
set<string> slave_files;  // files save as slave;string is file name; int is hash value
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
	srv.sin_port=listen_port;
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
        string filename = msg_str.substr(1);
        string ret;
        print_to_sdfs_log("Received a message: " + msg_str, true);
        
        switch(msg_str[0]){
            case 'P': // assume message format is P{sdfs_filename}
                master_files_lock.lock();
                master_files.insert(filename);
                master_files_lock.unlock();
                
                slave_idx_set_lock.lock();
                for(int slave : slave_idx_set)
                    thread(send_a_tcp_message, "p" + filename, slave).detach();
                slave_idx_set_lock.unlock();
                
                print_to_sdfs_log("Get master file: " + filename, true);
                print_current_files();
                break;
            case 'p':
                slave_files_lock.lock();
                slave_files.insert(filename);
                slave_files_lock.unlock();
                print_to_sdfs_log("Get slave file: " + filename, true);
                print_current_files();
                break;
            case 'D':
                master_files_lock.lock();
                master_files.erase(filename);
                master_files_lock.unlock();
                
                slave_idx_set_lock.lock();
                for(int slave : slave_idx_set)
                    thread(send_a_tcp_message, "d" + filename, slave).detach();
                slave_idx_set_lock.unlock();

                print_to_sdfs_log("Delete master file: " + filename, true);
                print_current_files();
                break;
            case 'd':
                slave_files_lock.lock();
                slave_files.erase(filename);
                slave_files_lock.unlock();

                print_to_sdfs_log("Delete slave file: " + filename, true);
                print_current_files();
                break;
            case 'G':
                if(master_files.find(filename) == master_files.end())
                    ret = "File doesn't exist!";
                else
                    ret = "File exists!";

                print_to_sdfs_log("Requst " + filename + ": " + ret, true);

                if((nbytes=send(clifd, ret.c_str(), ret.size() ,0))<0){
                    puts("Socket write fail!");
                    throw runtime_error("Socket write fail!");
                }
                break;
            default:
                puts("Message is in wrong format.");
                throw runtime_error("Message is in wrong format.");
        }
        close(clifd);
	}
	close(fd);
}

string to_string(const set<int> s){
    stringstream ss;
    for(int x:s)
        ss << x << " ";
    return ss.str();
}

string to_string(const set<string> s){
    stringstream ss;
    for(string x:s)
        ss << x << " ";
    return ss.str();
}

void send_a_tcp_message(const string& str, int target_index){
    string target_ip = get_ip_address_from_index(target_index);
	print_to_sdfs_log("Send a message to " + to_string(target_index+1) + ": " + str, true);
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		puts("TCP message sender build fail!");
        throw runtime_error("TCP message sender build fail!");
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=listen_port;

    if (inet_pton(AF_INET, target_ip.c_str(), &srv.sin_addr) <= 0) {
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
    char ret[max_buffer_size];
    memset(ret, 0, sizeof(ret));
    if(str[0]== 'G'){
        if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){//"read" will block until receive data 
        puts("TCP message receiver read fail!");
                    throw runtime_error("TCP message receiver read fail!");
        }
        print_to_sdfs_log(ret, true);
    }
	
	close(fd);
    
}

void membership_listener(){
    set<int> last_membership_set;
    while(true){
        set<int> new_membership_set = get_current_live_membership_set();
        vector<int> join_list, crash_list;
        for(int x:last_membership_set)
            if(!new_membership_set.count(x))
                crash_list.push_back(x);
        for(int x:new_membership_set)
            if(!last_membership_set.count(x))
                join_list.push_back(x);
        if( crash_list.size() > 0 || join_list.size() > 0 ) {
            set<int> new_slave_idx = get_new_slave_idx_set(new_membership_set);
            for(int x : crash_list) {
                print_to_sdfs_log("handle " + to_string(x + 1) + " Crasehed", true);
                set<int> delta_slaves;
                slave_idx_set_lock.lock();
                for(int x:new_slave_idx)
                    if(!slave_idx_set.count(x))
                        delta_slaves.insert(x);
                slave_idx_set_lock.unlock();
                handle_crash(x, new_membership_set, delta_slaves);
            }
            for(int x : join_list){
                print_to_sdfs_log("handle " + to_string(x + 1) + " Joined", true);
                handle_join(x, new_membership_set, new_slave_idx, get_new_master_idx_set(new_membership_set));
            }

            last_membership_set = new_membership_set;
            slave_idx_set_lock.lock();
            slave_idx_set = new_slave_idx;
            slave_idx_set_lock.unlock();
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

int find_next_live_id(const set<int> &s,int x){
    do{
        x = (x + 1) % 10;
    } while(!s.count(x));
    return x;
}

set<int> get_new_slave_idx_set(const set<int> &membership_set){
    set<int> res;
    int cur = find_next_live_id(membership_set, machine_idx);
    while(res.size() < 3 && cur != machine_idx){
        res.insert(cur);
        cur = find_next_live_id(membership_set, cur);
    }
    return res;
}

int find_last_live_id(const set<int> &s,int x){
    do{
        x = (x + 9) % 10;
    } while(!s.count(x));
    return x;
}

set<int> get_new_master_idx_set(const set<int> &membership_set){
    set<int> res;
    int cur = find_last_live_id(membership_set, machine_idx);
    while(res.size() < 3 && cur != machine_idx){
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
    int x = hash_value;
    while(!membership_set.count(x)){
        x = (x + 1) % 10;
    }
    return x;
}

void handle_crash(int crash_idx, const set<int> &new_membership_set, const set<int>& delta_slaves){
    print_to_sdfs_log("handle_crash_new_membership_set: " + to_string(new_membership_set), false);
    print_to_sdfs_log("handle_crash_delta_slaves: " + to_string(delta_slaves), false);
    if(machine_idx == find_next_live_id(new_membership_set, crash_idx)){
            vector<string> slave_to_master_file;
            
            slave_files_lock.lock();
            set<string> slave_files_temp = slave_files;
            slave_files_lock.unlock();
            
            for(string slave_file : slave_files_temp){
                if(find_master(new_membership_set, hash_string(slave_file)) == machine_idx){
                    slave_to_master_file.push_back(slave_file);
                    
                    master_files_lock.lock();
                    master_files.insert(slave_file);
                    master_files_lock.unlock();
                    
                    slave_idx_set_lock.lock();
                    print_to_sdfs_log("Start sending new master files to all the slaves", true);
                    for(int slave_id : slave_idx_set){
                        print_to_sdfs_log("Start sending new master files to one slave " + to_string(slave_id), true);
                        thread(send_a_tcp_message, "p" + slave_file, slave_id).detach();
                    }
                    slave_idx_set_lock.unlock();
                }
            }
            
            
            slave_files_lock.lock();
            for(string filename : slave_to_master_file)
                slave_files.erase(filename); 
            slave_files_lock.unlock();
    }

    for(int slave: delta_slaves){
        master_files_lock.lock();
        for(string master_file : master_files){
            thread(send_a_tcp_message, "p" + master_file, slave).detach();
        }
        master_files_lock.unlock();
    }
}

void handle_join(int join_idx, const set<int> &new_membership_set, const set<int> &new_slaves, const set<int> &new_masters){
    print_to_sdfs_log("handle_crash_new_membership_set: " + to_string(new_membership_set), false);
    print_to_sdfs_log("handle_join_new_slaves: " + to_string(new_slaves), false);
    print_to_sdfs_log("handle_join_new_masters: " + to_string(new_masters), false);
    if(machine_idx == find_next_live_id(new_membership_set, join_idx)){
        vector<string> master_file_to_delete;
        master_files_lock.lock();
        for(string master_file : master_files){
            if(find_master(new_membership_set, hash_string(master_file)) == join_idx){
                thread(send_a_tcp_message, "P" + master_file, join_idx).detach();
                master_file_to_delete.push_back(master_file);
            }
        }
        
        for(string filename : master_file_to_delete)
            master_files.erase(filename); 
        master_files_lock.unlock();
    }
    
    master_files_lock.lock();
    if(new_slaves.find(join_idx) != new_slaves.end()){
        for(string master_file : master_files)
            thread(send_a_tcp_message, "p" + master_file, join_idx).detach();
    }
    master_files_lock.unlock();

    vector<string> slave_file_to_delete;
    
    slave_files_lock.lock();
    for(string slave_file : slave_files){
        if(new_masters.find(find_master(new_membership_set, hash_string(slave_file))) 
                == new_masters.end())
            slave_file_to_delete.push_back(slave_file);
    }
    for(string filename : slave_file_to_delete)
        slave_files.erase(filename);
    slave_files_lock.unlock();
}

int64_t start_time_ms;
mutex fsdfs_out_lock;
ofstream fsdfs_out;
void print_to_sdfs_log(const string& str, bool flag){
    int64_t current_time_ms = cur_time_in_ms(); 
    if(flag)
        cout << "[" << current_time_ms - start_time_ms << "] " << str << endl;
    fsdfs_out_lock.lock();
    fsdfs_out << "[" << current_time_ms - start_time_ms << "] " << str << endl;
    fsdfs_out_lock.unlock();
}

void print_current_files(){
    master_files_lock.lock();
    stringstream ss;
    ss << "Master files: ";
    for(string file: master_files)
        ss << file << " ";
    print_to_sdfs_log(ss.str(), true);
    master_files_lock.unlock();

    slave_files_lock.lock();
    stringstream sss;
    sss << "Slave files: ";
    for(string file:slave_files)
        sss << file << " ";
    print_to_sdfs_log(sss.str(), true);
    slave_files_lock.unlock();
}

int main(int argc, char *argv[]){
    init_ip_list();
    if(argc != 2){
        puts("FATAL: please assign machine index!");
        exit(0);
    }
    machine_idx = stoi(argv[1]) - 1;
    start_time_ms = cur_time_in_ms();
    fsdfs_out.open( std::to_string(machine_idx + 1) + "_" + std::to_string(start_time_ms) +".log");
    start_membership_service(get_ip_address_from_index(machine_idx));
    thread(tcp_message_receiver).detach();
    thread(membership_listener).detach();
    
    string input;
    while(cin>>input){
        set<int> membership_set = get_current_live_membership_set();
        if(input == "Get" || input == "get" || input == "G" || input == "g") {
            string name;cin>>name;
            send_a_tcp_message("G"+name, find_master(membership_set, hash_string(name)));
        } else if(input == "Put" || input == "put" || input == "P" || input == "p") {
            string name;cin>>name;
            send_a_tcp_message("P"+name, find_master(membership_set, hash_string(name)));
        } else if(input == "Delete" || input == "delete" || input == "D" || input == "d") {
            string name;cin>>name;
            send_a_tcp_message("D"+name, find_master(membership_set, hash_string(name)));
        } else if(input == "Store" || input == "store" || input == "S" || input == "s") {
            //TODO
        } else if(input == "ls") {
            print_current_files();
        } else{
            puts("Unsupported Command!");
        }
    }
}
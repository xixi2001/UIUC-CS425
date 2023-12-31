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
#include <cstdio>
#include <filesystem>
using namespace std;
// namespace fs = std::filesystem;

int machine_idx;

mutex master_files_lock;
set<string> master_files; // files save as master; string is the file name
mutex slave_files_lock;
set<string> slave_files;  // files save as slave;string is file name; int is hash value
mutex slave_idx_set_lock;
set<int> slave_idx_set;

map<string, unsigned long long> cur_event_num, finish_event_num, last_write_num;
map<string, set<unsigned long long> >unfinished_events;
mutex event_num_lock;

constexpr int max_buffer_size = 1024;
constexpr int listen_port = 1022;
constexpr int file_receiver_port = 1020;
constexpr int receive_buffer_size = 1024;

vector<string> tokenize(string input, char delimeter){
    istringstream ss(input);
    string token;
    vector<string> ret;
    while(getline(ss, token, delimeter)){
        ret.push_back(token);
    }

    return ret;
}

// check whether s == pre_##
bool is_match_prefix(const string &s, const string &prefix){
    if(s.length() != prefix.length() + 3){
        return 0;
    }
    int cur = 0;
    for(;cur < prefix.size(); cur++) {
        if(s[cur] != prefix[cur]) {
            return 0;
        }
    }
    return s[cur] == '-';
}

bool is_prefix(const string &s, const string &prefix){
    if(s.length() < prefix.length() )return 0;
    for(int i=0; i<prefix.length(); i++){
        if(s[i] != prefix[i]){
            return 0;
        }
    }
    return 1;
}

unsigned long long read_lock(const string& file) {
    event_num_lock.lock();
    auto event_num = ++cur_event_num[file];
    unfinished_events[file].insert(event_num);
    auto last_write = last_write_num[file];
    event_num_lock.unlock();
    // cout << "send file: " << " event: " << event_num << " last write: " << last_write << endl;
    long long count = 0;
    while(true){// block before previous write is finished
        event_num_lock.lock();
        auto finish_num = finish_event_num[file];
        event_num_lock.unlock();
        count++;
        if(count % 100000000 == 1){
            stringstream ss;
            ss << "send file: " << file << " ,event: " << event_num << " last write: " << last_write << " cur finished: " << finish_num << endl;
            print_to_sdfs_log(ss.str(), false);
        }
        if(finish_num >= last_write) break; 
    }
    return event_num;
}

unsigned long long write_lock(const string& file) {
    event_num_lock.lock();
    auto event_num = ++cur_event_num[file];
    unfinished_events[file].insert(event_num);
    last_write_num[file] = event_num;
    event_num_lock.unlock();
    // cout << "receive file: " << " event: " << event_num << endl;
    long long count = 0;
    while(true){// block before previous event is finished
        event_num_lock.lock();
        auto finish_num = finish_event_num[file];
        event_num_lock.unlock();
        count++;
        if(count % 100000000 == 1){
            stringstream ss;
            ss << "receive file: " << " event: " << event_num << " cur finished: " << finish_num << endl;
            print_to_sdfs_log(ss.str(), false);
        }
        
        if(finish_num + 1 == event_num) break; 
    }
    return event_num;
}

void update_finish_event(unsigned long long event_num, const string& file){
    event_num_lock.lock();
    unfinished_events[file].erase(event_num);
    if(unfinished_events[file].empty()){
        finish_event_num[file] = cur_event_num[file]; 
    } else {
        finish_event_num[file] = *(unfinished_events[file].begin()) - 1;
    }
    stringstream ss;
    ss << "unfinished events: ";
    for(auto x:unfinished_events[file])
        ss << x << " ";
    ss << endl;
    print_to_sdfs_log("Update finish event num to: " + to_string(finish_event_num[file]) 
        + " finsied event num: " + to_string(event_num), false);
    print_to_sdfs_log(ss.str(), false);
    event_num_lock.unlock();
}

void post_receive_a_file(const char cmd,const string& filename){
    if(cmd == 'P'){
        master_files_lock.lock();
        master_files.insert(filename);
        master_files_lock.unlock();
        
        slave_idx_set_lock.lock();
        for(int slave_idx : slave_idx_set)
            thread(send_file, filename, filename, slave_idx, "p", true).detach();
        slave_idx_set_lock.unlock();
        print_to_sdfs_log("Get master file: " + filename, true);
    } else if(cmd == 'p'){
        slave_files_lock.lock();
        slave_files.insert(filename);
        slave_files_lock.unlock();
        print_to_sdfs_log("Get slave file: " + filename, true);
    } else if(cmd == 'G') {
        print_to_sdfs_log("Get local file: " + filename, true);
    }

}

void local_file_copy(const string src, const string dst, const char cmd) {
    print_to_sdfs_log("local copy from " +src + " to " + dst , true);
    ifstream readSrcFile( src );
    if(!readSrcFile){
        cout << "[ERROR] Cannot open source file:" << src;
        return;
	}
    auto event_num = write_lock(src);
    ofstream dst_file((cmd == 'G' ? "":"sdfs_files/") + dst);
    
    while(readSrcFile.good()){
        string str;
        while(str.length() < max_buffer_size/2 && readSrcFile.good()){
            if(readSrcFile.peek() != -1)
               str.push_back(readSrcFile.get());
        }
        const char *c_str = str.c_str();

        dst_file << c_str;
    }
    dst_file.close();
    readSrcFile.close();

    post_receive_a_file(cmd, dst);

    update_finish_event(event_num, src);
}

string to_string(const vector<string> &files) {
    stringstream ss;
    ss << "[";
    for(const string& str:files)
        ss << str << ",";
    ss << "]";
    return ss.str();
}

void merge_files_into_file(const vector<string> &files, const string &prefix) {
    auto event_num = write_lock(prefix);
    print_to_sdfs_log("local copy from sdfs_files/" + to_string(files) + " to sdfs_files/" + prefix , true);
    ofstream dst_file("sdfs_files/" + prefix);
    for(const string &file:files){
        ifstream readSrcFile( "sdfs_files/" + file );
        if(!readSrcFile){
            cout << "[ERROR] Cannot open source file:" << file;
            continue;
        }
        while(readSrcFile.good()){
            string str;
            while(str.length() < max_buffer_size/2 && readSrcFile.good()){
                if(readSrcFile.peek() != -1)
                str.push_back(readSrcFile.get());
            }
            const char *c_str = str.c_str();
            dst_file << c_str;
        }
        readSrcFile.close();
        remove(("./sdfs_files/" + file).c_str());
    }
    dst_file.close();
    update_finish_event(event_num, prefix);
}

void merge_all_files_with_common_prefix(const string& prefix) {
    master_files_lock.lock();
    map<string, vector<string> > file_bucket;
    for(const string& str:master_files) {
        if(is_prefix(str, prefix)) {
            const string prefix_key = str.substr(0, str.size() - 3);
            if(!file_bucket.count(prefix_key)) {
                file_bucket.emplace(prefix_key, vector<string>({}));
            }
            file_bucket[prefix_key].push_back(str);
        }
    }
    for(const auto& [prefix_key, files] : file_bucket) {
        merge_files_into_file(files, prefix_key);
        for(const string& file : files)master_files.erase(file);
        master_files.insert(prefix_key);
    }
    master_files_lock.unlock();

    slave_files_lock.lock();
    file_bucket.clear();
    for(const string& str:slave_files) {
        if(is_prefix(str, prefix)) {
            const string prefix_key = str.substr(0, str.size() - 3);
            if(!file_bucket.count(prefix_key)) {
                file_bucket.emplace(prefix_key, vector<string>({}));
            }
            file_bucket[prefix_key].push_back(str);
        }
    }
    for(const auto& [prefix_key, files] : file_bucket) {
        merge_files_into_file(files, prefix_key);
        for(const string& file : files)slave_files.erase(file);
        slave_files.insert(prefix_key);
    }
    slave_files_lock.unlock();
}

void send_file(string src, string dst, int target_idx, string cmd, bool is_in_sdfs_folder){
    if(target_idx == machine_idx) {
        local_file_copy((is_in_sdfs_folder ? "sdfs_files/":"")+src, dst, cmd[0]);
        return;
    }
    src = (is_in_sdfs_folder ? "sdfs_files/":"")+ src;
    ifstream readSrcFile( src );
    if(!readSrcFile){
        cout << "[ERROR] Cannot open source file:" << src << endl;
        return;
	}
    print_to_sdfs_log("Send a file to " + to_string(target_idx + 1) + " file name: " + src, true);


    string target_ip = get_ip_address_from_index(target_idx);
	
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		puts("[ERROR] File sender build fail!");
        return;
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=file_receiver_port;

    if (inet_pton(AF_INET, target_ip.c_str(), &srv.sin_addr) <= 0) {
        puts("[ERROR] Invalid address/ Address not supported!");
        return;
    }

	if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
        puts("[ERROR] Timeout when connecting!");
        return;
	}

    string info = cmd + dst;
    int nbytes;
	if((nbytes=send(fd, info.c_str(), info.size(), 0))<0){ 
        puts("[ERROR] Socket write fail!");
        return;
	}
    // print_to_sdfs_log(info, true);

    char ret[max_buffer_size];
    memset(ret, 0, sizeof(ret));
    if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){
        puts("[ERROR] File sender read fail!");
        return;
    }

    auto event_num = read_lock(src);
    
    while(readSrcFile.good()){
        string str;
        while(str.length() < max_buffer_size/2 && readSrcFile.good()){
            if(readSrcFile.peek() != -1)
               str.push_back(readSrcFile.get());
        }
        const char *c_str = str.c_str();

        if((nbytes=send(fd, c_str, strlen(c_str),0))<0){
            puts("FATAL: socket write fail");
            break;
        }
    }

	close(fd);

    if(cmd == "P") {
        print_to_sdfs_log("send " + src + " success", true);
    }

    update_finish_event(event_num, src);
}

void receive_a_file(int clifd){
    
    if(clifd<0){
		puts("[ERROR] File receiver accept fail!");
        return;
	}

    // Receive intial message including command and filename 
    int nbytes;
    char msg[max_buffer_size];
    memset(msg, 0, sizeof(msg));
    if((nbytes=recv(clifd, msg, sizeof(msg),0))<0){
        puts("[ERROR] File receiver read fail!");
        return;
    }

    string msg_str = (string)msg;
    char cmd = msg_str[0];
    string filename = msg_str.substr(1);

    string res = ""; res.append(1, cmd); res += " Confirm";
    if((nbytes=send(clifd, res.c_str(), res.size(), 0))<0){ 
        puts("[ERROR] Socket write fail!");
        return;
    }
    // print_to_sdfs_log(res, true);

    const string& dst = (cmd == 'G' ? "":"sdfs_files/")+filename;
    auto event_num = write_lock(dst);

    // Start transfering file
    ofstream dst_file(dst);
    
    char buf[receive_buffer_size];
    memset(buf, 0, sizeof(buf));

    while((nbytes = read(clifd, buf, sizeof(buf)))> 0) {
        dst_file.write(buf, nbytes);
        memset(buf, 0, sizeof(buf));
    }
    dst_file.close();

    close(clifd);

    post_receive_a_file(cmd, filename);

    update_finish_event(event_num, dst);
    // string to_print = string(1,cmd) + " command end: " + to_string(cur_time_in_ms());
    // print_to_sdfs_log(to_print, true);
}

void file_receiver(){
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
        puts("File receiver build fail!");
        throw runtime_error("File receiver build fail!");
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=file_receiver_port;
	srv.sin_addr.s_addr=htonl(INADDR_ANY);
	if((bind(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		puts("File receiver bind fail!");
        throw runtime_error("File receiver bind fail!");
	}
	if(listen(fd,10)<0) {
		puts("TCP message receiver listen fail!");
        throw runtime_error("File receiver listen fail!");
	}
	struct sockaddr_in cli;
	int clifd;
	socklen_t cli_len = sizeof(cli);
	
    while(1){
		clifd=accept(fd, (struct sockaddr*) &cli, &cli_len);//block until accept connection
		thread(receive_a_file, clifd).detach();
	}
	close(fd);
}

void sdfs_message_receiver(){
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
        puts("File receiver build fail!");
        throw runtime_error("File receiver build fail!");
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
        vector<string> info, files;
        info = tokenize(msg_str.substr(1), ' ');
        string filename = info[0];
        string ret;

        print_to_sdfs_log("Received a message: " + msg_str, false);
        
        switch(msg_str[0]){
            case 'D':
                master_files_lock.lock();
                if (master_files.find(filename) != master_files.end()){
                    if(remove(("./sdfs_files/" + filename).c_str()) != 0){
                        puts("Master file delete fail!");
                        throw runtime_error("Master file delete fail!");
                    }
                }
                else{
                    cout << filename << " not exist" << endl;
                }
                master_files.erase(filename);
                master_files_lock.unlock();
                
                slave_idx_set_lock.lock();
                for(int slave : slave_idx_set)
                    thread(send_a_sdfs_message, "d" + filename, slave).detach();
                slave_idx_set_lock.unlock();

                print_to_sdfs_log("Delete master file: " + filename, true);
                print_current_files();
                break;
            case 'd':
                slave_files_lock.lock();
                if (slave_files.find(filename) != slave_files.end()){
                    if(remove(("./sdfs_files/" + filename).c_str()) != 0){
                        puts("Slave file delete fail!");
                        throw runtime_error("Slave file delete fail!");
                    }
                }
                else{
                    cout << filename << " not exist" << endl;
                }
                slave_files.erase(filename);
                slave_files_lock.unlock();

                print_to_sdfs_log("Delete slave file: " + filename, true);
                print_current_files();
                break;
            case 'G':
            case 'L':
                if(master_files.find(info[0]) == master_files.end())
                    ret = "File doesn't exist!";
                else{
                    ret = "File exists!";
                    if(msg_str[0] == 'G')
                        thread(send_file, info[0], info[1], stoi(info[2]), "G", true).detach();
                }

                print_to_sdfs_log("Requst " + filename + ": " + ret, true);

                if((nbytes=send(clifd, ret.c_str(), ret.size() ,0))<0){
                    puts("Socket write fail!");
                    continue;
                }
                break;
            case 'C':
                merge_all_files_with_common_prefix(filename);
                break;
            case 'F':
                master_files_lock.lock();
                for(const string &file:master_files) {
                    if(is_prefix(file, filename))
                        ret += file + " ";
                }
                master_files_lock.unlock();

                print_to_sdfs_log("Files under folder " + filename + ": " + ret, true);

                if((nbytes=send(clifd, ret.c_str(), ret.size() ,0))<0){
                    puts("Socket write fail!");
                    continue;
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

mutex files_under_folder_lock;
vector<string> files_under_folder;
int file_folder_request_cnt;

void send_a_sdfs_message(const string& str, int target_index){
    string target_ip = get_ip_address_from_index(target_index);
	print_to_sdfs_log("Send a message to " + to_string(target_index+1) + ": " + str, true);
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		puts("[Error] TCP message sender build fail!");
        return;
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=listen_port;

    if (inet_pton(AF_INET, target_ip.c_str(), &srv.sin_addr) <= 0) {
        puts("[Error] Invalid address/ Address not supported!");
        return;
    }

	if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
        puts("[Error] Timeout when connecting!");
        return;
	}
	// send command to the server
	int nbytes;

	if((nbytes=send(fd, str.c_str(), str.size(), 0))<0){ 
        puts("[Error] Socket write fail!");
        return;
	}
    char ret[max_buffer_size];
    memset(ret, 0, sizeof(ret));
    if(str[0]== 'G'){
        if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){//"read" will block until receive data 
            puts("[Error] TCP message receiver read fail!");
            return;
        }
        print_to_sdfs_log(ret, true);
    } else if(str[0] == 'L'){
        if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){//"read" will block until receive data 
            puts("[Error] TCP message receiver read fail!");
            return;
        }
        print_to_sdfs_log(ret, true);
        if(ret[5] == 'e') {
            auto membership_set = get_current_live_membership_set();
            stringstream ss;
            ss << "master: " << target_index + 1 << " ";
            ss << "slave: ";
            int slave_id = find_next_live_id(membership_set, target_index);
            for(int i=1;i<=3;i++){
                ss << slave_id + 1 << " ";
                slave_id = find_next_live_id(membership_set, slave_id);
            }
            print_to_sdfs_log(ss.str(), true);
        }
    } else if(str[0] == 'F') {
        if((nbytes=recv(fd, ret, sizeof(ret),0)) < 0){//"read" will block until receive data 
            puts("[Error] TCP message receiver read fail!");
            return;
        }
        print_to_sdfs_log("get folder result from " + to_string(target_index) + ": " + ret, true);
        files_under_folder_lock.lock();
        vector<string> files = tokenize(ret, ' ');
        for(const string &file : files) {
            files_under_folder.push_back(file);
        }
        file_folder_request_cnt--;
        files_under_folder_lock.unlock();
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
            if(crash_list.size() > 0){
                string to_print =  "Handle crash start: "+ to_string(cur_time_in_ms());
                print_to_sdfs_log(to_print, true);
            }  
            set<int> new_slave_idx = get_new_slave_idx_set(new_membership_set);
            for(int x : crash_list) {
                print_to_sdfs_log("handle " + to_string(x + 1) + " Crasehed", true);
                set<int> delta_slaves;
                slave_idx_set_lock.lock();
                for(int x:new_slave_idx)
                    if(!slave_idx_set.count(x))
                        delta_slaves.insert(x);
                slave_idx_set = new_slave_idx;
                slave_idx_set_lock.unlock();
                handle_crash(x, new_membership_set, delta_slaves);
            }
            if(crash_list.size() > 0){
                string to_print =  "Handle crash end: "+ to_string(cur_time_in_ms());
                print_to_sdfs_log(to_print, true);
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
    int last_underline_idx = -1;
    for(int i=str.length()-1;i>=0;i--){
        if(str[i] == '_'){
            last_underline_idx = i;
            break;
        }
    }
    unsigned int sum = 0;
    if(last_underline_idx == -1){
        for(auto s:str)sum = sum*19260817 + s;
    } else {
        for(int i=0;i<last_underline_idx;i++){
            sum = sum*19260817 + str[i];
        }
    }
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
                        thread(send_file, slave_file, slave_file, slave_id, "p", true).detach();
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
            thread(send_file, master_file, master_file, slave, "p", true).detach();
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
                thread(send_file, master_file, master_file, join_idx, "P", true).detach();
                master_file_to_delete.push_back(master_file);
            }
        }
        
        for(string filename : master_file_to_delete){
            master_files.erase(filename);
        }
        master_files_lock.unlock();
    }
    
    master_files_lock.lock();
    if(new_slaves.find(join_idx) != new_slaves.end()){
        for(string master_file : master_files)
            thread(send_file, master_file, master_file, join_idx, "p", true).detach();
    }
    master_files_lock.unlock();

    vector<string> slave_file_to_delete;
    
    slave_files_lock.lock();
    for(string slave_file : slave_files){
        if(new_masters.find(find_master(new_membership_set, hash_string(slave_file))) 
                == new_masters.end())
            slave_file_to_delete.push_back(slave_file);
    }
    for(string filename : slave_file_to_delete){
        slave_files.erase(filename);
        remove(filename.c_str());
    }
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

void deleteDirectoryContents(const std::filesystem::path& dir){
    for (const auto& entry : std::filesystem::directory_iterator(dir)) 
        std::filesystem::remove_all(entry.path());
}

void start_sdfs_service() {
    init_ip_list();
    deleteDirectoryContents("./sdfs_files/");
    system("mkdir ./sdfs_files/input");
    fsdfs_out.open( std::to_string(machine_idx + 1) + "_" + std::to_string(start_time_ms) +".log");
    start_membership_service(get_ip_address_from_index(machine_idx));
    
    thread(sdfs_message_receiver).detach();
    thread(membership_listener).detach();
    thread(file_receiver).detach();
}

vector<string> get_files_under_folder(const string &prefix){
    set<int> membership_list = get_current_live_membership_set();

    files_under_folder_lock.lock();
    files_under_folder.clear();
    file_folder_request_cnt = membership_list.size();
    files_under_folder_lock.unlock();

    int64_t query_start_time_ms = cur_time_in_ms();

    for(int member:membership_list) {
        thread(send_a_sdfs_message, "F"+prefix, member).detach();
    }

    while(true) { // block until all live member response or timeout
        // print_to_sdfs_log("time passed: " + to_string(cur_time_in_ms() - query_start_time_ms), true);
        if(cur_time_in_ms() - query_start_time_ms >= 10000)break;
        files_under_folder_lock.lock();
        int request_cnt = file_folder_request_cnt;
        files_under_folder_lock.unlock();
        // print_to_sdfs_log( "request cnt: " + to_string(request_cnt), true);
        if(request_cnt == 0)break;
    }

    files_under_folder_lock.lock();
    auto res = files_under_folder;
    files_under_folder_lock.unlock();
    return res;
}

void wait_until_all_files_are_processed(const vector<string> &files) {
    // TO BE DONE
    for(const string& file : files) { // block until all previous event are done
        auto event_num = write_lock(file);
        //do nothing
        update_finish_event(event_num, file);
    }
}

int main(int argc, char *argv[]){
    init_ip_list();
    deleteDirectoryContents("./sdfs_files/");
    system("mkdir ./sdfs_files/input");
    if(argc != 2){
        puts("FATAL: please assign machine index!");
        exit(0);
    }
    machine_idx = stoi(argv[1]) - 1;
    start_time_ms = cur_time_in_ms();
    fsdfs_out.open( std::to_string(machine_idx + 1) + "_" + std::to_string(start_time_ms) +".log");
    start_membership_service(get_ip_address_from_index(machine_idx));
    thread(sdfs_message_receiver).detach();
    thread(membership_listener).detach();
    thread(file_receiver).detach();
    
    string input;
    while(cin>>input){
        set<int> membership_set = get_current_live_membership_set();
        if(input == "Get" || input == "get" || input == "G" || input == "g") {
            string sdfsfilename;cin>>sdfsfilename;
            string localfilename;cin>>localfilename;
            // string to_print =  "Get command start: " + to_string(cur_time_in_ms());
            // print_to_sdfs_log(to_print, true);
            send_a_sdfs_message("G"+sdfsfilename+" "+localfilename+" "+to_string(machine_idx), find_master(membership_set, hash_string(sdfsfilename)));
        } else if(input == "Put" || input == "put" || input == "P" || input == "p") {
            string localfilename;cin>>localfilename;
            string sdfsfilename;cin>>sdfsfilename;
            // string to_print =  "Put command start: "+ to_string(cur_time_in_ms());
            // print_to_sdfs_log(to_print, true);
            thread(send_file, localfilename, sdfsfilename, find_master(membership_set, hash_string(sdfsfilename)), "P", false).detach();
        } else if(input == "Delete" || input == "delete" || input == "D" || input == "d") {
            string name;cin>>name;
            send_a_sdfs_message("D"+name, find_master(membership_set, hash_string(name)));
        } else if(input == "Store" || input == "store" || input == "S" || input == "s") {
            print_current_files();
        } else if(input == "ls" || input == "list") {
            string file_name;cin>>file_name;
            send_a_sdfs_message("L"+file_name+" "+to_string(machine_idx), find_master(membership_set, hash_string(file_name)));
        } else if(input == "multiread" || input == "mr"){
            string sdfsfilename;cin>>sdfsfilename;
            string localfilename;cin>>localfilename;
            int k;cin>>k;
            for(int i=1;i<=k;i++){
                int x;cin>>x;
                x--;
                if(!membership_set.count(x))continue;
                thread(send_a_sdfs_message, "G"+sdfsfilename+" "+localfilename+" "+to_string(x), find_master(membership_set, hash_string(sdfsfilename))).detach();
            }
        } else if(input == "list_mem" || input == "member" || input == "mem"){
            print_membership_list();
        } else if(input == "list_self" || input == "self") {
            cout << "machine index: " << machine_idx << endl;
        } else if(input == "leave") {
            return 0;
        } else if(input == "CHANGE" || input == "C") {
            local_mode_change();
            print_current_mode();
            group_mode_change();
        } else if(input == "LOCAL" || input == "LOCALC" || input == "LOCALCHANGE"){
            local_mode_change();
            print_current_mode();
        } else if(input == "Folder" || input == "F"){
            string prefix;cin>>prefix;
            auto res = get_files_under_folder(prefix);
            stringstream ss;
            ss << "get [";
            for(const string &file: res) {
                ss << file << ",";
            }  
            ss << "] from folder " << prefix; 
            print_to_sdfs_log(ss.str(), true);
        } else if(input == "Combine" || input == "combine"){
            string prefix; cin>> prefix;
            for(const int target_index : membership_set) {
                thread(send_a_sdfs_message, 'C' + prefix, target_index).detach();
            }
        } else{
            puts("Unsupported Command!");
        }
    }
}
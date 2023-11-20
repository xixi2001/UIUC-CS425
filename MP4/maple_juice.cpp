#include "maple_juice.h"
#include <thread>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

using namespace std;

bool hash_partition = true;
constexpr int mj_receiver_port = 1088;
constexpr int max_buffer_size = 1024;
int machine_idx;
int64_t start_time_ms;
set<int> remain_set;
mutex remain_set_lock;

void send_mj_message(const string& str, int target_index){
    string target_ip = get_ip_address_from_index(target_index);
	print_to_mj_log("Send a message to " + to_string(target_index+1) + ": " + str, true);
    int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
		puts("[Error] TCP message sender build fail!");
        return;
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=mj_receiver_port;

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
	close(fd);
    
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
        string ret;

        print_to_mj_log("Received a message: " + msg_str, false);
        
        switch(msg_str[0]){
            case 'M':
                command_queue.push(msg);
                break;
            case 'm':
                work_maple_task(msg_str, clifd);
                break;
            default:
                puts("Message is in wrong format.");
                throw runtime_error("Message is in wrong format.");
        }
        close(clifd);
	}
	close(fd);
}

vector<vector<string>> range_based_partition(vector<string>& files, int num_tasks, set<int> membership_set){
    vector<vector<string>> ret;
    int num_files_per_task = files.size() / num_tasks;
    int cnt = 0;
    int num_files_remain = files.size() % num_tasks;
    for(int i = 0; i < num_tasks; i++){
        vector<string> task_files;
        for(int j = 0; j < num_files_per_task; j++){
            task_files.push_back(files[cnt]);
            cnt++;
        }
            
        if(i < num_files_remain){
            task_files.push_back(files[cnt]);
            cnt++;
        }

        ret.push_back(task_files);
    }

    return ret;
}

vector<vector<string>>hash_based_partition(vector<string>& files, int num_tasks, set<int> membership_set){
    auto hash_file = [&num_tasks](const string &str)->int{
        unsigned int sum = 0;
        for(auto s:str)sum = sum*19260817 + s;
        return sum % num_tasks;
    };

    vector<vector<string>> ret = vector<vector<string>> (num_tasks, vector<string>(0));
    
    for(string file : files){
        int task_id = hash_file(file);
        ret[task_id].push_back(file);
    }

    return ret;
}

int finished_task;
mutex finished_task_lock; 

void command_queue_listener(){
    // TODO: later need to deal with invalid command
    while(true){
        if(command_queue.empty()) continue;
        
        string cur_cmd = command_queue.front();
        command_queue.pop();
        set<int> membership_set = get_current_live_membership_set();
        vector<string> info;
        info = tokenize(cur_cmd, ' ');
        
        vector<string> files = get_files_from_folder(info[4]);
        vector<vector<string>> mission_partitions;
        int num_tasks = stoi(info[2]);
        if(num_tasks > membership_set.size())
            num_tasks = membership_set.size();

        // divide input_file into chunks
        if(hash_partition)
            mission_partitions = hash_based_partition(files, num_tasks, membership_set);
        else
            mission_partitions = range_based_partition(files, num_tasks, membership_set);
        
        // initiate maple_task_monitor
        remain_set = membership_set;
        finished_task_lock.lock();
        finished_task = 0;
        finished_task_lock.unlock();
        for(int i = 0; i < num_tasks; i++){
            thread(maple_task_monitor, cur_cmd, mission_partitions[i]).detach();   
        }

        // count # of finished task, after all maple tasks have finished, send "C" message 
        while(finished_task != num_tasks)
            continue;
        
        membership_set = get_current_live_membership_set();
        for(int member : membership_set)
            send_a_sdfs_message("C" + cur_cmd, member);

    }
}

void work_maple_task(const string& cmd, int socket_num){
    vector<string> info = tokenize(cmd, ' ');
    system("mkdir ./input");
    
    // get files from sdfs
    set<int> membership_set = get_current_live_membership_set();
    for(int i = 6; i < info.size(); i++){
        thread(send_a_sdfs_message,"G"+info[i]+" "+"./input/"+info[i]+" "+to_string(machine_idx), find_master(membership_set, hash_string(info[i]))).detach();
    }
    wait_until_all_files_are_received();
    
    // call maple_exec
    string maple_exe = info[1];
    system("mkdir ./result");
    for(int i = 6; i < info.size(); i++){
        string cmd = "./" + maple_exe + " ./input/" + info[i] + " >> ./result/temp_result";
        system(cmd.c_str());
    }

    map<string, vector<string>> maple_result;
    
    ifstream infile("./result/temp_result");
    string k, v;
    while (infile >> k >> v)
        maple_result[k].push_back(v);

    infile.close();
    
    string sdfs_intermediate_filename_prefix = info[3];
    for(auto pair : maple_result){
        ofstream outfile("./result/" + sdfs_intermediate_filename_prefix+"_"+pair.first+"_"+to_string(machine_idx));
        for(string val : pair.second){
            outfile << val << endl;
        }
        outfile.close();
    }

    // Put execution result
    vector<thread> sendTasks;
    for(auto pair : maple_result){
        string filename = sdfs_intermediate_filename_prefix+"_"+pair.first+"_"+to_string(machine_idx);
        sendTasks.push_back(thread(send_file, "./result/" + filename, filename, find_master(membership_set, hash_string(filename)), "P", false));
    }

    for (thread & th : sendTasks)
            th.join();
    
    // send success (S message) to leader & delete temp files
    int nbytes;
    string res = "S";
    if((nbytes=send(socket_num, res.c_str(), res.size(), 0))<0){ 
        puts("[ERROR] Socket write fail!");
        return;
    }
    deleteDirectoryContents("./result/");
    deleteDirectoryContents("./input/");
    system("rm ./result");
    system("rm ./input");

}


void maple_task_monitor(const string& cmd, const vector<string>& files){
    try{
        remain_set_lock.lock();
        if(remain_set.size() == 0) 
            remain_set = get_current_live_membership_set();
        int target_index = *remain_set.begin();
        remain_set.erase(target_index);
        remain_set_lock.unlock();

        string cur_cmd = "m" + cmd + " " + to_string(files.size());
        for(string file : files)
            cur_cmd += (" " +  file);

        string target_ip = get_ip_address_from_index(target_index);
        print_to_mj_log("Send a message to " + to_string(target_index + 1) + ": " + cur_cmd, true);
        
        int fd;
        if((fd=socket(AF_INET,SOCK_STREAM,0))<0){
            throw runtime_error("Failure in create socket");
        }
        struct sockaddr_in srv;
        srv.sin_family=AF_INET;
        srv.sin_port=mj_receiver_port;

        if (inet_pton(AF_INET, target_ip.c_str(), &srv.sin_addr) <= 0) {
            throw runtime_error("Invalid address/ Address not supported!");
        }

        if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
            throw runtime_error("[Error] Timeout when connecting!");
        }
        // send command to the server
        int nbytes;
        if((nbytes=send(fd, cur_cmd.c_str(), cur_cmd.size(), 0))<0){ 
            throw runtime_error("[Error] Socket write fail!");
        }
        
        char buf[max_buffer_size];
        memset(buf, 0, sizeof(buf));
        if((nbytes = read(fd, buf, sizeof(buf)))<=0){ 
            throw runtime_error("[Error] Socket write fail!");
        }

        close(fd);

        finished_task_lock.lock();
        finished_task++;
        finished_task_lock.unlock();
    }
    catch(runtime_error &e){
        thread(maple_task_monitor, cmd, files).detach();   
    }
}

mutex mj_out_lock;
ofstream mj_out;
void print_to_mj_log(const string& str, bool flag){
    int64_t current_time_ms = cur_time_in_ms(); 
    if(flag)
        cout << "[" << current_time_ms - start_time_ms << "] " << str << endl;
    mj_out_lock.lock();
    mj_out << "[" << current_time_ms - start_time_ms << "] " << str << endl;
    mj_out_lock.unlock();
}

int main(int argc, char *argv[]){
    if(argc != 2){
        puts("FATAL: please assign machine index!");
        exit(0);
    }
    machine_idx = stoi(argv[1]) - 1;
    start_time_ms = cur_time_in_ms();
    mj_out.open( "mj_" + std::to_string(machine_idx + 1) + "_" + std::to_string(start_time_ms) +".log");
    
    thread(mj_message_receiver).detach();
    
    string input;
    while(cin>>input){
        set<int> membership_set = get_current_live_membership_set();
        if(input == "maple" || input == "Maple" || input == "M" || input == "m") {
            string maple_exe;cin>>maple_exe;
            string num_maples;cin>>num_maples;
            string sdfs_intermediate_filename_prefix;cin>>sdfs_intermediate_filename_prefix;
            string sdfs_src_directory;cin>>sdfs_src_directory;
            string to_print =  "Get command start: " + to_string(cur_time_in_ms());
            print_to_sdfs_log(to_print, true);
            send_mj_message("maple " + maple_exe + " " + num_maples + " " + sdfs_intermediate_filename_prefix + " " + sdfs_src_directory, 0);
        }
    }
}


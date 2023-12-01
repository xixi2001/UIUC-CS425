#include "maple_juice.h"
#include <chrono>
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
	print_to_mj_log("Send a message to " + to_string(target_index+1) + ": " + str, false);
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
            case 'J':
                command_queue.push(msg);
                break;
            case 'j':
                work_juice_task(msg_str, clifd);
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

int hash_file(const string &str, int num_tasks){
    unsigned int sum = 0;
    for(auto s:str)sum = sum*19260817 + s;
    return sum % num_tasks;
}

vector<vector<string>>hash_based_partition(vector<string>& files, int num_tasks, set<int> membership_set){

    vector<vector<string>> ret = vector<vector<string>> (num_tasks, vector<string>(0));
    
    for(string file : files){
        int task_id = hash_file(file, num_tasks);
        ret[task_id].push_back(file);
    }

    return ret;
}

int finished_task;
mutex finished_task_lock; 

void command_queue_listener(){
    // TODO: later probably need to deal with invalid command
    while(true){
        if(command_queue.empty()) continue;

        command_queue_lock.lock();
        string cur_cmd = command_queue.front();
        command_queue.pop();
        command_queue_lock.unlock();
        print_to_mj_log("Process a command"  + cur_cmd, false);
        set<int> membership_set = get_current_live_membership_set();
        vector<string> info;
        info = tokenize(cur_cmd, ' ');
        
        vector<string> files;
        if(cur_cmd[0] == 'M') 
            files = get_files_from_folder(info[4]);
        else if(cur_cmd[0] == 'J')
           files = get_files_from_folder(info[3] + "_");
        
        vector<vector<string>> mission_partitions;
        int num_tasks = stoi(info[2]);
        if(num_tasks > membership_set.size())
            num_tasks = membership_set.size();

        // divide input_file into chunks
        if(hash_partition)
            mission_partitions = hash_based_partition(files, num_tasks, membership_set);
        else
            mission_partitions = range_based_partition(files, num_tasks, membership_set);
        
        // initiate task_monitor
        remain_set = membership_set;
        finished_task_lock.lock();
        finished_task = 0;
        finished_task_lock.unlock();
        for(int i = 0; i < num_tasks; i++){
            thread(task_monitor, cur_cmd, mission_partitions[i]).detach();   
        }

        // count # of finished task, after all maple tasks have finished, send "C" message 
        while(finished_task != num_tasks)
            continue;
        

        membership_set = get_current_live_membership_set();
        if(cur_cmd[0] == 'M'){
            for(int member : membership_set)
                send_a_sdfs_message("C" + info[3], member);
        }
        else if(cur_cmd[0] == 'J'){
            for(int member : membership_set)
                send_a_sdfs_message("C" + info[4], member);
        }

        print_to_mj_log("Finish processing command"  + cur_cmd, false);
    }
}


void work_maple_task(const string& cmd, int socket_num){
    vector<string> info = tokenize(cmd, ' ');
    system("mkdir ./local_input_maple");

    int cmd_para_size = stoi(info[5]);string cmd_para;
    for(int i = 6; i < 6 + cmd_para_size; i++){
        cmd_para += info[i] + " ";
    }
    print_to_mj_log("Maple task parameter: " + cmd_para , false);
    
    // get files from sdfs
    set<int> membership_set = get_current_live_membership_set();
    vector<string> files_to_be_got;
    for(int i = 7 + cmd_para_size; i < info.size(); i++){
        vector<string> file_path = tokenize(info[i], '/');
        string file_name = file_path.back();
        send_a_sdfs_message("G"+info[i]+" "+"./local_input_maple/"+file_name+" "+to_string(machine_idx), 
            find_master(membership_set, hash_string(info[i], true)));
        files_to_be_got.push_back("./local_input_maple/"+file_name);
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    wait_until_all_files_are_processed(files_to_be_got);
    print_to_mj_log("[worker]: All files are received", false);
    
    // call maple_exec
    string maple_exe = info[1];
    system("mkdir ./local_result_maple");
    for(int i = 7 + cmd_para_size; i < info.size(); i++){
        vector<string> file_path = tokenize(info[i], '/');
        string file_name = file_path.back();
        string cmd = "./" + maple_exe + " ./local_input_maple/" + file_name + " " + cmd_para + " >> ./local_result_maple/temp_result";
        print_to_mj_log("Execute: " + cmd, true);
        system(cmd.c_str());
    }
    print_to_mj_log("[worker]: temp_result is generated", false);

    map<string, vector<string>> maple_result;
    
    ifstream infile("./local_result_maple/temp_result");
    string input_line;
    while(getline(infile, input_line)) {
        vector<string> tmp = tokenize(input_line, ' ');
        string k = tmp[0];
        string v;
        if(tmp.size() >= 2) {
            v = tmp[1];
            for(int i=2;i<tmp.size(); i++)
                v = ' ' + tmp[i];
        }
        maple_result[k].push_back(v);
    }

    infile.close();
    auto two_digit_index = [](int machine_idx) {
        if(machine_idx <= 9){
            return "0" + to_string(machine_idx);
        } else {
            return to_string(machine_idx);
        }
    };
    string sdfs_intermediate_filename_prefix = info[3];
    for(auto pair : maple_result){
        ofstream outfile("./local_result_maple/" + sdfs_intermediate_filename_prefix+"_"+pair.first+"_"+two_digit_index(machine_idx + 1));
        for(string val : pair.second){
            outfile << pair.first << " " << val << endl;
        }
        outfile.close();
    }
    print_to_mj_log("[worker]: output files are generated", false);

    // Put execution result
    vector<string> files_to_be_sent;
    for(auto pair : maple_result){
        string filename = sdfs_intermediate_filename_prefix+"_"+pair.first+"_"+two_digit_index(machine_idx + 1);
        thread(send_file, "./local_result_maple/" + filename, filename, find_master(membership_set, hash_string(filename, true)), "P", false).detach();
        files_to_be_sent.push_back("./local_result_maple/" + filename);
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    wait_until_all_files_are_processed(files_to_be_sent);
    // Still need to wait for slave files to be transferred, large file might cause error here
    this_thread::sleep_for(chrono::milliseconds(100)); 
    
    // send success (S message) to leader & delete temp files
    int nbytes;
    string res = "S";
    if((nbytes=send(socket_num, res.c_str(), res.size(), 0))<0){ 
        puts("[ERROR] Socket write fail!");
        return;
    }

    print_to_mj_log("[worker]: output files successfully sent", false);

    system("rm -rf ./local_result_maple");
    system("rm -rf ./local_input_maple");

}

void work_juice_task(const string& cmd, int socket_num){
    vector<string> info = tokenize(cmd, ' ');
    system("mkdir ./local_input_juice");

    int cmd_para_size = stoi(info[6]);string cmd_para;
    for(int i = 7; i < 7 + cmd_para_size; i++){
        cmd_para += info[i] + " ";
    }
    print_to_mj_log("Juice task parameter: " + cmd_para, false);
    
    // get files from sdfs
    set<int> membership_set = get_current_live_membership_set();
    vector<string> files_to_be_got;
    for(int i = 8 + cmd_para_size; i < info.size(); i++){
        string file_name = info[i];
        send_a_sdfs_message("G"+info[i]+" "+"./local_input_juice/"+file_name+" "+to_string(machine_idx), find_master(membership_set, hash_string(info[i], false)));
        files_to_be_got.push_back("./local_input_juice/"+file_name);
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    wait_until_all_files_are_processed(files_to_be_got);
    print_to_mj_log("[worker]: All files are received", false);

    auto two_digit_index = [](int machine_idx) {
        if(machine_idx <= 9){
            return "0" + to_string(machine_idx);
        } else {
            return to_string(machine_idx);
        }
    };
    
    // call juice_exec
    string sdfs_dest_filename = info[4];
    string juice_exe = info[1];
    system("mkdir ./local_result_juice");
    for(int i = 8 + cmd_para_size; i < info.size(); i++){
        string file_name = info[i];
        string cmd = "cat ./local_input_juice/" + file_name + " | " + "./" + juice_exe
            + " " + cmd_para
            + " >> ./local_result_juice/"+sdfs_dest_filename+"_" + two_digit_index(machine_idx);
        print_to_mj_log("Execute: " + cmd, true);
        system(cmd.c_str());
    }
    print_to_mj_log("[worker]: result is generated", false);

    // Put execution result
    vector<string> files_to_be_sent;
    string filename = sdfs_dest_filename + "_" + two_digit_index(machine_idx);
    thread(send_file, "./local_result_juice/" + filename, filename, find_master(membership_set, hash_string(filename, true)), "P", false).detach();
    files_to_be_sent.push_back("./local_result_juice/" + filename);
    
    this_thread::sleep_for(chrono::milliseconds(100));
    wait_until_all_files_are_processed(files_to_be_sent);
    // Still need to wait for slave files to be transferred, large file might cause error here
    this_thread::sleep_for(chrono::milliseconds(100)); 
    
    // send success (S message) to leader & delete temp files
    int nbytes;
    string res = "S";
    if((nbytes=send(socket_num, res.c_str(), res.size(), 0))<0){ 
        puts("[ERROR] Socket write fail!");
        return;
    }

    print_to_mj_log("[worker]: juice result successfully sent", false);

    system("rm -rf ./local_result_juice");
    system("rm -rf ./local_input_juice");
}


void task_monitor(const string& cmd, const vector<string>& files){
    try{
        remain_set_lock.lock();
        if(remain_set.size() == 0) 
            remain_set = get_current_live_membership_set();
        int target_index = *remain_set.begin();
        remain_set.erase(target_index);
        remain_set_lock.unlock();

        string cur_cmd;
        if(cmd[0] == 'M') 
            cur_cmd = "m" + cmd + " " + to_string(files.size());
        else if(cmd[0] == 'J')
            cur_cmd = "j" + cmd + " " + to_string(files.size());

        for(string file : files)
            cur_cmd += (" " +  file);
        
        print_to_mj_log("[leader]: distribute \"" + cur_cmd + "\" to worker " + to_string(target_index + 1), true);
        string target_ip = get_ip_address_from_index(target_index);
        
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
        print_to_mj_log("worker " + to_string(target_index + 1) + " has finished the task", false);
    }
    catch(runtime_error &e){
        thread(task_monitor, cmd, files).detach();   
        print_to_mj_log("[leader]: restart task_monitor", false);
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
    
    start_sdfs_service();
    thread(mj_message_receiver).detach();
    if(machine_idx == 0) thread(command_queue_listener).detach();
    
    string input_line;
    while(getline(cin, input_line)){
        set<int> membership_set = get_current_live_membership_set();
        stringstream ss(input_line);
        string input;ss >> input;
        if(input == "Get" || input == "get" || input == "G" || input == "g") {
            string sdfsfilename;ss>>sdfsfilename;
            string localfilename;ss>>localfilename;
            send_a_sdfs_message("G"+sdfsfilename+" "+localfilename+" "+to_string(machine_idx), find_master(membership_set, hash_string(sdfsfilename, true)));
        } else if(input == "Put" || input == "put" || input == "P" || input == "p") {
            string localfilename;ss>>localfilename;
            string sdfsfilename;ss>>sdfsfilename;
            thread(send_file, localfilename, sdfsfilename, find_master(membership_set, hash_string(sdfsfilename, true)), "P", false).detach();
        } else if(input == "Delete" || input == "delete" || input == "D" || input == "d") {
            string name;ss>>name;
            send_a_sdfs_message("D"+name, find_master(membership_set, hash_string(name, true)));
        } else if(input == "Store" || input == "store" || input == "S" || input == "s") {
            print_current_files();
        } else if(input == "ls" || input == "list") {
            string file_name;ss>>file_name;
            send_a_sdfs_message("L"+file_name+" "+to_string(machine_idx), find_master(membership_set, hash_string(file_name, true)));
        } else if(input == "maple" || input == "Maple" || input == "M" || input == "m") {
            string maple_exe;ss>>maple_exe;
            string num_maples;ss>>num_maples;
            string sdfs_intermediate_filename_prefix;ss>>sdfs_intermediate_filename_prefix;
            string sdfs_src_directory;ss>>sdfs_src_directory;
            string to_print =  "Get command start: " + to_string(cur_time_in_ms());
            print_to_sdfs_log(to_print, true);
            send_mj_message("M " + maple_exe + " " + num_maples + " " + sdfs_intermediate_filename_prefix + " " + sdfs_src_directory, 0);
        } else if(input == "juice" || input == "Juice" || input == "J" || input == "j") {
            string juice_exe;ss>>juice_exe;
            string num_juices;ss>>num_juices;
            string sdfs_intermediate_filename_prefix;ss>>sdfs_intermediate_filename_prefix;
            string sdfs_dest_filename;ss>>sdfs_dest_filename;
            string delete_input;ss>>delete_input;
            string to_print =  "Get command start: " + to_string(cur_time_in_ms());
            print_to_sdfs_log(to_print, true);
            send_mj_message("J " + juice_exe + " " + num_juices + " " + sdfs_intermediate_filename_prefix
                 + " " + sdfs_dest_filename + " " + delete_input, 0);
        } else if(input == "SELECT" || input == "select") {
            vector<string> sql = tokenize(input_line, ' ');
            string regex_command;int regex_size = 0;
            for(int i=5; i+1 < sql.size();i+=3){
                regex_size += 2;
                regex_command += sql[i] + " " + sql[i+1];
            }
                
            print_to_mj_log("regex: " + regex_command, true);
            send_mj_message("M maple_sql 1 sql "+ sql[3] + " " + 
                to_string(regex_size) + " " + regex_command, 0);
            send_mj_message("J juice_sql 1 sql sqlResult 0 0", 0);
        } else if (input == "Detection" || input == "detection" || 
                    input == "Detect" || input == "detect"){
            string file_name, target;
            ss >> file_name >> target;
            send_mj_message("M maple_test 1 detect "+ file_name + " 1 " + target, 0);
            send_mj_message("J juice_test 1 detect detectResult 0 0", 0);
        } else {
            puts("Unsupported command!");
        }
    }
}


// put A input/A
// put B input/B
// put C input/C
// maple maple_test 3 wc input/
// juice juice_test 3 wc wcResult 0

/*
Test 1
put TSI.csv input/TSI
detect input/ Radio
*/

/*
Test 2
put TSI.csv input/TSI
SELECT ALL FROM input/ WHERE Interconne None
*/


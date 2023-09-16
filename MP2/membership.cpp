#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "membership.h"
using namespace std;
const string introducer_ip_address = "fa23-cs425-2401.cs.illinois.edu";
constexpr double heartbeat_interval_sec = 0.5;
constexpr int heartbeat_number = 5;
constexpr int max_buffer_size = 1024;
constexpr int listen_port = 8818;
constexpr int fail_time_ms = 3000;
constexpr int suspect_time_ms = 2000;
constexpr int suspect_timeout_ms = 2000;
constexpr int cleanup_time_ms = 15000;
void message_receiver() {

}
map<string, MemberEntry> message_to_member_entry(const string &str){
    map<string, MemberEntry> res;
    return res;
}

void combine_member_entry(const map<pair<string,int64_t>, MemberEntry> &other){

}

void heartbeat_sender(){

}

string member_entry_to_message(){
    string res;
    return res;
}

void failure_detector(){

}

void join_group(){

}
void response_join(){

}

void load_introducer_from_file(){

}
int main(int argc, char *argv[]){
    if(argc != 2 || argc != 3){
		puts("FATAL: please assign machine number!");
		exit(1);
	}
    int machine_id = stoi(argv[1]);
    if(machine_id == 1){ // introducer 
        load_introducer_from_file();
    } else { // others join the group 
        join_group();
    }
    if(argc == 3){
        suspection_mode = stoi(argv[2]);
    }
    print_current_mode();
    thread receiver(message_receiver);receiver.detach();
    thread sender(heartbeat_sender);sender.detach();
    thread detector(failure_detector);detector.detach();

    string input;
    while(cin >> input){
        if(input == "LEAVE") {
            // send leave message
        } else if(input == "CHANGE") {
            suspection_mode ^= 1;
            print_current_mode();
        } else {
            puts("Unsupported command, input again!");
        }
    }
}
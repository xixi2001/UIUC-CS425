#include <thread>
#include <set>
#include <sys/socket.h> 
#include <netdb.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include "membership.h"
using namespace std;
const string introducer_ip_address = "fa23-cs425-2401.cs.illinois.edu";
constexpr double heartbeat_interval_sec = 0.5;
constexpr int heartbeat_number = 5;
constexpr int max_buffer_size = 2048;
constexpr int listen_port = 8818;
constexpr int send_port = 8815;
constexpr int fail_time_ms = 3000;
constexpr int suspect_time_ms = 2000;
constexpr int suspect_timeout_ms = 2000;
constexpr int cleanup_time_ms = 15000;
constexpr int64_t leave_heartbeat = 1e13;

void message_receiver() {
    // open socket
    char buffer[max_buffer_size];

    int sockfd, clientlen;
    struct sockaddr_in serveraddr, clientaddr;
    clientlen = sizeof(clientaddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        puts("Message receiver connect fail!");
        throw runtime_error("Message receiver connect fail!");
    }
    
    bool reuse_addr = true;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuse_addr, sizeof(reuse_addr))){
        puts("Failure in UDP setsockopt");
        throw runtime_error("Failure in UDP setsockopt");
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(send_port);

    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
        puts("Message receiver bind UDP fail!");
        throw runtime_error("Message receiver bind UDP fail!");
    }

    while(true){ 
        // receive_message 
        if(recvfrom(sockfd, (char *) buffer, max_buffer_size, 0, (struct sockaddr *) &clientaddr,
                reinterpret_cast<socklen_t *>(&clientlen)) < 0) continue;
        string msg(buffer);
        switch(msg[0]){
            case 'J':
                response_join();
                break;
            case 'G':
                combine_member_entry(message_to_member_entry(msg.substr(1)));
                break;
            case 'L':
                member_status_lock.lock();
                // member_status[]
                member_status_lock.unlock();
                break;
            default:
                throw("FATAL: ");
        }

    }
}
map<pair<string, int64_t>, MemberEntry> message_to_member_entry(const string &str){
    map<pair<string, int64_t>, MemberEntry> res;
    return res;
}

void combine_member_entry(const map<pair<string,int64_t>, MemberEntry> &other){

}

vector<string> random_choose_send_target(set<string> &previous_sent){
    vector<string> choose_ip_from;
    vector<string> alive_ip;
    
    member_status_lock.lock();
    for (auto entry : member_status){
        if(entry.second.status == 0) continue;
        if(entry.first.first == machine_id.first) continue;
        alive_ip.push_back(entry.first.first);
        if(previous_sent.find(entry.first.first) != previous_sent.end()) continue;
        choose_ip_from.push_back(entry.first.first);
    }
    
    vector<string> send_target;
    if(alive_ip.size() <= heartbeat_number){
        for(string ip : alive_ip)
            send_target.push_back(ip);
    }
    else if(choose_ip_from.size() == heartbeat_number){
        previous_sent = {};
        for(string ip: choose_ip_from){
            send_target.push_back(ip);
        }
    }
    else if(choose_ip_from.size() < heartbeat_number){
        previous_sent = {};
        for(string ip: choose_ip_from)
            send_target.push_back(ip);
        random_shuffle(alive_ip.begin(), alive_ip.end());
        for(int i = 0; i < heartbeat_number - send_target.size(); i++){
            send_target.push_back(alive_ip[i]);
            previous_sent.insert(choose_ip_from[i]);
        }
    }
    else if(choose_ip_from.size() > heartbeat_number){
        random_shuffle(choose_ip_from.begin(), choose_ip_from.end());
        for(int i = 0; i < heartbeat_number; i++){
            send_target.push_back(choose_ip_from[i]);
            previous_sent.insert(choose_ip_from[i]);
        }
    }
    member_status_lock.unlock();
    return send_target;
}

void heartbeat_sender(){
    // assume there is only one server on a single ip address
    set<string> previous_sent;
    while(true){
        //(1) randomly select, reminder: use lock
        vector<string> target_ips = random_choose_send_target(previous_sent);

        //(2) update your own member entry
        int64_t cur_time = cur_time_in_ms();
        member_status[machine_id].time_stamp_ms = cur_time;
        member_status[machine_id].heart_beat_counter = cur_time;

        //(3) open socket and send the message
        string msg = member_entry_to_message();
        for(int i = 0; i < target_ips.size(); i++){
            int sockfd;
            struct sockaddr_in servaddr;
            struct hostent *server;

            if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
                puts("Hearbeat sender connect failed!");
                continue;
            }
            
            server = gethostbyname(target_ips[i].c_str());
            if (server == nullptr)
                puts("Heartbeat sender cannot get host by name!");

            memset(&servaddr, 0, sizeof(servaddr));
            
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(listen_port);
            bcopy((char *) server->h_addr, (char *) &servaddr.sin_addr.s_addr, server->h_length);
            
            int n;
            socklen_t len;
            
            if(sendto(sockfd, msg.c_str(), msg.size(),
                MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
                    sizeof(servaddr)) < 0){
                        puts("Heartbeat sender fail to send message!");
            }
        
            close(sockfd);
        }
        sleep(heartbeat_interval_sec);
    }
}

string member_entry_to_message(){
    string res;
    return res;
}

void failure_detector(){

}

void join_group(){
    // TODO: add machine_id timestamp
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
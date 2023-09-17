#include <thread>
#include <set>
#include <sys/socket.h> 
#include <netdb.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cassert>
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
constexpr int64_t leave_heart_beat = 5e13;
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
                response_join(msg.substr(1));
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
    close(sockfd);
}
int64_t ParseIntUntil(int &idx,const string& str, char c){
    int64_t res = 0;
    while(idx < str.length() && str[idx] != c) {
        assert(str[idx] >= '0' && str[idx] <= '9');
        res = res * 10 + str[idx] - '0';
        idx++;
    }
    idx++;
    return res;
}
string ParseStringUntil(int &idx,const string& str, char c){
    string res;
    while(idx < str.length() && str[idx] != c) {
        res += str[idx];
        idx++;
    }
    idx++;
    return res;   
}

map<pair<string,int64_t>, MemberEntry> message_to_member_entry(const string &str){
    map<pair<string,int64_t>, MemberEntry> res;
    int idx = 0;
    int entry_cnt = ParseIntUntil(idx, str, '#');
    for(int i = 1; i <= entry_cnt; i++) {
        string ip = ParseStringUntil(idx, str, '#');
        int64_t stamp = ParseIntUntil(idx, str, '#');
        MemberEntry entry;
        entry.time_stamp_ms = ParseIntUntil(idx, str, '#');
        entry.heart_beat_counter = ParseIntUntil(idx, str, '#');
        entry.status = ParseIntUntil(idx, str, '#');
        entry.incarnation_count = ParseIntUntil(idx, str, '#');
        cout << "->" << ip << " " << stamp << endl;
        res[make_pair(ip, stamp)] = entry;
    }
    return res;
}

void combine_member_entry(const map<pair<string,int64_t>, MemberEntry> &other){
    int64_t current_time_ms = cur_time_in_ms();
    member_status_lock.lock();
    for(const auto &[id, entry] : other){
        if(!member_status.count(id)){
            member_status[id] = entry;
        } else{
            auto &correspond = member_status[id];
            if(entry.status == 0 || correspond.status == 0){
                correspond.status = 0;// failure node should remain failure
                continue;
            }
            // heartbeat
            if(entry.heart_beat_counter > correspond.heart_beat_counter) {
                correspond.heart_beat_counter = entry.heart_beat_counter;
                correspond.time_stamp_ms = current_time_ms;   
            }
            // suspection
            if(entry.incarnation_count > correspond.incarnation_count ||
                (entry.incarnation_count == correspond.incarnation_count &&
                    entry.status < correspond.status)) {
                correspond.incarnation_count = entry.incarnation_count;
                correspond.status = entry.status;
            }
        }
    }
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
        string msg = "G" + member_entry_to_message();
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
    member_status_lock.lock();
    stringstream res;
    res << member_status.size();
    res << '#';
    for(auto id_entry:member_status) {
        res<<id_entry.first.first << '#';
        res<<id_entry.first.second << '#';
        res<<id_entry.second.time_stamp_ms << "#";
        res<<id_entry.second.heart_beat_counter << "#";
        res<<id_entry.second.status << "#";
        res<<id_entry.second.incarnation_count << "#";
    }
    member_status_lock.unlock();
    return res.str();
}

void failure_detector(){
    fstream fout(machine_id.first + ".log");
    auto GetNodeId = [](const pair<string,int64_t> &machine_id) -> string {
        return "(" + machine_id.first +", " + std::to_string(machine_id.second)  + ")";
    };
    while(true){
        int64_t current_time_ms = cur_time_in_ms();
        member_status_lock.lock();
        bool has_failure = false;
        for(auto&[id, entry] : member_status) {
            auto time_stamp_ms = entry.time_stamp_ms;
            if(time_stamp_ms == leave_heart_beat) {
                //node has leave
                entry.status = 0; // 0 for failure 
                fout << GetNodeId(id) << " has left" << endl;
                has_failure = true;
                continue;
            }
            if(suspection_mode) {
                if(current_time_ms - time_stamp_ms >= suspect_time_ms) {
                    entry.status = 1; // 1 for suspect
                    fout <<  "Suspect " << GetNodeId(id) << endl;
                }
                if(current_time_ms - time_stamp_ms >= suspect_time_ms + suspect_timeout_ms) {
                    entry.status = 0; // 0 for failure
                    fout << GetNodeId(id) << " is failure" << endl;
                    has_failure = true;
                }
            } else{
                if(current_time_ms - time_stamp_ms >= fail_time_ms){
                    entry.status = 0; // 0 for failure
                    fout << GetNodeId(id) << " is failure" << endl;
                    has_failure = true;
                }
            }   
        }
        if(has_failure) {
            fout << "New Membership List: " << endl;
            for(const auto&[id, entry] : member_status) {
                if(entry.status > 0) {
                    fout << GetNodeId(id);
                }
            }
            fout << endl;
        }
        member_status_lock.unlock();
        save_current_status_to_log();
        sleep(0.5);
    }
}

void join_group(){
    // TODO: add machine_id timestamp
    while(true){
        int sockfd_send;
        struct sockaddr_in servaddr;
        struct hostent *server;

        if ( (sockfd_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
            puts("Hearbeat sender connect failed!");
            continue;
        }
        
        server = gethostbyname(introducer_ip_address.c_str());
        if (server == nullptr)
            puts("Heartbeat sender cannot get host by name!");

        memset(&servaddr, 0, sizeof(servaddr));
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(listen_port);
        bcopy((char *) server->h_addr, (char *) &servaddr.sin_addr.s_addr, server->h_length);
        
        int n;
        socklen_t len;
        string msg = "J" + machine_id.first + "#" + to_string(machine_id.second); 
        if(sendto(sockfd_send, msg.c_str(), msg.size(),
            MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
                sizeof(servaddr)) < 0){
                    puts("Heartbeat sender fail to send message!");
        }

        close(sockfd_send);

        char buffer[max_buffer_size];

        int sockfd_recv, clientlen;
        struct sockaddr_in serveraddr, clientaddr;
        clientlen = sizeof(clientaddr);

        if ((sockfd_recv = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
            puts("Message receiver connect fail!");
            throw runtime_error("Message receiver connect fail!");
        }
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd_recv, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))){
            puts("Failure in UDP setsockopt");
            throw runtime_error("Failure in UDP setsockopt");
        }

        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = INADDR_ANY;
        serveraddr.sin_port = htons(send_port);

        // probably has problem (bind might not needed)
        if (bind(sockfd_recv, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
            puts("Message receiver bind UDP fail!");
            throw runtime_error("Message receiver bind UDP fail!");
        }
        // receive_message 
        if(recvfrom(sockfd_recv, (char *) buffer, max_buffer_size, 0, (struct sockaddr *) &clientaddr,
                reinterpret_cast<socklen_t *>(&clientlen)) > 0){ 
                close(sockfd_recv);
                break;
        }
        else{
            close(sockfd_recv);
            puts("Join timeout. Reconnect again...");
            sleep(0.5);
        }
    }
}
void response_join(const string &str){
    int idx = 1;
    string ip = ParseStringUntil(idx, str, '#');

    int sockfd_send;
    struct sockaddr_in servaddr;
    struct hostent *server;

    if ( (sockfd_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        puts("Hearbeat sender connect failed!");
        return;
    }
    
    server = gethostbyname(ip.c_str());
    if (server == nullptr)
        puts("Heartbeat sender cannot get host by name!");

    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(listen_port);
    bcopy((char *) server->h_addr, (char *) &servaddr.sin_addr.s_addr, server->h_length);
    
    int n;
    socklen_t len;
    string msg = "Response"; 
    if(sendto(sockfd_send, msg.c_str(), msg.size(),
        MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
            sizeof(servaddr)) < 0){
                puts("Heartbeat sender fail to send message!");
    }

    close(sockfd_send);
}

void load_introducer_from_file() {
    string log_file_name = machine_id.first + to_string(machine_id.second);
    ifstream fin( log_file_name + ".log");
    string str;fin>>str;
    auto res = message_to_member_entry(str);
    member_status_lock.lock();
    member_status = res;
    if(!member_status.count(machine_id)) {
        member_status.emplace(machine_id, cur_time_in_ms());
    }
    member_status[machine_id].time_stamp_ms = cur_time_in_ms();
    member_status[machine_id].heart_beat_counter = cur_time_in_ms();
    member_status_lock.unlock();
    fin.close();
}

void save_current_status_to_log() {
    string log_file_name = machine_id.first + to_string(machine_id.second);
    ofstream fout( log_file_name + ".log");
    fout << member_entry_to_message() << endl;
    fout.close();
}

int main(int argc, char *argv[]){
    if(argc != 2 || argc != 3){
		puts("FATAL: please assign machine number!");
		exit(1);
	}
    machine_id.first = stoi(argv[1]);
    machine_id.second = cur_time_in_ms();
    if(machine_id.first == 1){ // introducer 
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
#include <thread>
#include <set>
#include <chrono>
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
const string introducer_ip_address = "172.22.94.78";
constexpr int heartbeat_number = 5;
constexpr int max_buffer_size = 2048;
constexpr int port_num = 8815;
constexpr int fail_time_ms = 3000;
constexpr int suspect_time_ms = 1500;
constexpr int suspect_timeout_ms = 1500;
constexpr int cleanup_time_ms = 15000;
constexpr int heartbeat_rate_ms = 500;
constexpr int64_t leave_heart_beat = 5e13;
constexpr double dropout_rate = 00;

mutex member_status_lock;
map<pair<string,int64_t>, MemberEntry> member_status;
// id: <ip_address, time_stamp>

pair<string, int64_t> machine_id;
 
bool suspection_mode = 0;// only one writter, no mutex needed

mutex fmembership_out_lock;
ofstream fmembership_out;

map<string, string> ip_to_machine; // only write in main, no mutex needed 
map<int, string> machine_idx_to_ip;

int64_t cur_time_in_ms(){
    return (int64_t) chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
}

inline string node_id_to_string(const pair<string,int64_t> &machine_id) {
    return "(" + machine_id.first +", " + std::to_string(machine_id.second)  + ")";
}

inline void print_to_membership_log(const string &str, bool print_out){
    int64_t current_time_ms = cur_time_in_ms(); 
    if(print_out)
        cout << "[" << current_time_ms - machine_id.second << "] " << str << endl;
    fmembership_out_lock.lock();
    fmembership_out << "[" << current_time_ms - machine_id.second << "] " << str << endl;
    fmembership_out_lock.unlock();
}

bool print_cmp(pair <pair<string,int64_t>, MemberEntry > A, pair <pair<string,int64_t>, MemberEntry > B) {
    return stoi(A.first.first) < stoi(B.first.first);
}

void print_membership_list(){
    vector<pair <pair<string,int64_t>, MemberEntry > >tmp; 
    member_status_lock.lock();
    for(const auto& [ip, entry] : member_status){
        pair<string,int64_t> new_id({ip_to_machine[ip.first], ip.second});
        tmp.push_back({new_id, entry});
    }
    member_status_lock.unlock();
    sort(tmp.begin(), tmp.end(), print_cmp);
    stringstream ss;
    ss << "New Membership List: " << endl;
    for(const auto&[id, entry] : tmp) {
        if(entry.status > 0) {
            ss << node_id_to_string(id) << " ";
        }
    }
    
    print_to_membership_log(ss.str(), true);
}

void print_detailed_list(const map<pair<string,int64_t>, MemberEntry> & other){
    stringstream ss;
    for(auto &[ip, entry] : other) {
        ss << ip.first << " " << ip.second << ": " << entry.time_stamp_ms << " " << entry.heart_beat_counter << 
            " " << entry.status << " " << entry.incarnation_count << endl;
    }
    print_to_membership_log(ss.str(), false);
}

void print_current_mode(){
    if(suspection_mode) puts("In Gossip+Suspection Mode");
    else puts("In Gossip Mode");
}
void local_mode_change(){
    suspection_mode ^= 1;
}

void init_ip_list(){
    ip_to_machine["172.22.94.78"] = "1";
    ip_to_machine["172.22.156.79"] = "2";
    ip_to_machine["172.22.158.79"] = "3";
    ip_to_machine["172.22.94.79"] = "4";
    ip_to_machine["172.22.156.80"] = "5";
    ip_to_machine["172.22.158.80"] = "6";
    ip_to_machine["172.22.94.80"] = "7";
    ip_to_machine["172.22.156.81"] = "8";
    ip_to_machine["172.22.158.81"] = "9";
    ip_to_machine["172.22.94.81"] = "10";

    machine_idx_to_ip[0] = "172.22.94.78";
    machine_idx_to_ip[1] = "172.22.156.79";
    machine_idx_to_ip[2] = "172.22.158.79";
    machine_idx_to_ip[3] = "172.22.94.79";
    machine_idx_to_ip[4] = "172.22.156.80";
    machine_idx_to_ip[5] = "172.22.158.80";
    machine_idx_to_ip[6] = "172.22.94.80";
    machine_idx_to_ip[7] = "172.22.156.81";
    machine_idx_to_ip[8] = "172.22.158.81";
    machine_idx_to_ip[9] = "172.22.94.81";
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

void message_receiver() {
    // this_thread::sleep_for(chrono::milliseconds(500));
    cout << "Receiver starts receiving data..." << endl;
    // open socket
    char buffer[max_buffer_size];

    int sockfd, clientlen;
    struct sockaddr_in serveraddr, clientaddr;
    clientlen = sizeof(clientaddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        puts("Message receiver connect fail!");
        throw runtime_error("Message receiver connect fail!");
    }
    
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))){
        puts("Reveive failure in UDP setsockopt");
        throw runtime_error("Reveive failure in UDP setsockopt");
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(port_num);

    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
        puts("Message receiver bind UDP fail!");
        throw runtime_error("Message receiver bind UDP fail!");
    }

    while(true){ 
        // receive_message
        memset(buffer, 0, sizeof(buffer));
        if(recvfrom(sockfd, (char *) buffer, max_buffer_size, 0, (struct sockaddr *) &clientaddr,
                reinterpret_cast<socklen_t *>(&clientlen)) < 0) continue;
        string msg(buffer);
        int idx = 1;
        string ip = ParseStringUntil(idx, msg, '#');
        int64_t time_stamp = 0;
        print_to_membership_log("receive: " + msg, false);
        switch(msg[0]){
            case 'J':
                print_to_membership_log("Receive join request: " + ip, false);
                response_join(msg.substr(1));
                break;
            case 'G':
                print_to_membership_log("Receive gossip request: " + ip, false);
                combine_member_entry(message_to_member_entry(msg.substr(1)));
                print_detailed_list(message_to_member_entry(msg.substr(1)));
                print_to_membership_log("================== Member Status ================== ", false);
                member_status_lock.lock();
                print_detailed_list(member_status);
                member_status_lock.unlock();
                break;
            case 'L':
                time_stamp = ParseIntUntil(idx, msg, '#');
                member_status_lock.lock();
                member_status[{ip, time_stamp}].heart_beat_counter = leave_heart_beat;
                member_status_lock.unlock();
                break;
            case 'C':
                suspection_mode ^= 1;
                puts("Receive Mode Change Message!");
                print_current_mode();
                break;
            default:
                print_to_membership_log("Unsupported Comannd!", true);
        }
        print_to_membership_log("Ready to read next message!", false);

    }
    close(sockfd);
}

map<pair<string,int64_t>, MemberEntry> message_to_member_entry(const string &str){
    map<pair<string,int64_t>, MemberEntry> res;
    int idx = 0;
    string machine_ip = ParseStringUntil(idx, str, '#');
    int64_t time_stamp = ParseIntUntil(idx, str, '#');
    int entry_cnt = ParseIntUntil(idx, str, '#');
    for(int i = 1; i <= entry_cnt; i++) {
        string ip = ParseStringUntil(idx, str, '#');
        int64_t stamp = ParseIntUntil(idx, str, '#');
        MemberEntry entry;
        entry.time_stamp_ms = ParseIntUntil(idx, str, '#');
        entry.heart_beat_counter = ParseIntUntil(idx, str, '#');
        entry.status = ParseIntUntil(idx, str, '#');
        entry.incarnation_count = ParseIntUntil(idx, str, '#');
        res[make_pair(ip, stamp)] = entry;
    }
    return res;
}

void combine_member_entry(const map<pair<string,int64_t>, MemberEntry> &other){
    int64_t current_time_ms = cur_time_in_ms();
    member_status_lock.lock();
    bool is_change = 0;
    for(const auto &[id, entry] : other){
        if(!member_status.count(id) && entry.status != 0){
            member_status[id] = entry;
            if(entry.status != 0){
                print_to_membership_log(node_id_to_string(id) + " " + ip_to_machine[id.first] + 
                                " has joined", true);
                is_change = 1;
            }

        } else{
            auto &correspond = member_status[id];
            if(correspond.status == 0){
                correspond.status = 0;// failure node should remain failure
                continue;
            }
            if(entry.status == 0) {
                if(correspond.status != 0){
                    correspond.status = 0;
                    print_to_membership_log(node_id_to_string(id) + " " + ip_to_machine[id.first] + 
                                    " has failed by other", true);
                    is_change = 1;                    
                }
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
            if(id == machine_id && correspond.status == 1) { // self is suspected
                correspond.incarnation_count++;
                correspond.status = 2;// alive
            }
        }
    }
    member_status_lock.unlock();
    if(is_change) {
        print_membership_list();
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
    print_to_membership_log("alive ip: " + to_string(alive_ip.size()), false);
    print_to_membership_log("ip from: " + to_string(choose_ip_from.size()), false);
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
        int required = heartbeat_number - (int)send_target.size();
        for(int i = 0; i < required; i++){
            send_target.push_back(alive_ip[i]);
            previous_sent.insert(alive_ip[i]);
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
    sort(send_target.begin(), send_target.end());
    send_target.resize(unique(send_target.begin(), send_target.end()) - send_target.begin());
    return send_target;
}
double random_generator(){
    int x = rand() % (1<<10);
    return (double)x/(1<<10);
}

void send_a_udp_message(string ip, string msg) {
    if(random_generator() < dropout_rate) {
        return;
    }
    int sockfd;
    struct sockaddr_in servaddr;
    struct hostent *server;

    print_to_membership_log("Send gossip to: " + ip, false);
    print_to_membership_log(msg, false);

    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        puts("Hearbeat sender create socket fail failed!");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port_num);
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    int n;
    socklen_t len;

    print_to_membership_log("start to sent to" + ip, false);
    if(sendto(sockfd, msg.c_str(), msg.size(),
                MSG_CONFIRM, (const struct sockaddr *) &servaddr, 
                    sizeof(servaddr)) < 0){
        cout << "Heartbeat sender fail to send message to "  <<  ip << endl;
    }
    close(sockfd);
    print_to_membership_log("sent to " + ip + " success!" , false); 
}

void heartbeat_sender(){
    // assume there is only one server on a single ip address
    set<string> previous_sent;
    int64_t last_update_time = cur_time_in_ms();
    while(true){
        while(cur_time_in_ms() - last_update_time < heartbeat_rate_ms)continue;
        last_update_time = cur_time_in_ms();
        //(1) randomly select, reminder: use lock
        vector<string> target_ips;
        target_ips = random_choose_send_target(previous_sent);
        print_to_membership_log("target: " + to_string( target_ips.size() ), false);
        
        // cout << "Finish choosing send target" <<endl;

        //(2) update your own member entry
        int64_t cur_time = cur_time_in_ms();
        member_status_lock.lock();
        member_status[machine_id].time_stamp_ms = cur_time;
        member_status[machine_id].heart_beat_counter = cur_time;
        member_status_lock.unlock();
        // cout << "[" <<cur_time << "]: Update myself" <<endl;

        //(3) open socket and send the message
        string msg = "G" + member_entry_to_message();
        for(int i = 0; i < target_ips.size(); i++){
           thread(send_a_udp_message, target_ips[i], msg).detach();
        }
        // this_thread::sleep_for(chrono::milliseconds(500));
    }
}

string member_entry_to_message(){
    member_status_lock.lock();
    stringstream res;
    res << machine_id.first;
    res << '#';
    res << machine_id.second;
    res << '#';
    int cnt = 0;
    for(auto id_entry:member_status) {
        if(cur_time_in_ms() - id_entry.second.time_stamp_ms <= cleanup_time_ms){
            cnt++;
        }
    }
    res << cnt;
    res << '#';
    for(auto id_entry:member_status) {
        if(cur_time_in_ms() - id_entry.second.time_stamp_ms > cleanup_time_ms)
            continue;
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
    // this_thread::sleep_for(chrono::milliseconds(500));
    int64_t last_update_time = cur_time_in_ms();
    while(true){
        int64_t current_time_ms = cur_time_in_ms();
        while(cur_time_in_ms() - last_update_time < 250)continue;
        last_update_time = current_time_ms;
        print_to_membership_log("failure detector begin", false);
        member_status_lock.lock();

        //update your own member entry
        member_status[machine_id].time_stamp_ms = current_time_ms;
        member_status[machine_id].heart_beat_counter = current_time_ms;

        bool has_failure = false;
        for(auto&[id, entry] : member_status) {
            if(id == machine_id)continue;
            auto time_stamp_ms = entry.time_stamp_ms;
            if(entry.status == 0)continue; // skip dead node
            if(entry.heart_beat_counter == leave_heart_beat) {
                //node has leave
                entry.status = 0; // 0 for failure 
                print_to_membership_log(node_id_to_string(id) + " " + ip_to_machine[id.first] + 
                                " has left", true);
                has_failure = true;
                continue;
            }
            if(suspection_mode) {
                if(current_time_ms - time_stamp_ms >= suspect_time_ms && entry.status == 2) {
                    entry.status = 1; // 1 for suspect
                    print_to_membership_log("Suspect " + node_id_to_string(id) + 
                                    " " + ip_to_machine[id.first], true);
                }
                if(current_time_ms - time_stamp_ms >= suspect_time_ms + suspect_timeout_ms) {
                    entry.status = 0; // 0 for failure
                    print_to_membership_log(node_id_to_string(id) + " " + ip_to_machine[id.first] + 
                                    " has failed suspect_timeout ", true);
                    has_failure = true;
                }
            } else{
                if(current_time_ms - time_stamp_ms >= fail_time_ms){
                    entry.status = 0; // 0 for failure
                    print_to_membership_log(node_id_to_string(id) + " " + ip_to_machine[id.first] + 
                                    " has failed timeout", true);
                    has_failure = true;
                }
            }   
        }
        member_status_lock.unlock();
        if(has_failure)print_membership_list();
        save_current_status_to_log();

        //check self is failed by others
        member_status_lock.lock();
        if(member_status[machine_id].status == 0) {
            print_to_membership_log("Kicked out by others", true);
            exit(0);
        }
        member_status_lock.unlock();
        print_to_membership_log("failure detector end", false);
        // this_thread::sleep_for(chrono::milliseconds(250));
    }
}

void join_group(){
    while(true){
        int sockfd_send;
        struct sockaddr_in servaddr;

        if ( (sockfd_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
            puts("Hearbeat sender connect failed!");
            continue;
        }

        memset(&servaddr, 0, sizeof(servaddr));
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port_num);

        servaddr.sin_addr.s_addr = inet_addr(introducer_ip_address.c_str());

        
        int n;
        socklen_t len;
        string msg = "J" + machine_id.first + "#" + to_string(machine_id.second); 
        print_to_membership_log("send join request: " + msg, false);
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
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd_recv, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))){
            puts("Reveive failure in UDP setsockopt in join_group()");
            throw runtime_error("Reveive failure in UDP setsockopt in join_group()");
        }

        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = INADDR_ANY;
        serveraddr.sin_port = htons(port_num);

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
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }
}

void response_join(const string &str){
    print_to_membership_log("reponse: " + str, false);
    int idx = 0;
    string ip = ParseStringUntil(idx, str, '#');
    int64_t time_stamp = ParseIntUntil(idx, str, '#');

    member_status_lock.lock();
    member_status[{ip, time_stamp}].time_stamp_ms = cur_time_in_ms();
    member_status[{ip, time_stamp}].heart_beat_counter = time_stamp;
    member_status_lock.unlock();
    print_to_membership_log(node_id_to_string({ip, time_stamp}) + " " + ip_to_machine[ip] + " has joined", true);
    print_membership_list();

    int sockfd_send;
    struct sockaddr_in servaddr;
    struct hostent *server;

    if ( (sockfd_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        puts("Hearbeat sender connect failed!");
        return;
    }
    
    // server = gethostbyname(ip.c_str());
    // cout << server << endl;
    // if (server == nullptr)
    //     puts("Heartbeat sender cannot get host by name!");

    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port_num);
    // bcopy((char *) server->h_addr, (char *) &servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());
    
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

vector<string> get_alive_other_ips(){
    vector<string> target_ips;
    member_status_lock.lock();
    for (auto entry : member_status){
        if(entry.second.status == 0) continue;
        if(entry.first.first == machine_id.first) continue;
        target_ips.push_back(entry.first.first);
    }
    member_status_lock.unlock();
    return target_ips;
}

void leave_group(){
    vector<string> target_ips = get_alive_other_ips();
    string msg = "L" + machine_id.first + "#" + to_string(machine_id.second) + "#"; 
    for(int i = 0; i < target_ips.size(); i++){
        send_a_udp_message(target_ips[i], msg);
    } 
    
}

void group_mode_change(){
    vector<string> target_ips = get_alive_other_ips();
    string msg = "C";
    for(int i = 0; i < target_ips.size(); i++){
        thread(send_a_udp_message, target_ips[i], msg).detach();
    } 
}

void load_introducer_from_file() {
    ifstream fin(machine_id.first + ".log");
    string str;fin>>str;
    auto res = message_to_member_entry(str);
    auto cur_time_ms = cur_time_in_ms();
    member_status_lock.lock();
    member_status = res;
    pair<string,int64_t> previous_same_machine_id;
    for (auto& [id, entry] : member_status) {
        cout << id.first << " " << id.second << endl;
        if(id.first == machine_id.first) {
            previous_same_machine_id = id;
        }
        // update all of the alived node to current timestamp
        if(entry.status > 0) {
            entry.time_stamp_ms = cur_time_ms;
        }
    }
    // insert new self and erase previous self
    member_status.emplace(machine_id, cur_time_in_ms());
    member_status.erase(previous_same_machine_id);
    member_status_lock.unlock();
    print_membership_list();
    fin.close();
}

void save_current_status_to_log() {
    ofstream fout( machine_id.first + ".log");
    fout << member_entry_to_message() << endl;
    fout.close();
}

void start_membership_service(string ip){
    srand(time(NULL));
    machine_id.first = ip;
    machine_id.second = cur_time_in_ms();
    fmembership_out.open(node_id_to_string(machine_id) + ".log");
    if(machine_id.first == introducer_ip_address){ // introducer 
        load_introducer_from_file();
    } else { // others join the group 
        join_group();
    }
    
    print_current_mode();

    cout << "Start detaching receiver, sender, detector..." << endl; 
    thread receiver(message_receiver);receiver.detach();
    thread sender(heartbeat_sender);sender.detach();
    thread detector(failure_detector);detector.detach();
    
}

// int main(int argc, char *argv[]){
//     srand(time(NULL));
//     init_ip_list();
//     if(argc != 2 && argc != 3){
// 		puts("FATAL: please assign machine number!");
// 		exit(1);
// 	}
//     if(machine_id.first == introducer_ip_address){ // introducer 
//         load_introducer_from_file();
//     } else { // others join the group 
//         join_group();
//     }
//     if(argc == 3){
//         suspection_mode = stoi(argv[2]);
//     }
//     print_current_mode();
//     string input;
//     while(cin >> input){
//         if(input == "LEAVE" || input == "L") {
//             leave_group();
//             print_to_membership_log(node_id_to_string(machine_id) + " has left", true);
//             return 0;
//         } else if(input == "CHANGE" || input == "C") {
//             suspection_mode ^= 1;
//             print_current_mode();
//             group_mode_change();
//         } else if(input == "LOCAL" || input == "LOCALC" || input == "LOCALCHANGE"){
//             suspection_mode ^= 1;
//             print_current_mode();
//         } else {
//             puts("Unsupported command, input again!");
//         }
//     }
// }
set<int> get_current_live_membership_set(){
    set<int> res;
    member_status_lock.lock();
    for(auto&[id, entry] : member_status) {
        if(entry.status == 2){
            res.insert((get_index_from_ip_address(id.first)));
        }
    }
    member_status_lock.unlock();
    return res;
}

int get_index_from_ip_address(const string &str){
    return stoi(ip_to_machine[str]) - 1;
}

string get_ip_address_from_index(int index){
    return machine_idx_to_ip[index];
}
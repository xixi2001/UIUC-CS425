#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
using namespace std;
constexpr int grep_key_length = 20;
constexpr int line_max_length =  1024;
constexpr int traget_total_length = 1<<15;
constexpr int random_key = 19260817;
inline char RandChar(){
    if(rand() & 1)
        return rand() % 26 + 'a';
    return rand() % 26 + 'A';
}
int f[line_max_length];char s[line_max_length], n;
void GetFail(){ // Use KMP to server Grep
    n = strlen(s);
	f[0] = f[1] = 0;
	for(int i=1;i<n;i++){
		int j = f[i];
		while(s[j]!=s[i]&&j)j=f[j];
		f[i+1] = s[j] == s[i]? j+1:0;
	}
}
char t[line_max_length];
bool IsMacth(){
    int m = strlen(t), j = 0;
    for(int i=0;i<m;i++){
        while(j && s[j] != t[i])j = f[j];
        j = s[j] == t[i] ? j+1:0;
        if(j == n) return 1;
    }
    return 0;
}
int RandomStringGenerator(){
    memset(t,0,sizeof(t));
    int length = rand() % line_max_length;
    for(int i=0; i<length;i++) t[i] = RandChar();
    t[length] = 'a';
    return length + 1;
}
int TargetStringGenerator(){
    memset(t,0,sizeof(t));
    int prefix = rand() % 500,suffix = rand() % 500;
    for(int i=0;i<prefix;i++)t[i] = RandChar();
    for(int i=0;i<n;i++)t[i+prefix] = s[i];
    for(int i=0;i<suffix;i++)t[i+prefix+n]  = RandChar();
    t[prefix + n + suffix] = 'a';
    return prefix + n + suffix + 1;

}
void GenerateGrepKey(){
    for(int i=0;i<grep_key_length;i++)s[i] = RandChar();
    GetFail();
}
int total_line_cnt = 0;
int main(int argc, char *argv[]){
    srand(random_key);// fixed random key
    // Generate grep key
    if(argc == 1){
        // use random patter as default pattern
        for(int i=0;i<grep_key_length;i++)s[i] = RandChar(); // default rand
    }else{
        if(argv[1] == "frequent"){
            sprintf(s, "the");
        } else if (argv[1] == "medium"){
            sprintf(s, "ba");
        } else if (argv[1] == "all"){
            sprintf(s, "a");
        } else{// default as rare
            for(int i=0;i<grep_key_length;i++)s[i] = RandChar();
        }
    }
    GetFail();
    ofstream test_input("test_input");
    test_input << s << endl;
    test_input.close();
    ofstream test_result("test.result");
    //Generate input file and output file
    for(int i = 1;i <= 10;i++){
        test_result << "==================== grep result from machine " << i << " ====================" << endl;
        ofstream log_output("machine." + to_string(i) + ".log");
        int budget = traget_total_length;
        for(int j = 1; j <= 2 * traget_total_length / line_max_length; j++){
            if(budget <= 0)break;
            if(rand() % 100 == 0){
                budget -= TargetStringGenerator();
            } else {
                budget -= RandomStringGenerator();
            }
            log_output << t << endl;
            if(IsMacth()){
                test_result << t << endl;
                total_line_cnt++;   
            }
        }
        log_output.close();
    }
    test_result << "total "  << total_line_cnt << " lines grep." << endl; 
    test_result.close();
    
}
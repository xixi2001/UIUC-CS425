#include<fstream>
#include<iostream>
#include<vector>
#include<sstream>
#include<map>
using namespace std;

vector<string> tokenize(string input, char delimeter){
    istringstream ss(input);
    string token;
    vector<string> ret;
    while(getline(ss, token, delimeter)){
        ret.push_back(token);
    }
    return ret;
}

int main(int argc, char *argv[]){
    int total = 0;
    map<string,int> cnt;
    string line;
    while (getline(cin, line)){
        vector<string> p = tokenize(line, ' ');
        cnt[p[1]]++;
        total++;
    }
    for(const auto &[key, count] : cnt) {
        cout << key << " ";
        printf("%.2f%\n", 100.0 * count/total);       
    }
    return 0;
}
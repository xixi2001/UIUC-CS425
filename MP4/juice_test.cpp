#include<fstream>
#include<iostream>
#include<vector>
#include<sstream>
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
    
    string line;
    string key;
    int count = 0;
    while (getline(cin, line)){
        vector<string> p = tokenize(line, ' ');
        key = p[0];
        count++;
    }

    if(count != 0) cout << key << " " << count << endl;

    return 0;
}
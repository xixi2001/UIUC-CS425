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
    int count = 0, total = 0;
    while (getline(cin, line)){
        vector<string> p = tokenize(line, ' ');
        if(p[1][0] == '1'){
            count++;
        }
        total++;
        key = p[0];
    }
    
    cout << key << " ";
    printf("%.2f%\n", 100.0 * count/total);

    return 0;
}
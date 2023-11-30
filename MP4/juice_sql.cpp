#include<fstream>
#include<iostream>
#include<vector>
#include<sstream>
using namespace std;

int main(int argc, char *argv[]){
    string line;
    while (getline(cin, line)){
        if(line.length() >= 9){
            cout << line.substr(9) << endl;            
        }
    }
}
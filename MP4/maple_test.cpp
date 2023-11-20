#include<fstream>
#include<iostream>
using namespace std;

int main(int argc, char *argv[]){
    ifstream input(argv[1]);
    
    string token;
    while (input >> token) {
        cout << token << " " << 1 << endl;
    }

    return 0;
}
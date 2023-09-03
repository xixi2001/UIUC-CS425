#include <iostream>
#include <array>
#include <memory>
#include <vector>
using namespace std;
vector<string> exec(const char* cmd) {
    // array<char, 10000> buffer;
    char buffer[10000];
    vector<string> result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, 10000, pipe.get()) != nullptr) {
        result.push_back(buffer);
    }
    return result;
}
int main(){
    auto res = exec("grep int client.cpp");
    cout << "res: " << res.size() << endl;
}
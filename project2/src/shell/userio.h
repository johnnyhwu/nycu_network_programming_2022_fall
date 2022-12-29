#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace std;

class UserIO
{
    private:
        string raw_cmd;

    public: 
        UserIO(bool init_env);
        void output(string msg);
        void outerr(string msg);
        vector<string> input();
        void err_sys(string msg);
        vector<string> string_split(string s, string delimiter);
        string get_raw_cmd();
};

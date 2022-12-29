#pragma once

#include <vector>
#include <string>
#include <cstdlib>
#include <regex>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include "userio.h"

using namespace std;

class CMDer
{
    private:
        UserIO user_io = UserIO(false);

        // each cmd has its own id
        int cmd_uid;
        
        // each cmd-line has its own id
        int cmdline_uid;

        // cmd's global id => pipe's read end
        // cmd creates pipe
        map<int, int> cmd_pipe_table;

        // A cmd's global id => B cmd's global id
        // A cmd should read from pipe created by B cmd
        map<int, int> cmd_r_pipe_table;

        // cmdline uid => cmd uid
        // the first cmd at cmdline uid should read from pipe created by cmd uid
        map<int, int> cmdline_r_pipe_table;

    public:
        CMDer();
        int exec(vector<string> cmd_vec);
        
        bool is_simple_cmd(vector<string> cmd_vec);
        void exec_built_in(vector<string> cmd_vec);
        
        string get_cmd_full_path(vector<string> cmd_vec);
        static void sigchld_handler(int signo);
        void exec_bin(
            vector<string> cmd_vec, 
            bool explicit_wait,
            bool pipe_cmd,
            int fd_in,
            int fd_out,
            int fd_err,
            bool pipe_conflict,
            string pipe_conflict_content
        );

        void parse_pipe_cmd(
            vector<string> cmd_vec, 
            vector< vector<string> > & cmds_vec, 
            vector<int> & pipe_num,
            vector<int> & pipe_type
        );
};

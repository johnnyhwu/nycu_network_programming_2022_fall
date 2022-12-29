#pragma once

#include <vector>
#include <utility>
#include <string>
#include <cstdlib>
#include <regex>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <map>
#include "userio.h"
#include "../shm.h"

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

        // raw command string
        string raw_cmd;

        /*
            server type:
            (1) concurrent_server: multi-process, single client server
            (2) single_process_server: single process, multi client server
            (3) multi_process_Server: multi-process, multi client server
        */
        string server_type;

        int socket_fd;
        map<int, pair<string, int> >* client_ip_port_table;
        map<int, string>* client_name_table;
        map<int, int>* client_id_table;
        map<pair<int, int>, int>* client_user_pipe_table;
        map<int, map<string, string> >* client_env_table;

        ShM* client_shmer;

    public:
        CMDer();
        int exec(vector<string> cmd_vec, string raw_cmd);
        
        bool is_simple_cmd(vector<string> cmd_vec);
        
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

        void set_client_table(
            map<int, pair<string, int> >* ip_port_table,
            map<int, string>* name_table,
            map<int, int>* id_table,
            map<pair<int, int>, int>* user_pipe_table,
            map<int, map<string, string> >* env_table
        );
        void set_client_shmer(ShM* shmer);
        void set_socket_fd(int fd);
        void set_server_type(string type_name);

        void broadcast_msg(string msg);
        bool contain_user_pipe(vector<string> cmd_vec);
        vector<string> parse_user_pipe(vector<string> cmd_vec, map<string, int>* user_pipe_info);
};

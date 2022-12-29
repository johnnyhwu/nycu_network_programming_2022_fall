#pragma once

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <string.h>
#include <signal.h>
#include <map>

using namespace std;

typedef struct {    
    char ip[100][100];
    int port[100];
    
    char name[100][100];
    int id[100];

    int user_pipe_idx; // trg_user will look at this idx when getting signal from src_user
    int user_pipe[500][4]; // src_user id, trg_user id, src_user pipe write end, trg_user pipe read end

    char env[100][100]; // ex. PATH=bin:. VAR1=apple

    char message[1000];
} shm_struct;

class ShM
{
    private:
        int client_pid;
        int client_id;

    public: 
        ShM();
        void create(bool new_one);
        void detach();
        void remove();
        
        int shm_key;
        int shm_id;
        shm_struct* shm_struct_ptr;

        /**********************************************************************************************/
        /*                                            set                                             */
        /**********************************************************************************************/
        void set_id(int socket_fd_fake);
        void set_ip(string ip);
        void set_port(int port);
        void set_name(string name);
        void set_env(string key, string value);
        void set_message(string msg);
        int set_user_pipe(int src_id, int trg_id, string purpose);

        /**********************************************************************************************/
        /*                                          remove                                            */
        /**********************************************************************************************/
        void remove_id();
        void remove_user_pipe(int src_id, int trg_id);

        /**********************************************************************************************/
        /*                                            get                                             */
        /**********************************************************************************************/
        int get_id();
        vector<int> get_all_id();
        bool is_id_exist(int trg_id);

        string get_ip();
        string get_ip(int trg_id);
        int get_port();
        int get_port(int trg_id);

        string get_name();
        string get_name(int trg_id);
        vector<string> get_all_name();
        bool is_name_exist(string trg_name);

        string get_message();

        string get_env();
        string get_env_value(string key);
        map<string, string> get_env_table();

        bool is_user_pipe_exist(int src_id, int trg_id);
        int get_user_pipe_fd(int src_id, int trg_id);

        void signal_user(int user_id);
};

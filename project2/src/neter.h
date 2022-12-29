#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <strings.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <iostream>
#include <utility>
#include <map>

#include <filesystem>

#include "shell/shell.h"
#include "shm.h"

using namespace std;

class NETer
{
    public: 
        NETer();
        void concurrent_server(string service);
        void single_proc_server(string service);
        void multi_proc_server(string service);

        int create_socket(string service);

        Shell* init_shell(
            string server_type, 
            int socket_fd,

            /**********************************************************************************************/
            /*                           server 2 (single process tcp server)                             */
            /**********************************************************************************************/
            map<int, pair<string, int> >* client_ip_port_table,
            map<int, string>* client_name_table,
            map<int, int>* client_id_table,
            map<pair<int, int>, int>* client_user_pipe_table,
            map<int, map<string, string> >* client_env_table,

            /**********************************************************************************************/
            /*                           server 3 (multi-process tcp server)                              */
            /**********************************************************************************************/
            ShM* client_shmer
        );
        static void sigchld_handler(int signo);

        /**********************************************************************************************/
        /*                           server 3 (multi-process tcp server)                              */
        /**********************************************************************************************/
        static void sigint_handler(int signo);
        static void sigusr1_handler(int signo);
        static void sigusr2_handler(int signo);
        static ShM shmer;
};
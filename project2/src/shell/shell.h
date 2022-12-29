#pragma once

#include <unistd.h>
#include <vector>
#include <string>
#include "userio.h"
#include "cmder.h"
#include "../shm.h"

using namespace std;

class Shell
{
    private:
        UserIO* user_io;
        CMDer* cmder;

        /**********************************************************************************************/
        /*                           server 2 (single process tcp server)                             */
        /**********************************************************************************************/
        map<int, pair<string, int> >* client_ip_port_table;
        map<int, string>* client_name_table;
        map<int, int>* client_id_table;
        map<pair<int, int>, int>* client_user_pipe_table;
        map<int, map<string, string> >* client_env_table;

        /**********************************************************************************************/
        /*                           server 3 (multi-process tcp server)                              */
        /**********************************************************************************************/
        ShM* client_shmer;

    public:
        Shell();
        void activate(
            string server_type, 
            int socket_fd,

            map<int, pair<string, int> >* ip_port_table,
            map<int, string>* name_table,
            map<int, int>* id_table,
            map<pair<int, int>, int>* user_pipe_table,
            map<int, map<string, string> >* env_table,

            ShM* shmer
        );
        void activate_cc(int socket_fd);
        int activate_sp(int socket_fd);
        void activate_mp(int socket_fd);

        void broadcast_msg(string msg);
};
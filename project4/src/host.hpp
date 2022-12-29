#pragma once

#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <string.h>

using boost::asio::ip::tcp;
using namespace std;

class host_session : public std::enable_shared_from_this<host_session>
{
    public:
        tcp::socket client_socket_;
        tcp::socket host_socket_;
        tcp::endpoint endpoint_;
        enum { max_length = 1000000 };
        unsigned char client_data_[max_length];
        unsigned char host_data_[max_length];

        host_session(tcp::socket client_socket, tcp::socket host_socket, tcp::endpoint endpoint);
        void start();
        void connect_host();
        void reply_client();

        void read_from_client();
        void write_to_client(int length);
        void read_from_host();
        void write_to_host(int length);
        
        void clear_client_data();
        void clear_host_data();

};

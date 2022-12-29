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

class bind_session : public std::enable_shared_from_this<bind_session>
{
    public:
        tcp::socket client_socket_;
        tcp::acceptor acceptor_;
        enum { max_length = 1000000 };
        unsigned char client_data_[max_length];
        unsigned char host_data_[max_length];
        unsigned short port_;

        bind_session(tcp::socket client_socket, boost::asio::io_context& io_context);
        void start();
        void reply_client();
        void do_accept();

        void clear_client_data();
        void clear_host_data();
};

class bind_session_final : public std::enable_shared_from_this<bind_session_final>
{
    public:
        tcp::socket client_socket_;
        tcp::socket host_socket_;
        enum { max_length = 1000000 };
        unsigned char client_data_[max_length];
        unsigned char host_data_[max_length];
        unsigned short port_;

        bind_session_final(tcp::socket client_socket, tcp::socket host_socket, unsigned short port);
        void start();
        void reply_client();

        void read_from_client();
        void write_to_client(int length);
        void read_from_host();
        void write_to_host(int length);

        void clear_client_data();
        void clear_host_data();
};
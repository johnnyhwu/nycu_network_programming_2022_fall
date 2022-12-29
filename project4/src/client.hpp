#pragma once

#include <cstdlib>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include "host.hpp"

using boost::asio::ip::tcp;
using namespace std;

class client_session : public std::enable_shared_from_this<client_session>
{
    public:
        tcp::socket socket_;
        boost::asio::io_context& io_context_;
        enum { max_length = 1024 };
        unsigned char data_[max_length];

        string version;
        string command;
        string dest_port;
        string dest_ip;
        string domain;

        client_session(tcp::socket socket, boost::asio::io_context& io_context);
        void start();
        void parse_request();
        void exec_request(string command, string dest_port, string dest_ip, string domain);
        bool firewall_validate(char ops, string dest_ip);
        vector<string> split_string(string text, string key);
        void reject_client();
};

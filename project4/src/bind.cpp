#include "bind.hpp"

bind_session::bind_session(tcp::socket client_socket, boost::asio::io_context& io_context)
    : client_socket_(move(client_socket)), acceptor_(io_context, tcp::endpoint(tcp::v4(), 0))
{
    // cout << "[info, socks_server (bind session)] session is created" << endl;

    clear_client_data();
    clear_host_data();
}

void bind_session::start()
{
    reply_client();
}

void bind_session::reply_client()
{
    // cout << "[info, socks_server (bind session)] reply client" << endl;

    auto self(shared_from_this());
    port_ = acceptor_.local_endpoint().port();
    unsigned char port_low = port_ % 256;
    unsigned char port_high = port_ >> 8;
    unsigned char msg[8] = {0, 90, port_high, port_low, 0, 0, 0, 0};
    memcpy(client_data_, msg, 8);

    boost::asio::async_write(
        client_socket_, boost::asio::buffer(client_data_, 8),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                do_accept();
            }
        }
    );
}

void bind_session::do_accept()
{
    // cout << "[info, socks_server (bind session)] wait connection" << endl;

    auto self(shared_from_this());
    acceptor_.async_accept(
        [this, self](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                make_shared<bind_session_final>(move(client_socket_), move(socket), port_)->start();
                acceptor_.close();
            }
        }
    );
}

void bind_session::clear_client_data()
{
    memset(client_data_, 0x00, max_length);
}

void bind_session::clear_host_data()
{
    memset(host_data_, 0x00, max_length);
}


/***************************************************************************************************************************/


bind_session_final::bind_session_final(tcp::socket client_socket, tcp::socket host_socket, unsigned short port)
    : client_socket_(move(client_socket)), host_socket_(move(host_socket)), port_(port)
{
    // cout << "[info, socks_server (bind session final)] session is created" << endl;

    clear_client_data();
    clear_host_data();
}

void bind_session_final::start()
{
    reply_client();
}

void bind_session_final::reply_client() {
    auto self(shared_from_this());
    unsigned char port_low = port_ % 256;
    unsigned char port_high = port_ >> 8;
    unsigned char msg[8] = {0, 90, port_high, port_low, 0, 0, 0, 0};
    memcpy(client_data_, msg, 8);
    boost::asio::async_write(
        client_socket_, boost::asio::buffer(client_data_, 8),
        [this, self](boost::system::error_code ec, size_t /*length*/) {
            if (!ec) {
                read_from_client();
                read_from_host();
            }
        }
    );
}

void bind_session_final::read_from_client() {
    auto self(shared_from_this());
    client_socket_.async_read_some(
        boost::asio::buffer(client_data_, max_length),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                write_to_host(length);
            }
        }
    );
}

void bind_session_final::read_from_host() {
    auto self(shared_from_this());
    host_socket_.async_read_some(
        boost::asio::buffer(host_data_, max_length),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                write_to_client(length);
            }
        }
    );
}

void bind_session_final::write_to_host(int length) {
    auto self(shared_from_this());
    boost::asio::async_write(
        host_socket_, boost::asio::buffer(client_data_, length),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                read_from_client();
            }
        }
    );
}

void bind_session_final::write_to_client(int length) {
    auto self(shared_from_this());
    boost::asio::async_write(
        client_socket_, boost::asio::buffer(host_data_, length),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                read_from_host();
            }
        }
    );
}

void bind_session_final::clear_client_data()
{
    memset(client_data_, 0x00, max_length);
}

void bind_session_final::clear_host_data()
{
    memset(host_data_, 0x00, max_length);
}
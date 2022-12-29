#include "host.hpp"

host_session::host_session(tcp::socket client_socket, tcp::socket host_socket, tcp::endpoint endpoint)
    : client_socket_(move(client_socket)), host_socket_(move(host_socket)), endpoint_(endpoint)
{
    // cout << "[info, socks_server (host session)] session is created" << endl;

    clear_client_data();
    clear_host_data();
}

void host_session::start()
{
    connect_host();
}

void host_session::connect_host()
{
    // cout << "[info, socks_server (host session)] connect to host" << endl;

    auto self(shared_from_this());
    host_socket_.async_connect(
        endpoint_,
        [this, self](const boost::system::error_code& ec){
            if(!ec) {
                reply_client();
            }
        }
    );
}

void host_session::reply_client()
{
    // cout << "[info, socks_server (host session)] reply client" << endl;

    auto self(shared_from_this());
    unsigned char msg[8] = {0, 90, 0, 0, 0, 0, 0, 0};
    clear_client_data();
    memcpy(client_data_, msg, 8);

    boost::asio::async_write(
        client_socket_, boost::asio::buffer(client_data_, 8),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                read_from_client();
                read_from_host();
            }
        }
    );
}

void host_session::read_from_client()
{
    // cout << "[info, socks_server (host session)] read from client" << endl;

    auto self(shared_from_this());
    client_socket_.async_read_some(
        boost::asio::buffer(client_data_, max_length),
        [this, self](boost::system::error_code ec, size_t length) {
            // cout << "[info, socks_server (host session)] read from client (callback)" << endl;
            if (!ec) {
                write_to_host(length);
            }
        }
    );

}

void host_session::write_to_client(int length)
{
    // cout << "[info, socks_server (host session)] write to client" << endl;

    auto self(shared_from_this());
    boost::asio::async_write(
        client_socket_, boost::asio::buffer(host_data_, length),
        [this, self](boost::system::error_code ec, size_t length) {
            // cout << "[info, socks_server (host session)] write to client (callback)" << endl;
            if (!ec) {
                read_from_host();
            }
        }
    );
}

void host_session::read_from_host()
{
    // cout << "[info, socks_server (host session)] read from host" << endl;

    auto self(shared_from_this());
    host_socket_.async_read_some(
        boost::asio::buffer(host_data_, max_length),
        [this, self](boost::system::error_code ec, size_t length) {
            // cout << "[info, socks_server (host session)] read from host (callback)" << endl;
            if (!ec) {
                write_to_client(length);
            }
        }
    );
}

void host_session::write_to_host(int length)
{
    // cout << "[info, socks_server (host session)] write to host" << endl;

    auto self(shared_from_this());
    boost::asio::async_write(
        host_socket_, boost::asio::buffer(client_data_, length),
        [this, self](boost::system::error_code ec, size_t length) {
            // cout << "[info, socks_server (host session)] write to host (callback)" << endl;
            if (!ec) {
                read_from_client();
            }
        }
    );
}

void host_session::clear_client_data()
{
    memset(client_data_, 0x00, max_length);
}

void host_session::clear_host_data()
{
    memset(host_data_, 0x00, max_length);
}
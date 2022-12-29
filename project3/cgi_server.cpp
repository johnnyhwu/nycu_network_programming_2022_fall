#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <regex>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

class session_host : public std::enable_shared_from_this<session_host>
{
    public:
        tcp::socket socket_;
        shared_ptr<tcp::socket> socket_client_;
		tcp::endpoint endpoint_;
		int session_idx_;
		string target_host_;
		string target_port_;
		string target_file_;
		vector<string> file_content_;
		int file_content_idx_;
        
		enum { max_length =  1000000};
        char data_[max_length];

        session_host(
			tcp::socket socket, 
            shared_ptr<tcp::socket> socket_client,
			tcp::endpoint endpoint,
			int session_idx,
			string target_host,
			string target_port,
			string target_file
		) : socket_(move(socket)),
            socket_client_(socket_client), 
			endpoint_(endpoint),
			session_idx_(session_idx),
			target_host_(target_host),
			target_port_(target_port),
			target_file_(target_file),
			file_content_idx_(0)
        {
            cerr << "[info, console.cgi] session is created" << endl;
        }

        void start()
        {
            cerr << "[info, console.cgi] session starts" << endl;

			// read file content
			read_file();
			cerr << "[info, console.cgi] read file: " << target_file_ << " (#lines: " << file_content_.size() << ") " << endl;

            connect_to_server();
        }

        void connect_to_server()
        {
            auto self(shared_from_this());
            socket_.async_connect(
                endpoint_,
                [this, self](boost::system::error_code ec)
				{
					if (!ec) {
						cerr << "[info, console.cgi] connects to server" << endl;
						
						// write to client: update session's address and port
						string text = "<script>document.querySelector('body > div:nth-child(" + to_string(session_idx_) + ")').children[1].children[0].innerHTML += '" + target_host_ + "'</script>";
						write_to_client(text);

                        text = "<script>document.querySelector('body > div:nth-child(" + to_string(session_idx_) + ")').children[1].children[1].innerHTML += '" + target_port_ + "'</script>";
                        write_to_client(text);

						// write to client: clean console
						text = "<script>document.querySelector('body > div:nth-child(" + to_string(session_idx_) + ")').children[2].innerHTML = ''</script>";
                        write_to_client(text);
						
						// read from host
						read_from_host();
					}
				}
            );
        }

		void read_from_host()
		{
			auto self(shared_from_this());
			clear_buffer();
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
				[this, self](boost::system::error_code ec, std::size_t length)
				{
					if (!ec) {
						string host_output = data_;
						cerr << "[info, console.cgi] host output: " << endl;
						cerr << host_output << endl;

						// escape special character
						host_output = escape_character(host_output);

						// write to client: write the output of host to client
						string text = "<script>document.querySelector('body > div:nth-child(" + to_string(session_idx_) + ")').children[2].innerHTML += `" + host_output + "`</script>";
                        write_to_client(text);

						// find if '%' in host_output
						if(host_output.find("%") != std::string::npos) {
							write_to_host();
						}
						else{
							clear_buffer();
							read_from_host();
						}
					}
				}
			);
		}

		void write_to_host()
		{
			auto self(shared_from_this());
			string cmd = file_content_[file_content_idx_];
			file_content_idx_++;

			// write to client: write the input of host to client
			string text = "<script>document.querySelector('body > div:nth-child(" + to_string(session_idx_) + ")').children[2].innerHTML += `<pre class=\"input-cmd\">" + cmd + "</pre>`</script>";
            write_to_client(text);

			boost::asio::async_write(socket_, boost::asio::buffer(cmd.c_str(), cmd.length()),
				[this, self](boost::system::error_code ec, std::size_t /*length*/)
				{
					if (!ec) {
						read_from_host();
					}
				}
			);
		}

        void write_to_client(string text)
		{
			auto self(shared_from_this());

			boost::asio::async_write(*socket_client_, boost::asio::buffer(text.c_str(), text.length()),
				[this, self, text](boost::system::error_code ec, std::size_t /*length*/)
				{
                    if(ec) {
                        write_to_client(text);
                    }

				}
			);
		}

		void clear_buffer()
		{
			for(size_t i=0; i<max_length; i++) {
				data_[i] = '\0';
			}
		}

		void read_file()
		{
			string text;
			string filename = "test_case/" + target_file_;
			ifstream fs(filename);

			while (getline(fs, text)) {
				file_content_.push_back(text + "\n");
			}

			fs.close(); 
		}

		string escape_character(string text)
		{
			text = regex_replace(text, regex(R"(&)"), "&amp;");
			text = regex_replace(text, regex(R"(>)"), "&gt;");
			text = regex_replace(text, regex(R"(<)"), "&lt;");
			text = regex_replace(text, regex(R"(")"), "&quot;");
			text = regex_replace(text, regex(R"(')"), "&apos;");
			return text;
		}
};

class session : public std::enable_shared_from_this<session>
{
    public:
        tcp::socket socket_;
        enum { max_length = 1000000 };
        char data_[max_length];
        map<string, string> env_table_;

        session(tcp::socket socket) : socket_(std::move(socket))
        {
            cout << "[info, cgi_server] session is created" << endl;
            cout << "[info, cgi_server] remote address: " << socket_.remote_endpoint().address().to_string() << endl;
            cout << "[info, cgi_server] remote port: " << to_string(socket_.remote_endpoint().port()) << endl;
        }

        void start()
        {
            cout << "[info, cgi_server] session starts" << endl;
            parse_request();
        }

        void parse_request()
        {
            auto self(shared_from_this());
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    cout << "[info, cgi_server] receive request:" << endl;
                    string request_header = data_;
                    cout << request_header << endl;
                    
                    if(request_header.length() > 20) {

                        cout << "[info, cgi_server] parse request" << endl;

                        // split request header into sentences
                        vector<string> sentence_vec = split_string(request_header, "\r\n");
                        string program_path;

                        // save environment variable
                        env_table_["REQUEST_METHOD"] = split_string(sentence_vec[0], " ")[0];
                        if(env_table_["REQUEST_METHOD"] != "GET") {
                            cout << "[info, http_server] invalid request method" << endl;
                            return;
                        }

                        env_table_["REQUEST_URI"] = split_string(sentence_vec[0], " ")[1];
                        if(env_table_["REQUEST_URI"].find("?") != string::npos) {
                            env_table_["QUERY_STRING"] = env_table_["REQUEST_URI"].substr(env_table_["REQUEST_URI"].find("?") + 1);
                            program_path = env_table_["REQUEST_URI"].substr(0, env_table_["REQUEST_URI"].find("?"));
                        } else {
                            env_table_["QUERY_STRING"] = "";
                            program_path = env_table_["REQUEST_URI"];
                        }
                        env_table_["SERVER_PROTOCOL"] = split_string(sentence_vec[0], " ")[2];
                        env_table_["HTTP_HOST"] = split_string(sentence_vec[1], " ")[1];
                        env_table_["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
                        env_table_["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
                        env_table_["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
                        env_table_["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());
                        program_path = program_path.substr(1);

                        // show request header info
                        cout << "[info, cgi_server] request info:" << endl;
                        for(map<string, string>::iterator iter=env_table_.begin(); iter!=env_table_.end(); iter++) {
                            cout << iter->first << " : " << iter->second << endl;
                        }

                        if(program_path == "panel.cgi" || program_path == "console.cgi") {
                            process_request(program_path);
                        } else {
                            cout << "[info, cgi_server] invalid program path: " << program_path << endl;
                        }
                        
                    } else {
                        cout << "[info, cgi_server] ignore request" << endl;
                    }
                }
            );
        }

        void process_request(string program_path)
        {
            auto self(shared_from_this());

            cout << "[info, cgi_server] process request: " << program_path << endl;

            string response = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";
            strcpy(data_, response.c_str());

            boost::asio::async_write(
                socket_, 
                boost::asio::buffer(data_, strlen(data_)),
                [this, self, program_path](boost::system::error_code ec, std::size_t /*length*/)
                {

                    // panel.cgi
                    if(program_path == "panel.cgi") {
                        exec_panel_cgi();
                    }

                    // console.cgi
                    else {
                        exec_console_cgi();
                    }
                }
            );
        }

        vector<string> split_string(string text, string key)
        {
            size_t pos = 0;
            string part;
            vector<string> result;

            while ((pos = text.find(key)) != string::npos) {
                part = text.substr(0, pos);
                result.push_back(part);
                text.erase(0, pos + key.length());
            }

            if(text.length() > 0) {
                result.push_back(text);
            }

            return result;
        }

        void exec_panel_cgi()
        {
            auto self(shared_from_this());

            string html_content = R"(
                <html lang="en">
                <head>
                    <title>NP Project 3 Panel</title>
                    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css" integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2" crossorigin="anonymous">
                    <link href="https://fonts.googleapis.com/css?family=Source+Code+Pro" rel="stylesheet">
                    <link rel="icon" type="image/png" href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png">
                    <style>
                    * {
                        font-family: 'Source Code Pro', monospace;
                    }
                    </style>
                </head>
                <body class="bg-secondary pt-5">
                    <form action="console.cgi" method="GET">
                    <table class="table mx-auto bg-light" style="width: inherit">
                        <thead class="thead-dark">
                        <tr>
                            <th scope="col">#</th>
                            <th scope="col">Host</th>
                            <th scope="col">Port</th>
                            <th scope="col">Input File</th>
                        </tr>
                        </thead>
                        <tbody>
                        <tr>
                            <th scope="row" class="align-middle">Session 1</th>
                            <td>
                            <div class="input-group">
                                <select name="h0" class="custom-select">
                                <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                                </select>
                                <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                                </div>
                            </div>
                            </td>
                            <td>
                            <input name="p0" type="text" class="form-control" size="5">
                            </td>
                            <td>
                            <select name="f0" class="custom-select">
                                <option></option>
                                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                            </select>
                            </td>
                        </tr>
                        <tr>
                            <th scope="row" class="align-middle">Session 2</th>
                            <td>
                            <div class="input-group">
                                <select name="h1" class="custom-select">
                                <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                                </select>
                                <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                                </div>
                            </div>
                            </td>
                            <td>
                            <input name="p1" type="text" class="form-control" size="5">
                            </td>
                            <td>
                            <select name="f1" class="custom-select">
                                <option></option>
                                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                            </select>
                            </td>
                        </tr>
                        <tr>
                            <th scope="row" class="align-middle">Session 3</th>
                            <td>
                            <div class="input-group">
                                <select name="h2" class="custom-select">
                                <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                                </select>
                                <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                                </div>
                            </div>
                            </td>
                            <td>
                            <input name="p2" type="text" class="form-control" size="5">
                            </td>
                            <td>
                            <select name="f2" class="custom-select">
                                <option></option>
                                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                            </select>
                            </td>
                        </tr>
                        <tr>
                            <th scope="row" class="align-middle">Session 4</th>
                            <td>
                            <div class="input-group">
                                <select name="h3" class="custom-select">
                                <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                                </select>
                                <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                                </div>
                            </div>
                            </td>
                            <td>
                            <input name="p3" type="text" class="form-control" size="5">
                            </td>
                            <td>
                            <select name="f3" class="custom-select">
                                <option></option>
                                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                            </select>
                            </td>
                        </tr>
                        <tr>
                            <th scope="row" class="align-middle">Session 5</th>
                            <td>
                            <div class="input-group">
                                <select name="h4" class="custom-select">
                                <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                                </select>
                                <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                                </div>
                            </div>
                            </td>
                            <td>
                            <input name="p4" type="text" class="form-control" size="5">
                            </td>
                            <td>
                            <select name="f4" class="custom-select">
                                <option></option>
                                <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                            </select>
                            </td>
                        </tr>
                        <tr>
                            <td colspan="3"></td>
                            <td>
                            <button type="submit" class="btn btn-info btn-block">Run</button>
                            </td>
                        </tr>
                        </tbody>
                    </table>
                    </form>
                
                </body>
                </html>
            )";

            strcpy(data_, html_content.c_str());

            boost::asio::async_write(
                socket_, 
                boost::asio::buffer(data_, strlen(data_)),
                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                    ;
                }
            );
        }

        void exec_console_cgi()
        {
            auto self(shared_from_this());

            string html_content = R"(
                <!DOCTYPE html>
                <html lang="en">
                    <head>
                        <title>NP Project 3 Console</title>
                        <link
                        href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
                        rel="stylesheet"
                        />
                        <link
                        rel="icon"
                        type="image/png"
                        href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
                        />
                        <style>
                        * {
                        font-family: 'Source Code Pro', monospace;
                        }
                        </style>
                    </head>
                    <body>
                        <div class="console">
                            <div class="title">Session 1</div>
                            <div class="remote">
                                <div class="info">ADDR: </div>
                                <div class="info">PORT: </div>
                            </div>
                            <pre class="content">

                            </pre>
                        </div>

                        <div class="console">
                            <div class="title">Session 2</div>
                            <div class="remote">
                                <div class="info">ADDR: </div>
                                <div class="info">PORT: </div>
                            </div>
                            <pre class="content">

                            </pre>
                        </div>

                        <div class="console">
                            <div class="title">Session 3</div>
                            <div class="remote">
                                <div class="info">ADDR: </div>
                                <div class="info">PORT: </div>
                            </div>
                            <pre class="content">

                            </pre>
                        </div>

                        <div class="console">
                            <div class="title">Session 4</div>
                            <div class="remote">
                                <div class="info">ADDR: </div>
                                <div class="info">PORT: </div>
                            </div>
                            <pre class="content">

                            </pre>
                        </div>

                        <div class="console">
                            <div class="title">Session 5</div>
                            <div class="remote">
                                <div class="info">ADDR: </div>
                                <div class="info">PORT: </div>
                            </div>
                            <pre class="content">

                            </pre>
                        </div>
                    </body>
                </html>

                <style>
                    body {
                        width: 200vw;

                        margin: 0;
                        padding: 0;
                        box-sizing: border-box;

                        display: flex;
                        flex-direction: row;
                        justify-content: flex-start;
                        align-items: flex-start;

                        background-color: rgb(30, 30, 30);
                    }

                    div.console {
                        height: 100vh;

                        border-color: white;
                        border-style: solid;
                        border-width: 2px;
                        border-radius: 20px;

                        display: flex;
                        flex-direction: column;
                        justify-content: flex-start;
                        align-items: center;
                    }

                        div.title {
                            width: 100%;
                            box-sizing: border-box;
                            padding: 10px 0px;

                            color: white;
                            font-size: 1.5rem;
                            
                            display: flex;
                            flex-direction: row;
                            justify-content: center;
                            align-items: center;

                            border-bottom-color: white;
                            border-bottom-width: 2px;
                            border-bottom-style: solid;
                        }

                        div.remote {
                            width: 100%;
                            box-sizing: border-box;
                            padding: 5px 10px;

                            color: white;
                            font-size: 1.2rem;
                            
                            display: flex;
                            flex-direction: column;
                            justify-content: center;
                            align-items: flex-start;

                            border-bottom-color: white;
                            border-bottom-width: 2px;
                            border-bottom-style: solid;
                        }

                        pre.content {
                            width: 100%;
                            height: 100%;
                            box-sizing: border-box;
                            padding: 5px 10px;

                            color: white;
                            font-size: 1.0rem;

                            overflow-y: overlay;
                        }

                            pre.input-cmd {
                                display: inline;
                                color: greenyellow;
                            }
                </style>
            )";

            strcpy(data_, html_content.c_str());

            boost::asio::async_write(
                socket_, 
                boost::asio::buffer(data_, strlen(data_)),
                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                {

                }
            );

            // parse query string
            string raw_query = env_table_["QUERY_STRING"];
            vector<string> query = split_string(raw_query, "&");
            string target_host;
            string target_port;
            string target_file;
            vector<string> tmp;
            shared_ptr<tcp::socket> client_ptr(&socket_);

            for(size_t i=0; i<5; i++) {
                // host info
                tmp = split_string(query[3*i + 0], "=");
                if(tmp.size() == 1) {
                    break;
                }
                target_host = tmp[1];

                // port info
                tmp = split_string(query[3*i + 1], "=");
                target_port = tmp[1];

                // file info
                tmp = split_string(query[3*i + 2], "=");
                target_file = tmp[1];

                cerr << "[info, console.cgi] target host: " << target_host << endl;
                cerr << "[info, console.cgi] target port: " << target_port << endl;
                cerr << "[info, console.cgi] target file: " << target_file << endl;

                // build new session
                tcp::resolver resolver(io_context);
                tcp::resolver::query query(target_host, target_port);
                tcp::resolver::iterator iter = resolver.resolve(query);
                tcp::endpoint endpoint = iter->endpoint();
                tcp::socket socket(io_context);
                make_shared<session_host>(
                    move(socket),
                    client_ptr,
                    endpoint,
                    i+1,
                    target_host, 
                    target_port, 
                    target_file
                )->start();
            }

            io_context.run();
        }
};

class server
{
    public:
        tcp::acceptor acceptor_;

        server(boost::asio::io_context& io_context, short port) : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
            cout << "[info, cgi_server] server is created" << endl;
            do_accept();
        }
        
        void do_accept()
        {
            acceptor_.async_accept(
                [this](boost::system::error_code ec, tcp::socket socket)
                {
                    if (!ec) {
                        cout << "-------------------------------------" << endl;
                        cout << "[info, cgi_server] connection is accepted" << endl;
                        std::make_shared<session>(std::move(socket))->start();
                    }

                    do_accept();
                }
            );
        }
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2) {
      std::cerr << "[error, cgi_server] async_tcp_echo_server <port>" << endl;;
      return 1;
    }

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "[error, http_server] " << e.what() << endl;;
  }

  return 0;
}
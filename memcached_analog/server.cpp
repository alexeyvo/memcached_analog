#include "server.h"
#include <iostream>

AsyncServer::Session::Session(asio::ip::tcp::socket socket, Protocol protocol)
  : _socket(std::move(socket)), _protocol(std::move(protocol))
{

}

void AsyncServer::Session::start()
{
  read();
}

void AsyncServer::Session::read()
{
  auto self(shared_from_this());
  _socket.async_read_some(asio::buffer(_data, BUF_SIZE), [this, self](std::error_code ec, std::size_t length)
  {
    if (!ec)
      write(length);
  });
}

void AsyncServer::Session::write(std::size_t length)
{
  auto self(shared_from_this());
  auto res = _protocol.process(std::string(_data, length)) + "\n";
  asio::async_write(_socket, asio::buffer(res, res.size()), [this, self](std::error_code ec, std::size_t /*length*/)
  {
    if (!ec)
      read();
  });
}

AsyncServer::AsyncServer(short port, Protocol &protocol) : ServerInterface(port, protocol),
  _acceptor(_io_service, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)), _socket(_io_service), _protocol(protocol)
{

}

void AsyncServer::start()
{
  accept();
  _io_service.run();
}

void AsyncServer::stop()
{
  accept();
  _io_service.stop();
}

void AsyncServer::accept()
{
  _acceptor.async_accept(_socket, [this](std::error_code ec)
  {
    if (!ec)
      std::make_shared<Session>(std::move(_socket), std::move(_protocol))->start();

    accept();
  });
}

ThreadedServer::ThreadedServer(short port, Protocol &protocol) : ServerInterface(port, protocol), _protocol(protocol), _stop(false)
{
  _port = port;
}

void ThreadedServer::start()
{
  asio::io_context io_context;
  asio::ip::tcp::acceptor a(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), _port));
  while (!_stop)
    try {
      std::thread([&](asio::ip::tcp::socket sock, Protocol protocol)
      {
        try
        {
          while (!_stop)
          {
            char data[BUF_SIZE];

            asio::error_code error;
            size_t length = sock.read_some(asio::buffer(data), error);
            if (error == asio::error::eof)
              break;
            else if (error)
              throw asio::system_error(error);

            std::string input(data, length);
            auto res = protocol.process(input) + "\n";
            asio::write(sock, asio::buffer(res, res.size()));
          }
        }
        catch (std::exception& e)
        {
          std::cerr << "Exception in thread: " << e.what() << "\n";
        }
      }, a.accept(), _protocol).detach();
    }
    catch (const std::exception &e)
    {
      std::cerr << "Exception ignored: " << e.what() << "\n";
    }
}

void ThreadedServer::stop()
{
  _stop = true;
}

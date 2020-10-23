#include "http/HttpServer.h"

#include "muduo/base/Logging.h"
#include "http/HttpContext.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


using namespace muduo;
using namespace muduo::net;

namespace muduo
{
namespace net
{
namespace detail
{

void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
  resp->setStatusCode(HttpResponse::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& name,
                       TcpServer::Option option)
  : server_(loop, listenAddr, name, option),
    httpCallback_(detail::defaultHttpCallback)
{
  server_.setConnectionCallback(
      std::bind(&HttpServer::onConnection, this, _1));
  server_.setMessageCallback(
      std::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

void HttpServer::start()
{
  LOG_WARN << "HttpServer[" << server_.name()
    << "] starts listenning on " << server_.ipPort();
  server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    conn->setContext(HttpContext());
  }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
 // static size_t Content_Length = 0;
  HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());
  /*
  std::cout << "buf->peek():" << buf->peek() << std::endl;
	int fd = open("result_t.txt", O_RDWR | O_CREAT, 0666);
    size_t len = write(fd, buf->peek(), buf->writerIndex());
	if(len != buf->prependableBytes()) {
		std::cout << "write error!" << std::endl;
	}	
	std::cout << "buf->writerIndex():" << buf->writerIndex() << std::endl;
	std::cout << "len:" << len << std::endl;
  std::cout << "Content-Length:" << context->request().getHeader("Content-Length")<<std::endl;
  std::cout << "===========================" << std::endl;
  */
  if (!context->parseRequest(buf, receiveTime))
  {
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
    conn->shutdown();
  }
 // if(context->request().getHeader("Content-Length") != "") {
	 //Content_Length = std::stoi( context->request().getHeader("Content-Length") );
	 //std::cout << "Content_Length:" << Content_Length << std::endl;
 // }
  if (context->gotAll())
  {
    onRequest(conn, context->request());
    context->reset();
  }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
  const string& connection = req.getHeader("Connection");
  bool close = connection == "close" ||
    (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
  HttpResponse response(close);
  httpCallback_(req, &response);
  Buffer buf;
  response.appendToBuffer(&buf);
  conn->send(&buf);
  if (response.closeConnection())
  {
    conn->shutdown();
  }
}


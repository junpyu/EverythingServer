#include "muduo/net/Buffer.h"
#include "http/HttpContext.h"

#include <iostream>

using namespace muduo;
using namespace muduo::net;

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

  thread_local  string body_;
  thread_local  string boundary_;
  thread_local  string filename_;

bool HttpContext::processRequestLine(const char* begin, const char* end)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  if (space != end && request_.setMethod(start, space))
  {
    start = space+1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
      const char* question = std::find(start, space, '?');
      if (question != space)
      {
        request_.setPath(start, question);
        request_.setQuery(question, space);
      }
      else
      {
        request_.setPath(start, space);
      }
      start = space+1;
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          request_.setVersion(HttpRequest::kHttp11);
        }
        else if (*(end-1) == '0')
        {
          request_.setVersion(HttpRequest::kHttp10);
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}
// body
/*
    kBodyRequestLine,
    kBodyHeaders,
    kBodyBody,
	kBodyContinue,
    kBodyAll,
*/
bool HttpContext::parseBody(Buffer* buf)
{
	bool ok = true;
	bool hasMore = true;
	//std::cout << "this body" << std::endl;
	//bodystate_ = kBodyRequestLine;
	while (hasMore)
	{
		if (bodystate_ == kBodyRequestLine) {
			//
			if(request_.getHeader("Content-Type") != "") {
			 std::string oldContentType = request_.getHeader("Content-Type");
			 auto position = oldContentType.find(";");
			 if (position != oldContentType.npos){
				 if("multipart/form-data" == oldContentType.substr(0,position)) {
					auto p_ = oldContentType.find("=");
					if(p_ != oldContentType.npos) {
						//std::cout << oldContentType.substr(p_+1) << std::endl;
						boundary_ = "--"+oldContentType.substr(p_+1);
					}
				 }
			 }	
			}
		  //
			const char* crlf = buf->findCRLF();
			if (crlf)
			{
				if(boundary_ == std::string(buf->peek(), crlf)) {
					buf->retrieveUntil(crlf + 2);
					bodystate_ = kBodyHeaders;
				} else {
					hasMore = false;
				}       
			} 
			else {
				hasMore = false;
			}
		} else if(bodystate_ == kBodyHeaders) {
			//std::cout << "this kBodyHeaders" << std::endl;
			const char* crlf = buf->findCRLF();
			if (crlf)
			{
				const char* colon = std::find(buf->peek(), crlf, ':');
				if (colon != crlf)
				{
				  request_.addHeader(buf->peek(), colon, crlf);
				}
				else
				{
				  // empty line, end of header
				  // FIXME:
				  bodystate_ = kBodyBody;
				}
				buf->retrieveUntil(crlf + 2);
			}
		} else if(bodystate_ == kBodyBody) {
			//std::cout << "this kBodyBody" << std::endl;
			//std::cout << "-------------------" << std::endl;
			//std::cout << buf->peek() << std::endl;
			//std::cout << "-------------------" << std::endl;
			if(request_.getHeader("Content-Disposition") != "") {
				string s = request_.getHeader("Content-Disposition");
			    auto position = s.find("filename");
				if(position != s.npos) {
					filename_ = s.substr(position+10);
					filename_.pop_back();

				}
			}
			bodystate_=kBodyContinue;
			
		} 
		else if(bodystate_ == kBodyContinue) {
			//std::cout << "this kBodyContinue" << std::endl;
			string path = "fileStorage/"+std::to_string(pthread_self())+filename_;
			int fd = open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
			size_t readableBytes = buf->readableBytes();
			string tail(buf->peek()+readableBytes-4-boundary_.size(), buf->peek()+readableBytes-2);
			if(tail == string(boundary_+"--")) {
				bodystate_ = kBodyRequestLine;
				std::cout << "tail:" << tail << std::endl;
				readableBytes=readableBytes-4-boundary_.size();
			}
			//std::cout << "buf->readableBytes()" << buf->readableBytes() << std::endl;
			//std::cout << "buf->writableBytes()" << buf->writableBytes() << std::endl;
			size_t len = write(fd, buf->peek(), readableBytes);
			if(len != readableBytes) {
				std::cout << "write error!" << std::endl;
			}
			buf->retrieveUntil(buf->peek()+ readableBytes);
			//std::cout << filename_;
			hasMore = false;
		}
		
	}
	return ok;
}

// return false if any error
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
  bool ok = true;
  bool hasMore = true;
  //std::cout << "pthread_self:" <<pthread_self() << std::endl;
  //std::cout << "boundary_:" << boundary_ << std::endl;
	//std::cout << "---------start-----------" << std::endl;
	//std::cout << buf->peek() << std::endl;
	//std::cout << "----------end-----------" << std::endl;

	//std::cout << "state_:" <<(state_) << std::endl;
	//std::cout << (bodystate_ == kBodyContinue) << std::endl;
  while (hasMore)
  {
    if (state_ == kExpectRequestLine)
    {
		// ����
		if(bodystate_ == kBodyContinue) {
			if(parseBody(buf)) {
				if(bodystate_ == kBodyRequestLine) {
					state_ = kGotAll;
				}
				hasMore = false;
				break;
			}
		}
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
		if(string(buf->peek(), crlf) == boundary_) {
			if(parseBody(buf)) {
			//	state_ = kGotAll;
				hasMore = false;
				break;
			}
		}
        ok = processRequestLine(buf->peek(), crlf);
        if (ok)
        {
          request_.setReceiveTime(receiveTime);
          buf->retrieveUntil(crlf + 2);
          state_ = kExpectHeaders;
        }
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
    else if (state_ == kExpectHeaders)
    {
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        const char* colon = std::find(buf->peek(), crlf, ':');
        if (colon != crlf)
        {
          request_.addHeader(buf->peek(), colon, crlf);
        }
        else
        {
          // empty line, end of header
          // FIXME:
          state_ = kExpectBody;
         // hasMore = false;
        }
        buf->retrieveUntil(crlf + 2);
      }
      else
      {
        hasMore = false;
      }
    }
    else if (state_ == kExpectBody)
    {
      // FIXME:
	 // std::cout << "-`-`-`-`-`-`-`-" << std::endl;
	 // std::cout << buf->peek() << std::endl;
	 // std::cout << "-`-`-`-`-`-`-`-" << std::endl;
	 //std::cout << "state_ = kExpectBody" << std::endl;
	  if(parseBody(buf)) {
		if(bodystate_ == kBodyRequestLine) {
			state_ = kGotAll;
		}
	  }
      hasMore = false;
    }
	else if(state_ == kGotAll) {
		  hasMore = false;
	}
  }
  return ok;
}

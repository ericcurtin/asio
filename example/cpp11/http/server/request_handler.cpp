//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <zlib.h>
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"

namespace http {
namespace server {

request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root)
{
}

void request_handler::handle_request(const request& req, reply& rep)
{
  // Decode url to path.
  std::string request_path;
  if (!url_decode(req.uri, request_path))
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // Request path must be absolute and not contain "..".
  if (request_path.empty() || request_path[0] != '/'
      || request_path.find("..") != std::string::npos)
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // If path ends in slash (i.e. is a directory) then add "index.html".
  if (request_path[request_path.size() - 1] == '/')
  {
    request_path += "index.html";
  }

  // Determine the file extension.
  std::size_t last_slash_pos = request_path.find_last_of("/");
  std::size_t last_dot_pos = request_path.find_last_of(".");
  std::string extension;
  if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
  {
    extension = request_path.substr(last_dot_pos + 1);
  }

  // Open the file to send back.
  std::string full_path = doc_root_ + request_path;
  std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
  if (!is)
  {
    rep = reply::stock_reply(reply::not_found);
    return;
  }

  // Fill out the reply to be sent to the client.
  rep.status = reply::ok;
  #define CHUNK_SIZE 512
  char buf[CHUNK_SIZE];
/* windowsize is negative to suppress Zlib header */
//#define DEFAULT_COMPRESSION Z_DEFAULT_COMPRESSION
//#define DEFAULT_WINDOWSIZE -15 31
//#define DEFAULT_MEMLEVEL 9
//#define DEFAULT_BUFFERSIZE 8096
//  int size_of_inflated_resp = 0;
  while (is.read(buf, CHUNK_SIZE).gcount() > 0) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret;
    if (ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY)) {
      printf("deflateInit2: %d\n", ret);
    }

    strm.next_in = (unsigned char*) buf;
    // size_of_inflated_resp += is.gcount();
    strm.avail_in = is.gcount();
    char out[CHUNK_SIZE];
    do {
      int have;
      strm.avail_out = CHUNK_SIZE;
      strm.next_out = (unsigned char*) out;
      if (ret = deflate(&strm, Z_FINISH)) {
        printf("deflate: %d\n", ret);
      }

      have = CHUNK_SIZE - strm.avail_out;
      rep.content += std::string(out, have);
    } while (!strm.avail_out);

    if (ret = deflateEnd(&strm)) {
      printf("deflateEnd: %d\n", ret);
    }
  }

  rep.headers.resize(3);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = std::to_string(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);
  rep.headers[2].name = "Content-Encoding";
  rep.headers[2].value = "gzip";
}

bool request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value)
        {
          out += static_cast<char>(value);
          i += 2;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

} // namespace server
} // namespace http

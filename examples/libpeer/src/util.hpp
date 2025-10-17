#pragma once

#include <tuple>
#include <string>

inline std::tuple<std::string, std::string, std::string> parseUrl(const std::string& url) {
  // Parse URL
  std::string protocol, host, path;

  size_t proto_end = url.find("://");
  if (proto_end != std::string::npos) {
    protocol = url.substr(0, proto_end);
    
    size_t host_start = proto_end + 3;
    size_t path_start = url.find('/', host_start);
    

    if (path_start != std::string::npos) {
      host = url.substr(host_start, path_start - host_start);
    } else {
      host = url.substr(host_start);
    }
    
    if (path_start != std::string::npos) {
      path = url.substr(path_start);
    } else {
      path = "/";
    }
  }

  return {protocol, host, path};
}
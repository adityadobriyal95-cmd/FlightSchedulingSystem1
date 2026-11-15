#pragma once
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <functional>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  using SocketHandle = int;
  constexpr SocketHandle INVALID_SOCK = -1;
#endif

namespace httplib {

struct Request {
    std::string method;
    std::string path;
    std::string body;
    std::smatch matches;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;

    bool has_header(const std::string& key) const {
        std::string low = toLower(key);
        for (const auto& kv : headers) {
            if (toLower(kv.first) == low) return true;
        }
        return false;
    }

    std::string get_header_value(const std::string& key) const {
        std::string low = toLower(key);
        for (const auto& kv : headers) {
            if (toLower(kv.first) == low) return kv.second;
        }
        return "";
    }

    bool has_param(const std::string& key) const {
        return params.find(key) != params.end();
    }

    std::string get_param_value(const std::string& key) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : "";
    }

private:
    static std::string toLower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
        return r;
    }
};

struct Response {
    int status = 200;
    std::string body;
    std::map<std::string, std::string> headers;

    void set_content(const std::string& content, const std::string& contentType) {
        body = content;
        headers["Content-Type"] = contentType;
    }

    void set_header(const std::string& key, const std::string& val) {
        headers[key] = val;
    }
};

class Server {
public:
    using Handler = std::function<void(const Request&, Response&)>;

    Server() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    ~Server() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Get(const std::string& pattern, Handler h) { routes_.push_back({"GET", std::regex(pattern), h}); }
    void Post(const std::string& pattern, Handler h) { routes_.push_back({"POST", std::regex(pattern), h}); }
    void Put(const std::string& pattern, Handler h) { routes_.push_back({"PUT", std::regex(pattern), h}); }
    void Delete(const std::string& pattern, Handler h) { routes_.push_back({"DELETE", std::regex(pattern), h}); }
    void Options(const std::string& pattern, Handler h) { routes_.push_back({"OPTIONS", std::regex(pattern), h}); }

    void set_post_routing_handler(Handler h) { postRouting_ = h; }

    void set_mount_point(const std::string& mount, const std::string& dir) {
        mountPoint_ = mount;
        staticDir_ = dir;
    }

    bool listen(const std::string& host, int port) {
        SocketHandle serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSock == INVALID_SOCK) return false;

        int opt = 1;
#ifdef _WIN32
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        if (host == "0.0.0.0" || host.empty()) {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        }

        if (bind(serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            closeSocket(serverSock);
            return false;
        }

        if (::listen(serverSock, 64) < 0) {
            closeSocket(serverSock);
            return false;
        }

        while (true) {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            SocketHandle clientSock = accept(serverSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (clientSock == INVALID_SOCK) continue;

            std::thread(&Server::handleClient, this, clientSock).detach();
        }
        return true;
    }

private:
    struct Route {
        std::string method;
        std::regex pattern;
        Handler handler;
    };

    std::vector<Route> routes_;
    Handler postRouting_;
    std::string mountPoint_;
    std::string staticDir_;

    static void closeSocket(SocketHandle s) {
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }

    static std::string urlDecode(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '%' && i + 2 < s.size()) {
                int val = std::stoi(s.substr(i + 1, 2), nullptr, 16);
                out.push_back(static_cast<char>(val));
                i += 2;
            } else if (s[i] == '+') {
                out.push_back(' ');
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }

    static std::string mimeType(const std::string& path) {
        if (endsWith(path, ".html")) return "text/html; charset=utf-8";
        if (endsWith(path, ".css")) return "text/css; charset=utf-8";
        if (endsWith(path, ".js")) return "application/javascript; charset=utf-8";
        if (endsWith(path, ".json")) return "application/json; charset=utf-8";
        if (endsWith(path, ".svg")) return "image/svg+xml";
        if (endsWith(path, ".png")) return "image/png";
        if (endsWith(path, ".ico")) return "image/x-icon";
        return "text/plain; charset=utf-8";
    }

    static bool endsWith(const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static std::string statusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 409: return "Conflict";
            case 500: return "Internal Server Error";
            case 503: return "Service Unavailable";
            default: return "OK";
        }
    }

    void handleClient(SocketHandle sock) {
        std::string raw;
        char buf[4096];
        size_t headerEnd = std::string::npos;
        size_t contentLength = 0;

        while (true) {
            int n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            raw.append(buf, n);

            if (headerEnd == std::string::npos) {
                headerEnd = raw.find("

");
                if (headerEnd != std::string::npos) {
                    std::string hdrPart = raw.substr(0, headerEnd);
                    std::istringstream iss(hdrPart);
                    std::string line;
                    while (std::getline(iss, line)) {
                        if (!line.empty() && line.back() == '') line.pop_back();
                        std::string low = line;
                        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
                        if (low.rfind("content-length:", 0) == 0) {
                            contentLength = std::stoull(line.substr(15));
                        }
                    }
                }
            }

            if (headerEnd != std::string::npos && raw.size() >= headerEnd + 4 + contentLength) {
                break;
            }
        }

        if (headerEnd == std::string::npos) {
            closeSocket(sock);
            return;
        }

        Request req;
        std::istringstream hdrStream(raw.substr(0, headerEnd));
        std::string requestLine;
        std::getline(hdrStream, requestLine);
        if (!requestLine.empty() && requestLine.back() == '') requestLine.pop_back();

        std::istringstream rlStream(requestLine);
        std::string target;
        rlStream >> req.method >> target;

        size_t qPos = target.find('?');
        if (qPos != std::string::npos) {
            req.path = target.substr(0, qPos);
            std::string query = target.substr(qPos + 1);
            std::istringstream qStream(query);
            std::string pair;
            while (std::getline(qStream, pair, '&')) {
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
                }
            }
        } else {
            req.path = target;
        }

        std::string hLine;
        while (std::getline(hdrStream, hLine)) {
            if (!hLine.empty() && hLine.back() == '') hLine.pop_back();
            size_t colon = hLine.find(':');
            if (colon != std::string::npos) {
                std::string k = hLine.substr(0, colon);
                std::string v = hLine.substr(colon + 1);
                size_t firstNonWs = v.find_first_not_of(" 	");
                if (firstNonWs != std::string::npos) v = v.substr(firstNonWs);
                req.headers[k] = v;
            }
        }

        req.body = raw.substr(headerEnd + 4, contentLength);

        Response res;
        bool matched = false;

        for (const auto& route : routes_) {
            if (route.method == req.method && std::regex_match(req.path, req.matches, route.pattern)) {
                route.handler(req, res);
                matched = true;
                break;
            }
        }

        if (!matched && req.method == "GET" && !staticDir_.empty()) {
            std::string relPath = req.path;
            if (relPath == "/") relPath = "/login.html";
            if (relPath.find("..") == std::string::npos) {
                std::filesystem::path filePath = std::filesystem::path(staticDir_) / relPath.substr(1);
                if (std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath)) {
                    std::ifstream ifs(filePath, std::ios::binary);
                    std::ostringstream ss;
                    ss << ifs.rdbuf();
                    res.status = 200;
                    res.set_content(ss.str(), mimeType(filePath.string()));
                    matched = true;
                }
            }
        }

        if (!matched) {
            res.status = 404;
            res.set_content("{"success":false,"error":"Endpoint or resource not found"}", "application/json");
        }

        if (postRouting_) {
            postRouting_(req, res);
        }

        res.headers["Content-Length"] = std::to_string(res.body.size());
        res.headers["Connection"] = "close";

        std::ostringstream out;
        out << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "
";
        for (const auto& kv : res.headers) {
            out << kv.first << ": " << kv.second << "
";
        }
        out << "
" << res.body;

        std::string payload = out.str();
        send(sock, payload.c_str(), static_cast<int>(payload.size()), 0);
        closeSocket(sock);
    }
};

} // namespace httplib

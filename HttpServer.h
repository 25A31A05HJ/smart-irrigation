#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// Tiny dependency-free HTTP/1.1 server (Windows Winsock + Linux/Mac sockets).
// Single-threaded, one request per connection - perfect for a college project.

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
typedef SOCKET SocketHandle;
#define BAD_SOCKET INVALID_SOCKET
inline void closeSocket(SocketHandle s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <csignal>
typedef int SocketHandle;
#define BAD_SOCKET (-1)
inline void closeSocket(SocketHandle s) { close(s); }
#endif

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> query;  // ?a=1&b=2
    std::map<std::string, std::string> form;   // POST body a=1&b=2
};

struct HttpResponse {
    int status;
    std::string contentType;
    std::string body;
    HttpResponse() : status(200), contentType("text/plain; charset=utf-8") {}
};

inline int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() && hexValue(s[i + 1]) >= 0 && hexValue(s[i + 2]) >= 0) {
            out += static_cast<char>(hexValue(s[i + 1]) * 16 + hexValue(s[i + 2]));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

inline void parseParams(const std::string& s, std::map<std::string, std::string>& out) {
    std::size_t pos = 0;
    while (pos <= s.size()) {
        std::size_t amp = s.find('&', pos);
        if (amp == std::string::npos) amp = s.size();
        std::string pair = s.substr(pos, amp - pos);
        if (!pair.empty()) {
            std::size_t eq = pair.find('=');
            if (eq == std::string::npos)
                out[urlDecode(pair)] = "";
            else
                out[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        }
        pos = amp + 1;
    }
}

class HttpServer {
public:
    typedef std::function<HttpResponse(const HttpRequest&)> Handler;

    explicit HttpServer(int portNumber) : port(portNumber) {}

    bool run(Handler handler) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }
#else
        std::signal(SIGPIPE, SIG_IGN);
#endif
        SocketHandle server = socket(AF_INET, SOCK_STREAM, 0);
        if (server == BAD_SOCKET) {
            std::cerr << "Could not create socket\n";
            return false;
        }
#ifndef _WIN32
        int opt = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // only this computer
        addr.sin_port = htons(static_cast<unsigned short>(port));

        if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            std::cerr << "Could not bind to port " << port
                      << " (already in use?). Try another port: irrigation_server 9090\n";
            closeSocket(server);
            return false;
        }
        if (listen(server, 16) != 0) {
            std::cerr << "listen() failed\n";
            closeSocket(server);
            return false;
        }

        std::cout << "==============================================\n"
                  << " Smart Irrigation Scheduler is running!\n"
                  << " Open in your browser:  http://localhost:" << port << "\n"
                  << " Press Ctrl+C to stop.\n"
                  << "==============================================\n";

        while (true) {
            SocketHandle client = accept(server, nullptr, nullptr);
            if (client == BAD_SOCKET) continue;
            setReceiveTimeout(client);
            handleClient(client, handler);
            closeSocket(client);
        }
        return true;
    }

private:
    int port;

    static void setReceiveTimeout(SocketHandle c) {
#ifdef _WIN32
        DWORD ms = 1000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }

    static std::size_t contentLengthOf(const std::string& headers) {
        std::string lower = headers;
        for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        std::size_t p = lower.find("content-length:");
        if (p == std::string::npos) return 0;
        return static_cast<std::size_t>(std::strtoul(lower.c_str() + p + 15, nullptr, 10));
    }

    static bool readRequest(SocketHandle c, std::string& raw) {
        char buf[4096];
        while (true) {
            std::size_t hdrEnd = raw.find("\r\n\r\n");
            if (hdrEnd != std::string::npos) {
                std::size_t len = contentLengthOf(raw.substr(0, hdrEnd));
                if (len > 65536) return false;
                if (raw.size() >= hdrEnd + 4 + len) return true;
            }
            if (raw.size() > 131072) return false;
            int n = static_cast<int>(recv(c, buf, sizeof(buf), 0));
            if (n <= 0) return false;
            raw.append(buf, static_cast<std::size_t>(n));
        }
    }

    static const char* statusText(int code) {
        switch (code) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            default: return "Internal Server Error";
        }
    }

    static void sendAll(SocketHandle c, const std::string& data) {
        std::size_t off = 0;
        while (off < data.size()) {
#ifdef MSG_NOSIGNAL
            int flags = MSG_NOSIGNAL;
#else
            int flags = 0;
#endif
            int n = static_cast<int>(send(c, data.data() + off, static_cast<int>(data.size() - off), flags));
            if (n <= 0) return;
            off += static_cast<std::size_t>(n);
        }
    }

    static void handleClient(SocketHandle c, const Handler& handler) {
        std::string raw;
        if (!readRequest(c, raw)) return;

        std::size_t hdrEnd = raw.find("\r\n\r\n");
        std::istringstream first(raw.substr(0, raw.find("\r\n")));
        HttpRequest req;
        std::string target, version;
        first >> req.method >> target >> version;
        if (req.method.empty() || target.empty()) return;

        std::size_t q = target.find('?');
        req.path = urlDecode(target.substr(0, q));
        if (q != std::string::npos) parseParams(target.substr(q + 1), req.query);
        req.body = raw.substr(hdrEnd + 4);
        parseParams(req.body, req.form);

        HttpResponse res;
        try {
            res = handler(req);
        } catch (const std::exception& e) {
            res.status = 500;
            res.body = std::string("{\"ok\":false,\"error\":\"") + e.what() + "\"}";
            res.contentType = "application/json";
        }

        std::ostringstream out;
        out << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n"
            << "Content-Type: " << res.contentType << "\r\n"
            << "Content-Length: " << res.body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "Connection: close\r\n\r\n";
        sendAll(c, out.str());
        sendAll(c, res.body);
    }
};

#endif

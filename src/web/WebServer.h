#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <set>
#include <memory>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace tcmt::web {

class WebServer {
public:
    WebServer();
    ~WebServer();

    bool Start(int port = 8080);
    void Stop();
    bool IsRunning() const { return running_.load(); }
    void BroadcastData(const std::string& jsonData);
    std::string LastError() const { return lastError_; }

private:
    using server_t = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;

    void OnHttp(connection_hdl hdl);
    void OnWsOpen(connection_hdl hdl);
    void OnWsClose(connection_hdl hdl);

    server_t server_;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

    std::set<connection_hdl, std::owner_less<connection_hdl>> connections_;
    std::mutex connectionsMutex_;

    std::string lastError_;
};

} // namespace tcmt::web

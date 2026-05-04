#include "IPCServer.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <poll.h>

namespace tcmt::ipc {

IPCServer::IPCServer() = default;

IPCServer::~IPCServer() {
    Stop();
}

bool IPCServer::Start() {
    // 1. Create shared memory file + mmap
    shmFd_ = shm_open("/tcmt_ipc", O_CREAT | O_RDWR, 0666);
    if (shmFd_ == -1) {
        lastError_ = "shm_open failed: " + std::string(std::strerror(errno));
        return false;
    }
    if (ftruncate(shmFd_, sizeof(IPCDataBlock)) == -1) {
        lastError_ = "ftruncate failed: " + std::string(std::strerror(errno));
        close(shmFd_); shmFd_ = -1;
        return false;
    }
    shmPtr_ = mmap(nullptr, sizeof(IPCDataBlock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd_, 0);
    if (shmPtr_ == MAP_FAILED) {
        lastError_ = "mmap failed: " + std::string(std::strerror(errno));
        close(shmFd_); shmFd_ = -1;
        return false;
    }
    std::memset(shmPtr_, 0, sizeof(IPCDataBlock));

    // 2. Create UDS
    listenFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd_ == -1) {
        lastError_ = "socket failed: " + std::string(std::strerror(errno));
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);
    unlink(IPC_SOCK_PATH);

    if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        lastError_ = "bind failed: " + std::string(std::strerror(errno));
        close(listenFd_); listenFd_ = -1;
        return false;
    }

    if (listen(listenFd_, IPC_MAX_CLIENTS) == -1) {
        lastError_ = "listen failed: " + std::string(std::strerror(errno));
        close(listenFd_); listenFd_ = -1;
        return false;
    }

    // 3. Start accept thread
    running_ = true;
    serverThread_ = std::thread(&IPCServer::AcceptLoop, this);

    return true;
}

void IPCServer::Stop() {
    running_ = false;
    if (serverThread_.joinable())
        serverThread_.join();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (int fd : clients_) {
        close(fd);
    }
    clients_.clear();

    if (listenFd_ != -1) { close(listenFd_); listenFd_ = -1; }
    if (shmPtr_ && shmPtr_ != MAP_FAILED) { munmap(shmPtr_, sizeof(IPCDataBlock)); shmPtr_ = nullptr; }
    if (shmFd_ != -1) { close(shmFd_); shmFd_ = -1; }
    unlink(IPC_SOCK_PATH);
}

void IPCServer::AcceptLoop() {
    while (running_) {
        struct pollfd pfd = {listenFd_, POLLIN, 0};
        int ret = poll(&pfd, 1, 500);
        if (ret <= 0) continue;

        struct sockaddr_un clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd == -1) continue;

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(clientFd);
        }
        HandleClient(clientFd);
    }
}

void IPCServer::HandleClient(int clientFd) {
    // Send schema immediately on connect
    SendSchema(clientFd);
}

void IPCServer::SendSchema(int fd) {
    auto data = SerializeSchema();
    if (data.empty()) return;
    write(fd, data.data(), data.size());
}

std::vector<uint8_t> IPCServer::SerializeSchema() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint8_t> buf;
    buf.resize(IPC_SCHEMA_HEADER_SIZE + schemaFields_.size() * IPC_FIELD_DEF_SIZE);

    schemaHeader_.fieldCount = static_cast<uint16_t>(schemaFields_.size());
    std::memcpy(buf.data(), &schemaHeader_, IPC_SCHEMA_HEADER_SIZE);

    for (size_t i = 0; i < schemaFields_.size(); ++i) {
        std::memcpy(buf.data() + IPC_SCHEMA_HEADER_SIZE + i * IPC_FIELD_DEF_SIZE,
                    &schemaFields_[i], IPC_FIELD_DEF_SIZE);
    }
    return buf;
}

void IPCServer::UpdateSchema(const SchemaHeader& header, const std::vector<FieldDef>& fields) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        schemaHeader_ = header;
        schemaFields_ = fields;
    }

    auto data = SerializeSchema();

    // Broadcast to all connected clients
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto it = clients_.begin(); it != clients_.end(); ) {
        int n = write(*it, data.data(), data.size());
        if (n <= 0) {
            close(*it);
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace tcmt::ipc

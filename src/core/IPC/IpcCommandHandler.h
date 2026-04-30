#pragma once
#include "DataStruct/DataStruct.h"
#include <functional>
#include <mutex>
#include <unordered_map>

// Callback type: returns true if command was handled
using IpcCommandCallback = std::function<bool(const IpcCommand& cmd)>;

class IpcCommandHandler {
public:
    static IpcCommandHandler& Instance();

    // Called each main loop iteration from the C++ side (Windows: SharedMemoryBlock path)
    void ProcessCommands(SharedMemoryBlock* block);

    // Generic overload: process directly from command/ack fields (any shared memory block)
    void ProcessCommands(IpcCommand* cmd, uint32_t* ack);

    // Register a callback for specific command types
    void RegisterHandler(IpcCommandType type, IpcCommandCallback callback);

private:
    IpcCommandHandler() = default;
    ~IpcCommandHandler() = default;
    IpcCommandHandler(const IpcCommandHandler&) = delete;
    IpcCommandHandler& operator=(const IpcCommandHandler&) = delete;

    std::mutex mutex_;
    std::unordered_map<uint32_t, IpcCommandCallback> handlers_;
};

#include "IpcCommandHandler.h"
#include "../Utils/Logger.h"

IpcCommandHandler& IpcCommandHandler::Instance() {
    static IpcCommandHandler instance;
    return instance;
}

void IpcCommandHandler::ProcessCommands(SharedMemoryBlock* block) {
    if (!block) return;
    ProcessCommands(&block->command, &block->commandAck);
}

void IpcCommandHandler::ProcessCommands(IpcCommand* cmd, uint32_t* ack) {
    if (!cmd || !ack) return;

    if (cmd->magic == 0x54434D54 && cmd->status == 0) {
        IpcCommandCallback handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(cmd->type);
            if (it != handlers_.end())
                handler = it->second;
        }
        if (handler) {
            bool ok = handler(*cmd);
            cmd->status = ok ? 1 : 2;
            cmd->result = ok ? 0 : 1;
            Logger::Debug("IPC command processed: type=" + std::to_string(cmd->type) +
                         ", id=" + std::to_string(cmd->id) +
                         ", status=" + std::to_string(cmd->status));
        } else {
            cmd->status = 2; // error: unknown command
            cmd->result = 2;
            Logger::Warn("Unknown IPC command type: " + std::to_string(cmd->type));
        }
        *ack = cmd->id;
    }
}

void IpcCommandHandler::RegisterHandler(IpcCommandType type, IpcCommandCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[static_cast<uint32_t>(type)] = std::move(callback);
}

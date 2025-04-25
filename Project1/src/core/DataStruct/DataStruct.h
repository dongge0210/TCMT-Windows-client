struct SharedMemoryBlock {
    CRITICAL_SECTION lock;
    char cpuName[128];
    int physicalCores;
    int logicalCores;
    float cpuUsage;
    int performanceCores;
    int efficiencyCores;
    double pCoreFreq;
    double eCoreFreq;
    bool hyperThreading;
    bool virtualization;

    uint64_t totalMemory;
    uint64_t usedMemory;
    uint64_t availableMemory;

    struct GpuInfo {
        wchar_t name[128];
        wchar_t brand[64];
        uint64_t memory;
        double coreClock;
    } gpus[2];
    int gpuCount;

    struct AdapterInfo {
        wchar_t name[128];
        wchar_t mac[64];
        uint64_t speed;
    } adapters[4];
    int adapterCount;

    struct DiskInfo {
        char letter;
        wchar_t label[64];
        wchar_t fileSystem[32];
        uint64_t totalSize;
        uint64_t usedSpace;
        uint64_t freeSpace;
    } disks[8];
    int diskCount;

    struct TemperatureInfo {
        wchar_t sensorName[64];
        float temperature;
    } temperatures[10];
    int tempCount;

    SYSTEMTIME lastUpdate;
};

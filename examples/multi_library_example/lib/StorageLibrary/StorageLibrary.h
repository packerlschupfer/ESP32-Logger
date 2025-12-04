#ifndef STORAGE_LIBRARY_H
#define STORAGE_LIBRARY_H

#include "LogInterface.h"
#include <vector>

class StorageLibrary {
private:
    static constexpr const char* TAG = "Storage";
    static constexpr size_t MAX_RECORDS = 100;
    
    struct DataRecord {
        unsigned long timestamp;
        float value1;
        float value2;
    };
    
    std::vector<DataRecord> records;
    bool initialized = false;
    size_t totalWrites = 0;
    size_t totalReads = 0;
    
public:
    bool begin() {
        LOG_INFO(TAG, "Initializing storage library...");
        
        // Simulate filesystem initialization
        LOG_DEBUG(TAG, "Mounting filesystem...");
        delay(200);
        
        LOG_DEBUG(TAG, "Checking available space...");
        size_t freeSpace = 1024 * 1024; // 1MB
        LOG_INFO(TAG, "Free space: %d KB", freeSpace / 1024);
        
        // Simulate loading existing data
        LOG_DEBUG(TAG, "Loading existing records...");
        int existingRecords = random(10);
        for (int i = 0; i < existingRecords; i++) {
            records.push_back({millis() - (i * 10000), 20.0f + i, 50.0f + i});
        }
        
        LOG_INFO(TAG, "Storage initialized with %d existing records", existingRecords);
        initialized = true;
        return true;
    }
    
    bool saveRecord(float value1, float value2) {
        if (!initialized) {
            LOG_ERROR(TAG, "Cannot save - storage not initialized");
            return false;
        }
        
        if (records.size() >= MAX_RECORDS) {
            LOG_WARN(TAG, "Storage full (%d records), removing oldest", MAX_RECORDS);
            records.erase(records.begin());
        }
        
        DataRecord record = {millis(), value1, value2};
        records.push_back(record);
        totalWrites++;
        
        LOG_DEBUG(TAG, "Saved record #%d - Time: %lu, V1: %.2f, V2: %.2f", 
                  records.size(), record.timestamp, value1, value2);
        
        // Simulate occasional write delays
        if (random(5) == 0) {
            LOG_VERBOSE(TAG, "Flash write delay...");
            delay(50);
        }
        
        // Log statistics periodically
        if (totalWrites % 10 == 0) {
            LOG_INFO(TAG, "Storage stats - Records: %d/%d, Writes: %d, Reads: %d",
                     records.size(), MAX_RECORDS, totalWrites, totalReads);
        }
        
        return true;
    }
    
    size_t getRecordCount() const {
        size_t count = records.size();
        LOG_VERBOSE(TAG, "Record count requested: %d", count);
        return count;
    }
    
    bool getRecord(size_t index, float& value1, float& value2, unsigned long& timestamp) {
        if (index >= records.size()) {
            LOG_ERROR(TAG, "Invalid record index: %d (max: %d)", index, records.size() - 1);
            return false;
        }
        
        const DataRecord& record = records[index];
        value1 = record.value1;
        value2 = record.value2;
        timestamp = record.timestamp;
        totalReads++;
        
        LOG_VERBOSE(TAG, "Read record %d - Time: %lu, V1: %.2f, V2: %.2f",
                    index, timestamp, value1, value2);
        
        return true;
    }
    
    void clearAll() {
        LOG_WARN(TAG, "Clearing all records...");
        size_t count = records.size();
        records.clear();
        LOG_INFO(TAG, "Cleared %d records from storage", count);
    }
    
    void printStats() {
        LOG_INFO(TAG, "=== Storage Statistics ===");
        LOG_INFO(TAG, "Total records: %d/%d", records.size(), MAX_RECORDS);
        LOG_INFO(TAG, "Total writes: %d", totalWrites);
        LOG_INFO(TAG, "Total reads: %d", totalReads);
        LOG_INFO(TAG, "Memory used: %d bytes", records.size() * sizeof(DataRecord));
        
        if (records.size() > 0) {
            LOG_DEBUG(TAG, "Oldest record: %lu ms ago", millis() - records.front().timestamp);
            LOG_DEBUG(TAG, "Newest record: %lu ms ago", millis() - records.back().timestamp);
        }
    }
};

#endif // STORAGE_LIBRARY_H
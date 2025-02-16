#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <sqlite3.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

namespace bip = boost::interprocess;
namespace fs = std::filesystem;

struct alignas(64) SharedData {  
    double left_rpm;
    double right_rpm;
    double linear_vel;
    double angular_vel;
    int64_t timestamp_ms;
    bool new_data;
    std::mutex mtx;
    std::condition_variable cv;
};

class DifferentialDriveCalculator {
private:
    const double WHEEL_DISTANCE = 0.443;  // meters
    const double WHEEL_DIAMETER = 0.181;  // meters
    const double WHEEL_RADIUS = WHEEL_DIAMETER / 2.0;
    const double RPM_CONVERSION = 60.0 / (2.0 * M_PI);
    
    bip::shared_memory_object shm;
    bip::mapped_region region;
    SharedData* shared_data;
    sqlite3* db;
    std::string db_path;

    void calculateWheelRPM(double linear_vel, double angular_vel) {
        double right_wheel_vel = (2.0 * linear_vel + angular_vel * WHEEL_DISTANCE) / 2.0;
        double left_wheel_vel = (2.0 * linear_vel - angular_vel * WHEEL_DISTANCE) / 2.0;
        
        double right_rpm = (right_wheel_vel / WHEEL_RADIUS) * RPM_CONVERSION;
        double left_rpm = (left_wheel_vel / WHEEL_RADIUS) * RPM_CONVERSION;

        // Debugging Print
        std::cout << "[INFO] Computed RPMs: Left = " << left_rpm << ", Right = " << right_rpm << std::endl;

        std::lock_guard<std::mutex> lock(shared_data->mtx);
        shared_data->right_rpm = right_rpm;
        shared_data->left_rpm = left_rpm;
        shared_data->linear_vel = linear_vel;
        shared_data->angular_vel = angular_vel;
        shared_data->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        shared_data->new_data = true;
        shared_data->cv.notify_all();
    }

    void checkDatabaseSchema() {
        const char* sql = R"(
            SELECT name, sql 
            FROM sqlite_master 
            WHERE type='table' 
            AND name='messages';
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare schema check: " + std::string(sqlite3_errmsg(db)));
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* tableName = sqlite3_column_text(stmt, 0);
            const unsigned char* tableSql = sqlite3_column_text(stmt, 1);
            std::cout << "[INFO] Table: " << tableName << " | SQL: " << tableSql << std::endl;
        }
        sqlite3_finalize(stmt);
    }

public:
    DifferentialDriveCalculator(const std::string& database_path) :
        shm(bip::open_or_create, "wheel_velocity_data", bip::read_write),
        db_path(database_path)
    {
        try {
            shm.truncate(sizeof(SharedData));
            region = bip::mapped_region(shm, bip::read_write);
            shared_data = new (region.get_address()) SharedData();
            std::memset(shared_data, 0, sizeof(SharedData));  // Ensure initialized values
            shared_data->new_data = false;
            
            if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
                cleanup();
                throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
            }
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }

    void cleanup() {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
    }

    ~DifferentialDriveCalculator() {
        cleanup();
        bip::shared_memory_object::remove("wheel_velocity_data");
    }

    void processRosbag() {
        checkDatabaseSchema();
    
        const char* sql = R"(
            SELECT data, timestamp
            FROM messages
            WHERE topic_id = (SELECT id FROM topics WHERE name = '/cmd_vel')
            ORDER BY timestamp;
        )";
    
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(db)));
        }
    
        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            int blob_size = sqlite3_column_bytes(stmt, 0);
    
            if (blob_size < 16) {  // ensuring we have enough bytes for two doubles
                std::cerr << "[WARN] Skipping small/invalid data entry." << std::endl;
                continue;
            }

            std::cout << "[INFO] Extracted Data Size: " << blob_size << " bytes" << std::endl;

            try {
    eprosima::fastcdr::FastBuffer buffer((char*)blob, blob_size);
    eprosima::fastcdr::Cdr deserializer(buffer);

    uint32_t unused_header;  
    deserializer >> unused_header;

    double linear_vel, angular_vel;
    deserializer >> linear_vel >> angular_vel;

    std::cout << "[INFO] Decoded: Linear Vel = " << linear_vel 
              << ", Angular Vel = " << angular_vel << std::endl;

    calculateWheelRPM(linear_vel, angular_vel);
} catch (const std::exception& e) {
    std::cerr << "[ERROR] Deserialization failed: " << e.what() << std::endl;
}

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); //simulating real-time 
        }
    
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("SQL query failed: " + std::string(sqlite3_errmsg(db)));
        }
    
        sqlite3_finalize(stmt);
    }
};

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <path_to_rosbag_db>" << std::endl;
            return 1;
        }
        DifferentialDriveCalculator calculator(argv[1]);
        calculator.processRosbag();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

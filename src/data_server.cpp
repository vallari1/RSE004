
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

namespace bip = boost::interprocess;
using json = nlohmann::json;

struct SharedData {
    double left_rpm;
    double right_rpm;
    double linear_vel;
    double angular_vel;
    int64_t timestamp_ms;
    bool new_data;
};

class DataServer {
private:
    bip::shared_memory_object shm;
    bip::mapped_region region;
    httplib::Server svr;
    std::atomic<bool> running{true};
    const int LOOP_RATE_HZ = 10;

    void setupEndpoints() {
        svr.Get("/get_data_from_B", [this](const httplib::Request&, httplib::Response& res) {
            auto* shared_data = static_cast<SharedData*>(region.get_address());
            
            json response = {
                {"left_rpm", shared_data->left_rpm},
                {"right_rpm", shared_data->right_rpm},
                {"linear_vel", shared_data->linear_vel},
                {"angular_vel", shared_data->angular_vel},
                {"timestamp_a", shared_data->timestamp_ms},
                {"timestamp_b", std::chrono::system_clock::now().time_since_epoch().count()}
            };
            
            res.set_content(response.dump(), "application/json");
        });
    }

public:
    DataServer() : shm(bip::open_only, "wheel_velocity_data", bip::read_only),
                  region(shm, bip::read_only) {
        setupEndpoints();
    }

    void run() {
        // Start HTTP server in a separate thread
        std::thread server_thread([this]() {
            svr.listen("localhost", 8080);
        });

        // Main loop at 10Hz
        while (running) {
            auto start = std::chrono::steady_clock::now();
            
            auto* shared_data = static_cast<SharedData*>(region.get_address());
            if (shared_data->new_data) {
                std::cout << "Left RPM: " << shared_data->left_rpm << "\n"
                         << "Right RPM: " << shared_data->right_rpm << "\n"
                         << "Linear Vel: " << shared_data->linear_vel << "\n"
                         << "Angular Vel: " << shared_data->angular_vel << "\n"
                         << "Timestamp: " << shared_data->timestamp_ms << "\n\n";
            }

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            auto sleep_time = std::chrono::milliseconds(1000 / LOOP_RATE_HZ) - duration;
            
            if (sleep_time > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }

        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    void stop() {
        running = false;
        svr.stop();
    }
};

int main() {
    try {
        DataServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
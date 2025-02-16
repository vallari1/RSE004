# Differential Drive RPM Calculator & Visualizer without ROS

A  system for processing ROS 2 `cmd_vel` messages from `.db3` bag files, computing wheel RPMs using differential drive kinematics, and visualizing the results in real-time.

## System Architecture

The project consists of three main components working together:

1. **Message Processor (Script A)**
   - Reads and deserializes ROS 2 messages from SQLite database, then calculates and sends calculated rpms to shared memory
   
2. **Data Server (Script B)**
   - Accesses shared memory containing RPM data, and gives api endpoints
   
3. **Visualizer (Script C)**
   - Fetches real-time data via REST API, amd plots it

![Alt text](./media/Screenshot_20250216_135549.png?raw=true "Screenshot")



### Key Libraries & Dependencies

- **SQLite3**
  - Direct access to ROS 2 bag files without ROS dependencies (since using ROS was forbidden for commucation)


- **Fast CDR**
  - Native ROS 2 message deserialization

- **Boost**
  - for shared memory implementation using interprocess communication  in cpp

- **FastAPI**
  - best performance wise

- **Matplotlib**
  - for plotting data 

## Prerequisites

### System Requirements

- Ubuntu 22.04 or newer for ROS2 (I personally used ROS2 Jazzy even though the bag file was for HUmble according to info)


## Installation

### System Dependencies

```bash
# Update package list
sudo apt update

# Install C++ build tools and libraries
sudo apt install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libsqlite3-dev \
    libfastcdr-dev

# Install Python and pip
sudo apt install -y python3-pip
```

### Python Dependencies

```bash
pip3 install \
    requests \
    matplotlib \
    numpy \
    fastapi \
    uvicorn
```

### Building from Source

```bash
# Clone repository
git clone https://github.com/vallari1/RSE004.git
cd RSE004

# Create build directory
rm -rf build
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)
```

## Usage

### Running the System

Start each component in separate terminals:

```bash
# Terminal 1: Start the message processor
./script_a /path/in-your-laptop/bagfile.db3

# Terminal 2: Launch the data server
./script_b

# Terminal 3: Run the visualizer
python3 ../src/visuals.py
```

### Configuration

The system uses several configurable parameters:

```cpp
// In config.h
#define WHEEL_RADIUS 0.1    // meters
#define WHEEL_BASE 0.5      // meters
#define RPM_SCALE 9.5493    // conversion factor from rad/s to RPM
```

### API Endpoints

The REST API provides the following endpoints:

- `GET /get_data_from_B`
  - Returns current RPM data
  - Format: `{"left_rpm": float, "right_rpm": float, "timestamp": int}`

## Development & Debugging

### Bag File Inspection

```bash
# List tables in the bag file
sqlite3 /path/in-your-laptop/rse_assignment_unbox_robotics.db3 ".tables"

# View topic information
sqlite3 /path/in-your-laptop/rse_assignment_unbox_robotics.db3 "SELECT id, name FROM topics;"

# Check message count
sqlite3 /path/in-your-laptop/rse_assignment_unbox_robotics.db3 "SELECT COUNT(*) FROM messages WHERE topic_id=4;"
```

### Shared Memory Debugging

```bash
# Check if shared memory segment exists
ipcs -m

# Remove shared memory (if needed)
ipcrm -M <shared_memory_key>
```

### API Testing

```bash
# Test API endpoint
curl http://localhost:8080/get_data_from_B

# Monitor API logs
tail -f api_server.log
```

## Technical Details

### Differential Drive Kinematics

The system uses standard differential drive equations:

```
left_wheel_rpm = (linear_vel - angular_vel * wheel_base/2) * rpm_scale / wheel_radius
right_wheel_rpm = (linear_vel + angular_vel * wheel_base/2) * rpm_scale / wheel_radius
```

### Data Flow

1. Script A reads messages from SQLite
2. Deserializes using Fast CDR
3. Computes RPMs
4. Writes to shared memory
5. Script B reads shared memory
6. Serves data via REST API
7. Script C fetches and visualizes

### Performance Considerations

- Shared memory provides microsecond-level latency
- Message processing runs in a dedicated thread
- Visualization updates at 60 FPS
- REST API implements connection pooling

## Troubleshooting

### Common Issues

1. **Shared Memory Access**
   ```bash
   # Fix permissions
   sudo chmod 666 /dev/shm/rpm_data
   ```

2. **Database Access**
   ```bash
   # Check file permissions
   ls -l /home/your-laptop-path/bagfile.db3
   
   # Fix if needed
   chmod 644 /home/your-laptop-path/bagfile.db3
   ```

3. **Build Failures**
   ```bash
   # Clean build
   cd build
   rm -rf *
   cmake ..
   make clean
   make -j$(nproc)
   ```

4. **Bag File Analysis**

#### Basic Bag Information
```bash
# Get bag file information
ros2 bag info /path/to/rse_assignment_unbox_robotics.db3
ros2 bag play /path/in-your-laptop/rse_assignment_unbox_robotics.db3

# List all tables in the database
sqlite3 /path/in-your-laptop/rse_assignment_unbox_robotics.db3 ".tables"

# View topic information
sqlite3 /path/in-your-laptop/rse_assignment_unbox_robotics.db3"SELECT id, name FROM topics;"
```


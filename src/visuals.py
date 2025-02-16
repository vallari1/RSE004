import requests
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np
from collections import deque
import time

class WheelVelocityVisualizer:
    def __init__(self, window_size=100):
        self.window_size = window_size
        self.timestamps = deque(maxlen=window_size)
        self.left_rpms = deque(maxlen=window_size)
        self.right_rpms = deque(maxlen=window_size)
        
        # Setup plot
        plt.style.use('dark_background')
        self.fig, self.ax = plt.subplots(figsize=(12, 6))
        self.left_line, = self.ax.plot([], [], 'b-', label='Left Wheel RPM', linewidth=2)
        self.right_line, = self.ax.plot([], [], 'r-', label='Right Wheel RPM', linewidth=2)
        
        self.ax.set_title('Differential Drive Wheel Velocities')
        self.ax.set_xlabel('Time (s)')
        self.ax.set_ylabel('RPM')
        self.ax.legend()
        self.ax.grid(True, alpha=0.3)
        
    def fetch_data(self):
        try:
            response = requests.get('http://localhost:8080/get_data_from_B')
            if response.status_code == 200:
                data = response.json()
                print(f"\nReceived data:")
                print(f"Left RPM: {data['left_rpm']:.2f}")
                print(f"Right RPM: {data['right_rpm']:.2f}")
                print(f"Linear Velocity: {data['linear_vel']:.2f} m/s")
                print(f"Angular Velocity: {data['angular_vel']:.2f} rad/s")
                return data
            else:
                print(f"Error: Received status code {response.status_code}")
                return None
        except requests.exceptions.RequestException as e:
            print(f"Connection error: {e}")
            return None
            
    def update_plot(self, frame):
        data = self.fetch_data()
        if data:
            self.timestamps.append(time.time())
            self.left_rpms.append(data['left_rpm'])
            self.right_rpms.append(data['right_rpm'])
            
            x = np.array(self.timestamps) - self.timestamps[0]
            self.left_line.set_data(x, self.left_rpms)
            self.right_line.set_data(x, self.right_rpms)
            
            self.ax.relim()
            self.ax.autoscale_view()
            
        return self.left_line, self.right_line
        
    def run(self):
        ani = FuncAnimation(self.fig, self.update_plot, 
                          interval=100,  # 10 Hz update rate
                          blit=True)
        plt.show()

if __name__ == "__main__":
    visualizer = WheelVelocityVisualizer()
    visualizer.run()
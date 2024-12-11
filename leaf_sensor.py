import socket
import json
import csv
import time
import threading
import queue
import configparser
import shutil

import pyqtgraph as pg
from PyQt5.QtWidgets import QApplication

import os
import sys

# Read the configuration file
config = configparser.ConfigParser()
config_filename = "config.ini"
# Check if the config file exists
if not os.path.exists(config_filename):
    # Set default values
    config['parameters'] = {
        'sample_interval': '100',
        'pre_stepV': '0,0,0,0',
        'v1': '0,0,0,0',
        'v2': '0,0,0,0',
        'gain': '5,5,5,5',
        'quietTime': '1000',
        't1': '1000',
        't2': '1000'
    }
    
    # Write the default configuration to the file
    with open(config_filename, 'w') as configfile:
        config.write(configfile)
    print(f"Default config file created at {config_filename}")
else:
    print(f"Config file '{config_filename}' already exists.")
    
config.read(config_filename)

# Configuration for the server
HOST = "192.168.1.100"  # IP address of the Arduino Nano 33 IoT
PORT = 80              # Port number matching the server

# Headers and indices (for reference)
headers = ["v_bias1", "time1", "i1", "v_bias2", "time2", "i2", "v_bias3", "time3", "i3", "v_bias4", "time4", "i4"]
# Initialize a thread-safe queue
data_queue = queue.Queue()

# Initialize storage for plotting
data = {key: [] for key in headers}
colors = {
    'v_bias1':'r','i1':'r',
    'v_bias2':'g','i2':'g',
    'v_bias3':'b','i3':'b',
    'v_bias4':'y','i4':'y',
}

def io_thread(q, stop_event):
    """Thread for handling IO with the server."""

    # Data to send
    # Parse values into send_data
    send_data = {
        "sample_interval": int(config["parameters"]["sample_interval"]),
        "range": int(6),
        "pre_stepV": list(map(int, config["parameters"]["pre_stepV"].split(","))),
        "v1": list(map(int, config["parameters"]["v1"].split(","))),
        "v2": list(map(int, config["parameters"]["v2"].split(","))),
        "gain": list(map(int, config["parameters"]["gain"].split(","))),
        "quietTime": int(config["parameters"]["quietTime"]),
        "t1": int(config["parameters"]["t1"]),
        "t2": int(config["parameters"]["t2"]),
    }
    
    # Create a TCP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
        print(f"Connecting to {HOST}:{PORT}...")
        client_socket.connect((HOST, PORT))
        print("Connected. Receiving data...\n")
        
        json_data = json.dumps(send_data) + '\n'  # Add newline to signify end
        client_socket.sendall(json_data.encode('utf-8'))
        print("Sent data:", json_data)
        
        timestamp = time.strftime("%Y-%m-%d_%H-%M-%S")
        
        shutil.copy("config.ini", timestamp+"_"+config_filename)
        with open(timestamp+'_chronoamperometry.csv', 'w', newline='') as csvfile:
            csvwriter = csv.writer(csvfile)
            csvwriter.writerow(headers)
            
            data = ''
            
            try:
                while not stop_event.is_set():
                    # Receive data from the server
                    if data and data[-1] != '\n':
                        data = data.split('\n')[-1]
                    else:
                        data = ''
                    
                    data += client_socket.recv(1024).decode('utf-8')
                    
                    rows = data.split('\n')                        
                    rows = [row.strip(',') for row in rows]
                    for row in rows:
                        row = row.split(',')
                        if len(row) == 12:
                            for i in [1,4,7]:
                                row[i] = str(float(row[i])/1000.0)
                            print(row)
                            q.put([float(x) for x in row])
                            csvwriter.writerow(row)
                    if not data or "STOP" in data.strip():
                        client_socket.close()
                        break
            except Exception as e:
                print(f"Error: {e}")

def real_time_plotter(q:queue.Queue):
    """Plotting happens in the main thread using pyqtgraph."""
    app = QApplication([])  # Create the application instance
    
    # Create a window with two plots
    win = pg.GraphicsLayoutWidget(show=True)
    win.showMaximized()
    win.setWindowTitle('Real-Time Plotter')
    
    # Create two plot areas
    ax1 = win.addPlot(title="V_bias Setting")
    ax1.setLabel('left', 'v_bias')
    ax1.setLabel('bottom', 'time')
    ax1.addLegend()
    
    ax2 = win.addPlot(title='Current (uA)')
    ax2.setLabel('left', 'i')
    ax2.setLabel('bottom', 'time')
    ax2.addLegend()
    
    # Initialize the scatter plots for "v_bias" across time
    v_bias_lines = {
        key: ax1.plot([], [], symbol='o', name=key, pen=pg.mkPen(colors[key]), symbolBrush=colors[key]) for key in headers if "v_bias" in key
    }
    
    # Initialize the scatter plots for "i" across time
    i_lines = {
        key: ax2.plot([], [], symbol='o', name=key, pen=pg.mkPen(colors[key]), symbolBrush=colors[key]) for key in headers if key.startswith("i")
    }
    
    # Keep track of the data
    data = {key: [] for key in headers}
    
    while not win.isHidden():
        try:
            new_data = q.get(block=False)
            if not new_data or len(new_data) != len(headers):
                continue
            
            # Append new data to the corresponding storage
            for i, key in enumerate(headers):
                data[key].append(new_data[i])
            
            # Update v_bias plots
            for key, line in v_bias_lines.items():
                line.setData(data["time1"], data[key])  # Assuming "time1" as the x-axis
            
            # Update i plots
            for key, line in i_lines.items():
                line.setData(data["time1"], data[key])  # Assuming "time1" as the x-axis
            
            # Process the GUI events (important for real-time updates)
            app.processEvents()
        except queue.Empty:
            app.processEvents()
            continue

def main():
    # Event to stop the IO thread
    stop_event = threading.Event()
    
    # Start the IO thread
    io_thread_instance = threading.Thread(target=io_thread, args=(data_queue, stop_event), daemon=True)
    io_thread_instance.start()
    
    try:
        # Start the plotting in the main thread
        real_time_plotter(data_queue)
    except KeyboardInterrupt:
        print("\nStopping...")
        stop_event.set()
        io_thread_instance.join()

if __name__ == "__main__":
    main()
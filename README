# "Arduino Nano 33 IoT x LMP9100" Potentiostat with Real-Time Visualization

## Chronoamperometry Overview

This repository contains two files that enable real-time monitoring and visualization of chronoamperometry experiments:

1. **`chronoamperometry.py`**: A Python script for communicating with an Arduino Nano 33 IoT, collecting data, and plotting results in real time.
2. **`chronoamperometry.ino`**: An Arduino sketch that handles the hardware-side operation, including data collection and network communication.

---

### Hardware Setup
- Connect the Arduino Nano 33 IoT to your sensors and actuators according to the requirements of your chronoamperometry experiment.

### Software Setup

#### On Arduino:
1. Open the Arduino IDE.
2. Load the `chronoamperometry.ino` file.
3. (Optional) Configure the Wi-Fi credentials:
   ```Python
   char ssid[] = "Leaf_Sensor_AP";
   char pass[] = "Leaf_Sensor_AP_password";
   ```
4. Upload the sketch to the Arduino Nano 33 IoT.

#### On Python:
1. Ensure Python is installed on your system.
2. Install the required libraries using pip:
   ```bash
   pip install pyqtgraph PyQt5
   ```
3. Place the `chronoamperometry.py` file in your desired working directory.
4. Run the script:
   ```bash
   python chronoamperometry.py
   ```

---

### Configuration using `config.ini`
The `chronoamperometry.py` script uses a configuration file to define experimental parameters. If no `config.ini` file is found, a default one is generated with the following settings:
```ini
[parameters]
sample_interval = 100
pre_stepV = 0,0,0,0
v1 = 0,0,0,0
v2 = 0,0,0,0
gain = 5,5,5,5
quietTime = 1000
t1 = 1000
t2 = 1000
```
You can modify these parameters to suit your experiment.
Voltages and time are recorded in millivolts (mV) and milliseconds (ms) respectively.
Voltages can be either positive or negative. 792mV is the maximum value and voltages exceeding that will result in no output.
Please consult the LMP91000 datasheet for setting the gain parameter.

The chronoamperometry experiment happens in three stages (marked by `quietTime`, `t1`, and `t2`).
At each stage, the corresponding voltage is applied to the DUT (namely `pre_stepV`, `v1`, and `v2`).
Variable `sample_interval` sets the time between samples in milliseconds (ms).
Values under 20ms must be avoided.



---

### Running the Experiment

1. **Start the Arduino Sketch**:
   - Ensure the Arduino is connected to your network and running the `chronoamperometry.ino` sketch.

2. **Run the Python Script**:
   - Execute `chronoamperometry.py` to establish a connection with the Arduino.
   - View voltage and current plots in real time using `pyqtgraph`.
3. **Stop the Python Script**:
   - The Python script generates a CSV file with a timestamped filename containing columns for:
     - Bias voltage (`v_bias1`, `v_bias2`, etc.)
     - Time (`time1`, `time2`, etc.)
     - Current (`i1`, `i2`, etc.)
   - The Python script saves a configuration (.ini) file with a timestamped filename containing the parameters used to run the chronoamperometry experiment.

---

## License
This project is licensed under the MIT License.


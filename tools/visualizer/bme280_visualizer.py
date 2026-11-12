"""Live BME280 readings over serial.

Usage:
    python bme280_visualizer.py [PORT] [BAUD]

Examples:
    python bme280_visualizer.py /dev/tty.usbmodem1103   (macOS)
    python bme280_visualizer.py COM10                   (Windows)

If PORT is omitted, available serial ports are listed and the first one is used.
Expects lines of the form: "<temp_C>,<pressure_hPa>,<humidity_%>"
"""
import sys
import serial
import serial.tools.list_ports
from PyQt5 import QtWidgets, QtCore, QtGui

# ----------------------------- CONFIG --------------------------------
DEFAULT_BAUD = 115200


def pick_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("ERROR: No serial ports found. Pass the port as an argument.")
        sys.exit(1)
    print("Available ports:")
    for p in ports:
        print(f"  {p.device}  ({p.description})")
    print(f"No port given, using {ports[0].device}")
    return ports[0].device


PORT = pick_port()
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD

# ------------------------- SERIAL SETUP ------------------------------
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected to {PORT} at {BAUD} baud")
except serial.SerialException as e:
    print(f"ERROR: Could not open {PORT}: {e}")
    sys.exit(1)


# ------------------------- GUI SETUP ---------------------------------
class BME280Window(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("BME280 Live Values")
        self.setGeometry(100, 100, 600, 400)

        # Set background color
        self.setStyleSheet("background-color: #1e1e2f; color: #ffffff;")

        # Fonts
        font_label = QtGui.QFont("Arial", 24, QtGui.QFont.Bold)

        # Labels
        self.temp_label = QtWidgets.QLabel("Temperature: -- °F", self)
        self.temp_label.setFont(font_label)
        self.temp_label.setAlignment(QtCore.Qt.AlignCenter)

        self.press_label = QtWidgets.QLabel("Pressure: -- hPa", self)
        self.press_label.setFont(font_label)
        self.press_label.setAlignment(QtCore.Qt.AlignCenter)

        self.hum_label = QtWidgets.QLabel("Humidity: -- %", self)
        self.hum_label.setFont(font_label)
        self.hum_label.setAlignment(QtCore.Qt.AlignCenter)

        # Layout
        layout = QtWidgets.QVBoxLayout()
        layout.setSpacing(30)
        layout.addStretch()
        layout.addWidget(self.temp_label)
        layout.addWidget(self.press_label)
        layout.addWidget(self.hum_label)
        layout.addStretch()
        self.setLayout(layout)

        # Timer for live update
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_values)
        self.timer.start(500)  # update every 500 ms

    def update_values(self):
        try:
            raw = ser.readline().decode('utf-8', errors='ignore').strip()
            if not raw:
                return

            parts = raw.split(',')
            if len(parts) != 3:
                return  # malformed line

            t_c = float(parts[0].strip())
            t_f = t_c * 9 / 5 + 32  # Convert to Fahrenheit
            p = float(parts[1].strip())
            h = float(parts[2].strip())

            # Update GUI labels
            self.temp_label.setText(f"Temperature: {t_f:.2f} °F")
            self.press_label.setText(f"Pressure: {p:.2f} hPa")
            self.hum_label.setText(f"Humidity: {h:.2f} %")

            # Also print to terminal
            print(f"Temp: {t_f:.2f} °F | Pressure: {p:.2f} hPa | Humidity: {h:.2f} %")

        except ValueError:
            pass  # ignore parse errors


# ------------------------- RUN APP ---------------------------------
app = QtWidgets.QApplication(sys.argv)
window = BME280Window()
window.show()

try:
    sys.exit(app.exec_())
finally:
    ser.close()
    print("Serial port closed.")

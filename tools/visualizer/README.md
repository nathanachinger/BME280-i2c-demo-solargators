# BME280 Visualizer

Desktop window showing live temperature, pressure, and humidity from the board over UART.

The firmware must print lines like `23.45,1013.25,40.12` (°C, hPa, %) at 115200 baud.

## Setup

```bash
cd tools/visualizer
python3 -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

## Run

```bash
python bme280_visualizer.py /dev/tty.usbmodem1103   # macOS (find yours with: ls /dev/tty.usbmodem*)
python bme280_visualizer.py COM10                   # Windows
```

If you omit the port, the script lists the available ports and uses the first one.
An optional second argument sets the baud rate.

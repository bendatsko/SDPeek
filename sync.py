import sys
import os
import serial
import time
import glob
import select
from tqdm import tqdm

DEFAULT_SYNC_DIR = "sync_dir"
DEFAULT_REMOTE_DIR = "/SYNC"


def find_teensy_port():
    if sys.platform.startswith("darwin"):
        ports = glob.glob("/dev/cu.usbmodem*")
    elif sys.platform.startswith("win"):
        ports = [f"COM{i + 1}" for i in range(256)]
    elif sys.platform.startswith("linux") or sys.platform.startswith("cygwin"):
        ports = glob.glob("/dev/ttyACM*")
    else:
        raise EnvironmentError("Unsupported platform")

    for port in ports:
        try:
            with serial.Serial(port, 2000000, timeout=2) as ser:
                ser.write(b"\n")
                time.sleep(0.1)
                response = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
                if ">" in response:
                    return port
        except (OSError, serial.SerialException):
            pass
    return None


def wait_for_teensy_ready(ser):
    print("Waiting for Teensy to be ready...")
    ser.write(b"\n")
    start_time = time.time()
    while time.time() - start_time < 5:  # Wait up to 5 seconds
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode("utf-8", errors="ignore")
            if ">" in response:
                print("Teensy is ready.")
                return True
        time.sleep(0.1)
    return False


def send_file(ser, local_path, remote_path):
    file_size = os.path.getsize(local_path)
    ser.write(f"FILE:{os.path.basename(local_path)}\n".encode())
    ser.write(f"{file_size}\n".encode())
    time.sleep(0.1)

    print(f"Sending file: {local_path}, size: {file_size}")

    with open(local_path, "rb") as file:
        chunk_size = 512
        while True:
            chunk = file.read(chunk_size)
            if not chunk:
                break
            ser.write(chunk)
            # time.sleep(0.01)  # Small delay between chunks

    response = ser.readline().decode().strip()
    # if response != "FILE_RECEIVED":
    # print(f"Error sending file {local_path}")
    # return False
    print(f"Sent {len(chunk)} bytes")

    return True


def sync_directory(ser, local_path, remote_path):
    ser.write(f"syncdir {remote_path}\n".encode())
    response = ser.readline().decode().strip()
    if response != "Ready to receive files. Start transfer from host.":
        print(f"Error: Teensy not ready for sync. Response: {response}")
        return

    files = [
        os.path.join(root, file)
        for root, _, files in os.walk(local_path)
        for file in files
    ]

    ser.write(f"FILE_COUNT:{len(files)}\n".encode())

    success_count = 0
    for file in tqdm(files, desc="Syncing files", unit="file"):
        relative_path = os.path.relpath(file, local_path)
        remote_file_path = os.path.join(remote_path, relative_path).replace("\\", "/")
        if send_file(ser, file, remote_file_path):
            success_count += 1

    ser.write("SYNC_COMPLETE\n".encode())
    print(
        f"Sync completed. Successfully synced {success_count} out of {len(files)} files."
    )


def open_serial_monitor(port, baud_rate, local_dir):
    try:
        with serial.Serial(port, baud_rate, timeout=0.1) as ser:
            print(f"Serial monitor opened on {port} at {baud_rate} baud")
            print("Type 'exit' to close the monitor.")

            ser.write(b"banner\n")
            time.sleep(0.1)  # Give the Teensy time to respond

            while True:
                if ser.in_waiting:
                    print(
                        ser.read(ser.in_waiting).decode("utf-8", errors="replace"),
                        end="",
                        flush=True,
                    )
                if select.select([sys.stdin], [], [], 0)[0]:
                    cmd = input()
                    if cmd.lower() == "exit":
                        break
                    elif cmd.lower() == "resync":
                        print("Resyncing files...")
                        sync_directory(ser, local_dir, DEFAULT_REMOTE_DIR)
                    else:
                        ser.write((cmd + "\n").encode())
    except KeyboardInterrupt:
        print("\nSerial monitor closed")
    except serial.SerialException as e:
        print(f"Error: Unable to open serial port: {e}")


if __name__ == "__main__":
    port = find_teensy_port()
    if not port:
        print("Error: Teensy not found. Make sure it's connected and running SDPeek.")
        sys.exit(1)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    local_dir = os.path.join(script_dir, DEFAULT_SYNC_DIR)

    if not os.path.exists(local_dir):
        os.makedirs(local_dir)
        print(f"Created sync directory: {local_dir}")

    print(f"Connecting to Teensy on {port}...")

    try:
        with serial.Serial(port, 2000000, timeout=1) as ser:
            if wait_for_teensy_ready(ser):
                print(
                    f"Syncing files from {local_dir} to {DEFAULT_REMOTE_DIR} on Teensy..."
                )
                sync_directory(ser, local_dir, DEFAULT_REMOTE_DIR)
            else:
                print("Error: Teensy did not respond as expected.")
                sys.exit(1)
    except serial.SerialException as e:
        print(f"Error: Unable to open serial port: {e}")
        sys.exit(1)

    print("Opening serial monitor...")
    open_serial_monitor(port, 2000000, local_dir)

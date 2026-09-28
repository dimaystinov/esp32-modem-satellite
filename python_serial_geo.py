import serial
from serial.tools import list_ports
import struct
import time
import binascii
from pathlib import Path


BAUD = 115200


FULL_FRAME_HEX = None

STARTB = 0x79
STOPB = 0x47
CMD = 0x01

WAIT_DEVICE_AFTER_OPEN = 4.0
WAIT_BETWEEN_REOPEN_SEC = 2.0

WAIT_ECHO_START_SEC = 5.0
IDLE_AFTER_ECHO_SEC = 1.0

CHUNK_SIZE = 64
CHUNK_DELAY_SEC = 0.02

READ_TIMEOUT_SEC = 0.2
WRITE_TIMEOUT_SEC = 8.0
OPEN_ATTEMPTS = 2

SCRIPT_DIR = Path(__file__).resolve().parent


def crc16_ibm_sdlc(data: bytes, init: int = 0xFFFF) -> int:
    crc = init
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame_from_body(body: bytes) -> bytes:
    size = len(body)
    head = bytes([STARTB, CMD]) + struct.pack("<H", size)
    crc = crc16_ibm_sdlc(head + body)
    return head + body + struct.pack("<H", crc) + bytes([STOPB])


def list_com_ports():
    ports = list(list_ports.comports())
    if not ports:
        print("COM ports not found", flush=True)
        return []
    print("Available COM ports:", flush=True)
    for i, p in enumerate(ports):
        print(f"{i}: {p.device} ({p.description})", flush=True)
    return ports


def choose_port():
    ports = list_com_ports()
    if not ports:
        return None

    if len(ports) == 1:
        print(f"Using single port: {ports[0].device}", flush=True)
        return ports[0].device

    while True:
        s = input("Select COM port index: ").strip()
        if not s.isdigit():
            print("Please enter a number", flush=True)
            continue
        idx = int(s)
        if 0 <= idx < len(ports):
            return ports[idx].device
        print(f"Index out of range 0..{len(ports)-1}", flush=True)


def list_waypoint_files():
    files = sorted(SCRIPT_DIR.glob("*.waypoints"))
    if not files:
        print(f"No .waypoints files found in: {SCRIPT_DIR}", flush=True)
        return []

    print("Available .waypoints files:", flush=True)
    for i, p in enumerate(files):
        try:
            size = p.stat().st_size
        except Exception:
            size = -1
        size_info = f"{size} bytes" if size >= 0 else "size n/a"
        print(f"{i}: {p.name} ({size_info})", flush=True)
    return files


def choose_waypoint_file():
    files = list_waypoint_files()
    if not files:
        return None

    if len(files) == 1:
        print(f"Using single waypoint file: {files[0].name}", flush=True)
        return files[0]

    while True:
        s = input("Select waypoint file index: ").strip()
        if not s.isdigit():
            print("Please enter a number", flush=True)
            continue
        idx = int(s)
        if 0 <= idx < len(files):
            return files[idx]
        print(f"Index out of range 0..{len(files)-1}", flush=True)


def load_body_from_waypoint_file(path: Path) -> bytes:
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig")

    if not text.endswith("\n"):
        text += "\n"

    body = text.encode("utf-8")
    line_count = len([ln for ln in text.splitlines() if ln.strip()])
    print(
        f"Selected mission: {path.name}, lines={line_count}, body_len={len(body)}",
        flush=True,
    )
    return body


def open_serial(port_name: str) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port_name
    ser.baudrate = BAUD
    ser.timeout = READ_TIMEOUT_SEC
    ser.write_timeout = WRITE_TIMEOUT_SEC
    ser.rtscts = False
    ser.dsrdtr = False
    ser.xonxoff = False

    print(f"\nOpening port {port_name} @ {BAUD}...", flush=True)
    ser.open()
    print("Port open:", ser.is_open, flush=True)

    try:
        ser.dtr = False
        ser.rts = False
        print("DTR/RTS lowered", flush=True)
    except Exception as e:
        print(f"DTR/RTS set failed: {e}", flush=True)

    print(f"Waiting for device ready {WAIT_DEVICE_AFTER_OPEN:.1f}s...", flush=True)
    time.sleep(WAIT_DEVICE_AFTER_OPEN)

    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print("Port buffers cleared", flush=True)
    except Exception as e:
        print(f"Buffer reset failed: {e}", flush=True)

    return ser


def send_chunked(ser, data: bytes, chunk_size=64, delay_sec=0.02):
    total = 0

    for i in range(0, len(data), chunk_size):
        chunk = data[i:i + chunk_size]
        chunk_no = i // chunk_size

        print(f"TX chunk {chunk_no}: len={len(chunk)} total_before={total}", flush=True)

        n = ser.write(chunk)

        total += n
        print(f"TX chunk {chunk_no}: written={n} total_after={total}", flush=True)

        if n != len(chunk):
            raise serial.SerialTimeoutException(
                f"partial write on chunk {chunk_no}: {n} of {len(chunk)}"
            )

        time.sleep(delay_sec)

    return total


def read_until_idle(ser, wait_start_sec=5.0, idle_sec=1.0) -> bytes:
    data = bytearray()

    print(f"Waiting response start up to {wait_start_sec:.1f}s...", flush=True)
    start = time.time()

    while time.time() - start < wait_start_sec:
        try:
            n = ser.in_waiting
        except Exception:
            n = 0

        if n > 0:
            chunk = ser.read(n)
            if chunk:
                data.extend(chunk)
                print(f"Response started, bytes: {len(data)}", flush=True)
                break

        time.sleep(0.01)

    if not data:
        print("Response did not start", flush=True)
        return bytes(data)

    print(f"Reading until idle for {idle_sec:.1f}s...", flush=True)
    last_data_time = time.time()

    while True:
        try:
            n = ser.in_waiting
        except Exception:
            n = 0

        if n > 0:
            chunk = ser.read(n)
            if chunk:
                data.extend(chunk)
                last_data_time = time.time()
                print(f"RX +{len(chunk)} bytes, total={len(data)}", flush=True)
        else:
            if time.time() - last_data_time >= idle_sec:
                break
            time.sleep(0.01)

    return bytes(data)


def do_one_session(port_name: str, frame: bytes):
    ser = None
    try:
        ser = open_serial(port_name)

        print(f"Sending in chunks of {CHUNK_SIZE} bytes...", flush=True)
        sent = send_chunked(ser, frame, CHUNK_SIZE, CHUNK_DELAY_SEC)
        print("Sent bytes:", sent, flush=True)

        answer = read_until_idle(
            ser,
            wait_start_sec=WAIT_ECHO_START_SEC,
            idle_sec=IDLE_AFTER_ECHO_SEC
        )

        print("Total received bytes:", len(answer), flush=True)
        print("ANSWER HEX:", answer.hex(), flush=True)

        if len(answer) == len(frame):
            print("Response length matches sent frame", flush=True)
        else:
            print(f"Partial response: {len(answer)} of {len(frame)} bytes", flush=True)

        return True

    finally:
        if ser is not None and ser.is_open:
            time.sleep(100)
            ser.close()
            print("Port closed", flush=True)


def main():
    if FULL_FRAME_HEX:
        frame = binascii.unhexlify(FULL_FRAME_HEX)
    else:
        mission_file = choose_waypoint_file()
        if mission_file is None:
            print("Select a .waypoints mission file before sending.", flush=True)
            return
        else:
            body = load_body_from_waypoint_file(mission_file)
        frame = build_frame_from_body(body)

    print("Frame len:", len(frame), flush=True)
    print("Frame HEX:", frame.hex(), flush=True)

    port_name = choose_port()
    if not port_name:
        return

    last_error = None

    for attempt in range(1, OPEN_ATTEMPTS + 1):
        print(f"\n=== Attempt {attempt}/{OPEN_ATTEMPTS} ===", flush=True)
        try:
            ok = do_one_session(port_name, frame)
            if ok:
                return
        except serial.SerialTimeoutException as e:
            last_error = e
            print("SerialTimeoutException:", e, flush=True)
        except serial.SerialException as e:
            last_error = e
            print("SerialException:", e, flush=True)
        except Exception as e:
            last_error = e
            print("Exception:", repr(e), flush=True)

        if attempt < OPEN_ATTEMPTS:
            print(f"Waiting before retry {WAIT_BETWEEN_REOPEN_SEC:.1f}s...", flush=True)
            time.sleep(WAIT_BETWEEN_REOPEN_SEC)

    print("\nAll attempts exhausted.", flush=True)
    if last_error is not None:
        print("Last error:", repr(last_error), flush=True)


if __name__ == "__main__":
    main()

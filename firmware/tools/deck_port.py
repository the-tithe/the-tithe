"""Find and open the Deck's USB serial port without resetting it."""
import glob, os, termios, time

import serial


def find_port():
    if os.environ.get("DECK_PORT"):
        return os.environ["DECK_PORT"]
    ports = sorted(glob.glob("/dev/cu.usbmodem*")) + sorted(glob.glob("/dev/ttyACM*"))
    if not ports:
        raise SystemExit("No Cardputer found. Plug it in over USB, or set DECK_PORT=/dev/cu.usbmodem...")
    return ports[0]


def open_port(timeout=10):
    s = serial.Serial()
    s.port = find_port()
    s.baudrate = 115200
    s.timeout = timeout
    s.dtr = True
    s.rts = True
    s.open()
    # Closing the port must not reset the Cardputer.
    attrs = termios.tcgetattr(s.fd)
    attrs[2] &= ~termios.HUPCL
    termios.tcsetattr(s.fd, termios.TCSANOW, attrs)
    # Throw away anything left over from an earlier, interrupted session:
    # read until the Deck has been quiet for a moment.
    s.timeout = 0.2
    while s.read(4096):
        pass
    s.timeout = timeout
    return s


def read_line(s):
    line = s.readline()
    if not line:
        raise TimeoutError("the Deck didn't answer")
    return line.decode("latin1").rstrip("\r\n")


def command(s, text):
    """Send a console command; return (lines before the ok/err line, that line)."""
    s.write((text + "\n").encode())
    lines = []
    while True:
        line = read_line(s)
        if line.startswith("ok") or line.startswith("err"):
            return lines, line
        lines.append(line)

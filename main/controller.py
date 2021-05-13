import serial
import time
from sys import stdout


# uart propreties
PORT = "COM7"
BAUD = 115200
TIMEOUT = 1  # sec


s = serial.Serial(port=PORT, baudrate=BAUD, timeout=TIMEOUT)


def writer(data: str):
    if data == "":
        # user entered not data.
        # happens when just pressing "ENTER" key, to pass input function
        data = " "  # instead insert a SPACE character (0x20)
    s.write(bytes(data, "utf-8"))
    s.flush()


def reader():
    """

    """
    recv = "0"

    while recv != "":
        try:
            recv = s.readline()  # read a '\n' terminated line or timeout
            recv = recv.decode("cp1252")  # decodes utf-8 and more
            stdout.write(recv)
            stdout.flush()

            if ">" in recv:  # user input is required
                return True
        
        except serial.SerialException:
            print("Reader TimeoutError")
            break
    return None
    


def init_arduino():
    """
    read arduino "start message" and send a start signal
    """
    # wait for arduino to send start message
    while reader() is None:
        pass
    # wait for user to send a start char to Arduino
    while input() != "s":
        pass
    writer("s")


def main():
    s.reset_input_buffer()
    s.reset_output_buffer()
    s.flushInput()
    s.flushOutput()

    # Starting Arduino
    init_arduino()
    
    # Entering Arduino's main menu
    while True:
        r = reader()
        if r is True:
            command = input()
            writer(command)

        if command == 'z':
            reader()
            break    
    s.close()

if __name__ == "__main__":
    main()

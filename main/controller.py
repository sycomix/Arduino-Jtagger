import serial
import time


# uart propreties
PORT = "COM7"
BAUD = 115200
TIMEOUT = 1  # sec


s = serial.Serial(port=PORT, baudrate=BAUD, timeout=TIMEOUT)


def writer(data: str):
    print(data)
    print(type(data))
    s.write(bytes(data, 'utf-8'))
    s.flush()


def reader():
    """

    """
    recv = "0"
    while recv:
        try:
            recv = s.readline()
            print(recv)
            if ">" in recv:  # user input is required
                return True
        
        except TimeoutError:
            break
    return None
    


def init_arduino():
    """
    read arduino "start message" and send a start signal
    """
    print(s.readlines(2))
    if input(">") == "s":
        write("s")


def main():
    s.reset_input_buffer()
    s.reset_output_buffer()
    init_arduino()
    
    # Entering Arduino's main menu
    while True:
        if reader() is not None:
            input()

    
    

    s.close()

if __name__ == "__main__":
    main()

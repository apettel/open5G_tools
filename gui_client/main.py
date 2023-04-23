from PyQt5 import QtWidgets, QtCore
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QMessageBox
import socket

class MyWindow(QtWidgets.QWidget):
    def __init__(self):
        self.timer = QTimer()
        self.timer.setInterval(100)    
        super().__init__()
        self.mem_read_items = list()
        self.initUI()

    def initUI(self):
        # create a grid layout with 10 rows and 2 columns
        layout = QtWidgets.QGridLayout(self)

        group_box = QtWidgets.QGroupBox("Settings")
        group_box_layout = QtWidgets.QGridLayout(group_box)

        label = QtWidgets.QLabel(f"ip")
        group_box_layout.addWidget(label, 0, 0) 
        self.edit_box_ip = QtWidgets.QLineEdit()
        self.edit_box_ip.setText('192.168.137.2')        
        group_box_layout.addWidget(self.edit_box_ip, 0, 1)

        label = QtWidgets.QLabel(f"port")
        group_box_layout.addWidget(label, 1, 0)
        self.edit_box_port = QtWidgets.QLineEdit()     
        self.edit_box_port.setText('69')        
        group_box_layout.addWidget(self.edit_box_port, 1, 1)

        label = QtWidgets.QLabel(f"update interval (ms)")
        group_box_layout.addWidget(label, 0, 2)
        self.edit_box_upd = QtWidgets.QLineEdit()
        self.edit_box_upd.setText('50')
        self.edit_box_upd.textChanged.connect(self.upd_changed)
        group_box_layout.addWidget(self.edit_box_upd, 0, 3)

        self.button_connect = QtWidgets.QPushButton("connect")
        self.button_connect.clicked.connect(self.connect_socket)
        group_box_layout.addWidget(self.button_connect, 2, 0)

        self.button_disconnect = QtWidgets.QPushButton("disconnect")
        self.button_disconnect.setEnabled(False)
        self.button_disconnect.clicked.connect(self.disconnect_socket)
        group_box_layout.addWidget(self.button_disconnect, 2, 1)
        layout.addWidget(group_box, 0, 0, 1, 2)                

        # create PSS detector group box
        group_box = QtWidgets.QGroupBox("PSS detector")

        # create a grid layout inside the group box with 10 rows and 2 columns
        group_box_layout = QtWidgets.QGridLayout(group_box)

        self.add_read_location("id string",     0x7c44400c, group_box_layout)
        self.add_read_location("peak count 0",  0x7c444020, group_box_layout)
        self.add_read_location("peak count 1",  0x7c444024, group_box_layout)
        self.add_read_location("peak count 2",  0x7c444028, group_box_layout)
        self.add_read_location("mode",          0x7c444014, group_box_layout)
        self.add_read_location("CFO mode",      0x7c44401c, group_box_layout)
        self.add_read_location("CFO (Hz)",      0x7c444018, group_box_layout)
        # self.add_read_location("N_id_2",        0x7c444010, group_box_layout)

        group_box.setLayout(group_box_layout)

        # add the group box to the main layout
        layout.addWidget(group_box, 1, 0, 1, 1)

        # create Frame sync group box
        group_box = QtWidgets.QGroupBox("rx core")

        # create a grid layout inside the group box with 10 rows and 2 columns
        group_box_layout = QtWidgets.QGridLayout(group_box)

        self.add_read_location("id string", 0x7c44800c, group_box_layout)
        self.add_read_location("fs state",  0x7c448014, group_box_layout)
        self.add_read_location("rx signal", 0x7c448018, group_box_layout)

        group_box.setLayout(group_box_layout)

        # add the group box to the main layout
        layout.addWidget(group_box, 1, 1, 1, 2)        

        self.setLayout(layout)

    def upd_changed(self):
        try:
            self.timer.setInterval(int(self.edit_box_upd.text()))
        except:
            pass

    def add_read_location(self, label, addr, group_box_layout):
        label = QtWidgets.QLabel(label)
        edit_box = QtWidgets.QLineEdit()
        edit_box.setEnabled(False)
        idx = len(self.mem_read_items)
        group_box_layout.addWidget(label, idx, 0)
        group_box_layout.addWidget(edit_box, idx, 1)
        self.mem_read_items.append((addr, edit_box))

    def connect_socket(self):
        ip = self.edit_box_ip.text()
        port = int(self.edit_box_port.text())

        # Connect to the socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(1.0)

        try:
            # connect to server
            self.sock.connect((ip, port))
            print('Connection established')
            self.setWindowTitle("connected")
            self.button_connect.setEnabled(False)
            self.button_disconnect.setEnabled(True)
            # Start a timer to receive data from the socket every 100ms
            self.timer.timeout.connect(self.update_data)
            self.timer.start()
        except socket.timeout:
            QMessageBox.warning(self, '','Connection timed out')
        except ConnectionRefusedError:
            QMessageBox.warning(self, '','Connection refused')


    def disconnect_socket(self):
        try:
            self.timer.stop()
            self.sock.close()
            print('Connection closed')
            self.button_connect.setEnabled(True)
            self.button_disconnect.setEnabled(False)
        except:
            print('Error closing connection')

    def update_data(self):
        # assembly request message
        num_items = len(self.mem_read_items)
        num_read_bytes = 5 + 4*num_items
        import numpy as np
        req_msg = np.empty(num_read_bytes, np.int8)
        req_msg[0] = ((num_read_bytes - 4) >> 24) & 0xFF
        req_msg[1] = ((num_read_bytes - 4) >> 16) & 0xFF
        req_msg[2] = ((num_read_bytes - 4) >> 8) & 0xFF
        req_msg[3] = (num_read_bytes - 4) & 0xFF
        req_msg[4] = 0 # mode 0 is read
        for i in range(num_items):
            addr = self.mem_read_items[i][0]
            req_msg[5 + i*4] = (addr >> 24) & 0xFF
            req_msg[6 + i*4] = (addr >> 16) & 0xFF
            req_msg[7 + i*4] = (addr >> 8) & 0xFF
            req_msg[8 + i*4] = (addr >> 0) & 0xFF
        # send and wait for answer
        self.sock.send(req_msg)
        recv_msg = self.sock.recv(num_read_bytes)
        # fill GUI
        for i in range(num_items):
            val = recv_msg[5 + i*4] << 24
            val = recv_msg[6 + i*4] << 16
            val = recv_msg[7 + i*4] << 8
            val = recv_msg[8 + i*4] << 0
            if (self.mem_read_items[i][1] & 0xFF == 0x0c):
                self.mem_read_items[i][1].setText(f'{val:c}{val>>8:c}{val>>16:c}{val>>24:c}')
            else:
                self.mem_read_items[i][1].setText(f'{val:08x}')

if __name__ == "__main__":
    import sys
    app = QtWidgets.QApplication(sys.argv)
    window = MyWindow()
    window.show()
    sys.exit(app.exec_())

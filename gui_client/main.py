import numpy as np
from PyQt5 import QtWidgets, QtCore
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QMessageBox
from PyQt5.QtCore import Qt
import socket

class MyWindow(QtWidgets.QWidget):
    def __init__(self):
        self.first_read = True
        self.timer = QTimer()
        self.timer.setInterval(100)    
        super().__init__()
        self.mem_read_items = list()
        self.initUI()

    def closeEvent(self, event):
        self.disconnect_socket()
        event.accept()

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
        self.edit_box_upd.setText('100')
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
        self.add_read_location("noise limit",   0x7c44402c, group_box_layout)
        self.add_read_location("detection shift", 0x7c444030, group_box_layout)
        self.add_read_location("mode",          0x7c444014, group_box_layout)
        self.add_read_location("CFO mode",      0x7c44401c, group_box_layout)
        self.add_read_location("CFO (Hz)",      0x7c444018, group_box_layout)
        
        
        label = QtWidgets.QLabel("CFO mode")
        self.combo_box_cfo = QtWidgets.QComboBox()
        self.combo_box_cfo.addItem("auto (0)")
        self.combo_box_cfo.addItem("manual (1)")
        self.combo_box_cfo.currentIndexChanged.connect(self.set_CFO_mode)
        idx = len(self.mem_read_items)
        group_box_layout.addWidget(label, idx, 0)
        group_box_layout.addWidget(self.combo_box_cfo, idx, 1)

        label = QtWidgets.QLabel("detection shift")
        self.combo_box_ds = QtWidgets.QComboBox()
        self.combo_box_ds.addItem("1")
        self.combo_box_ds.addItem("2")
        self.combo_box_ds.addItem("3")
        self.combo_box_ds.addItem("4")
        self.combo_box_ds.addItem("5")
        self.combo_box_ds.addItem("6")
        self.combo_box_ds.currentIndexChanged.connect(self.set_detection_shift)
        idx = len(self.mem_read_items) + 1
        group_box_layout.addWidget(label, idx, 0)
        group_box_layout.addWidget(self.combo_box_ds, idx, 1)

        label = QtWidgets.QLabel("noise limit")
        self.slider = QtWidgets.QSlider(Qt.Horizontal)
        self.slider.setRange(0, 100)
        self.slider.valueChanged.connect(self.set_noise_limit)
        idx = len(self.mem_read_items) + 2
        group_box_layout.addWidget(label, idx, 0)
        group_box_layout.addWidget(self.slider, idx, 1)

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
        self.add_read_location("N_id_2",    0x7c44801c, group_box_layout)
        self.add_read_location("N_id",      0x7c448020, group_box_layout)
        self.add_read_location("sample_cnt_mismatch",      0x7c448024, group_box_layout)

        group_box.setLayout(group_box_layout)

        # add the group box to the main layout
        layout.addWidget(group_box, 1, 1, 1, 2)        

        self.setLayout(layout)

    def upd_changed(self):
        try:
            self.timer.setInterval(int(self.edit_box_upd.text()))
        except:
            pass

    def set_noise_limit(self, value):
        noise_limit = int(2**(value/100*32))
        num_write_bytes = 13
        req_msg = np.empty(num_write_bytes, np.int8)
        req_msg[0] = ((num_write_bytes - 4) >> 0) & 0xFF
        req_msg[1] = ((num_write_bytes - 4) >> 8) & 0xFF
        req_msg[2] = ((num_write_bytes - 4) >> 16) & 0xFF
        req_msg[3] = ((num_write_bytes - 4) >> 24) & 0xFF
        req_msg[4] = 1 # mode 1 is write
        req_msg[5] = 0x2c
        req_msg[6] = 0x40
        req_msg[7] = 0x44
        req_msg[8] = 0x7c
        req_msg[9] = noise_limit & 0xFF
        req_msg[10] = (noise_limit >> 8) & 0xFF
        req_msg[11] = (noise_limit >> 16) & 0xFF
        req_msg[12] = (noise_limit >> 24) & 0xFF
        print("send " + ":".join("{:02x}".format(c) for c in req_msg.tobytes()))
        self.sock.send(req_msg.tobytes())        
    
    def set_CFO_mode(self, idx):
        if self.button_connect.isEnabled() == False:
            num_write_bytes = 13
            req_msg = np.empty(num_write_bytes, np.int8)
            req_msg[0] = ((num_write_bytes - 4) >> 0) & 0xFF
            req_msg[1] = ((num_write_bytes - 4) >> 8) & 0xFF
            req_msg[2] = ((num_write_bytes - 4) >> 16) & 0xFF
            req_msg[3] = ((num_write_bytes - 4) >> 24) & 0xFF
            req_msg[4] = 1 # mode 1 is write
            req_msg[5] = 0x1c
            req_msg[6] = 0x40
            req_msg[7] = 0x44
            req_msg[8] = 0x7c
            req_msg[9] = idx
            req_msg[10] = 0
            req_msg[11] = 0
            req_msg[12] = 0
            print("send " + ":".join("{:02x}".format(c) for c in req_msg.tobytes()))
            self.sock.send(req_msg.tobytes())

    def set_detection_shift(self, idx):
        if self.button_connect.isEnabled() == False:
            num_write_bytes = 13
            req_msg = np.empty(num_write_bytes, np.int8)
            req_msg[0] = ((num_write_bytes - 4) >> 0) & 0xFF
            req_msg[1] = ((num_write_bytes - 4) >> 8) & 0xFF
            req_msg[2] = ((num_write_bytes - 4) >> 16) & 0xFF
            req_msg[3] = ((num_write_bytes - 4) >> 24) & 0xFF
            req_msg[4] = 1 # mode 1 is write
            req_msg[5] = 0x30
            req_msg[6] = 0x40
            req_msg[7] = 0x44
            req_msg[8] = 0x7c
            req_msg[9] = idx + 1
            req_msg[10] = 0
            req_msg[11] = 0
            req_msg[12] = 0
            print("send " + ":".join("{:02x}".format(c) for c in req_msg.tobytes()))
            self.sock.send(req_msg.tobytes())

    def add_read_location(self, label, addr, group_box_layout, signed = False):
        label = QtWidgets.QLabel(label)
        edit_box = QtWidgets.QLineEdit()
        edit_box.setReadOnly(True)
        idx = len(self.mem_read_items)
        group_box_layout.addWidget(label, idx, 0)
        group_box_layout.addWidget(edit_box, idx, 1)
        self.mem_read_items.append((addr, edit_box, signed))

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

            # self.sock.settimeout(0.01)
            # for i in range(100):
            #     try:
            #         self.sock.recv(1)
            #     except:
            #         pass
            self.sock.settimeout(1)

            self.button_connect.setEnabled(False)
            self.button_disconnect.setEnabled(True)
            # Start a timer to receive data from the socket every 100ms
            self.timer.timeout.connect(self.update_data)
            self.first_read = True
            self.timer.start()
        except socket.timeout:
            QMessageBox.warning(self, '','Connection timed out')
        except ConnectionRefusedError:
            QMessageBox.warning(self, '','Connection refused')

    def disconnect_socket(self):
        try:
            self.timer.stop()
            self.sock.shutdown(socket.SHUT_RDWR)
            self.sock.close()
            print('Connection closed')
            self.button_connect.setEnabled(True)
            self.button_disconnect.setEnabled(False)
        except:
            print('Error closing connection')

    def twos_comp(self, val, bits):
        """compute the 2's complement of int value val"""
        if (val & (1 << (bits - 1))) != 0: # if sign bit is set e.g., 8bit: 128-255
            val = val - (1 << bits)        # compute negative value
        return val        

    def update_data(self):
        # assembly request message
        num_items = len(self.mem_read_items)
        num_read_bytes = 5 + 4*num_items
        req_msg = np.empty(num_read_bytes, np.int8)
        req_msg[0] = ((num_read_bytes - 4) >> 0) & 0xFF
        req_msg[1] = ((num_read_bytes - 4) >> 8) & 0xFF
        req_msg[2] = ((num_read_bytes - 4) >> 16) & 0xFF
        req_msg[3] = ((num_read_bytes - 4) >> 24) & 0xFF
        req_msg[4] = 0 # mode 0 is read
        for i in range(num_items):
            addr = self.mem_read_items[i][0]
            req_msg[5 + i*4] = (addr >> 0) & 0xFF
            req_msg[6 + i*4] = (addr >> 8) & 0xFF
            req_msg[7 + i*4] = (addr >> 16) & 0xFF
            req_msg[8 + i*4] = (addr >> 24) & 0xFF
        # send and wait for answer
        # print("send " + ":".join("{:02x}".format(c) for c in req_msg.tobytes()))
        self.sock.send(req_msg.tobytes())
        recv_msg = self.sock.recv(num_read_bytes)
        if len(recv_msg) < num_read_bytes: # sometimes not all data comes with first read
            recv_msg += self.sock.recv(num_read_bytes - len(recv_msg))
        # print(f"receive {len(recv_msg)} bytes -> " + ":".join("{:02x}".format(c) for c in recv_msg))
        # fill GUI
        for i in range(num_items):
            val = int(recv_msg[5 + i*4] << 0)
            val += recv_msg[6 + i*4] << 8
            val += recv_msg[7 + i*4] << 16
            val += recv_msg[8 + i*4] << 24
            if (self.mem_read_items[i][0] & 0xFF == 0x0c):
                val_str = recv_msg[5 + i*4 : 9 + i*4].decode("ascii")[::-1]
                self.mem_read_items[i][1].setText(val_str)
            else:
                if self.mem_read_items[i][2]:
                    self.mem_read_items[i][1].setText(f'{self.twos_comp(val, 8)}')
                else:
                    self.mem_read_items[i][1].setText(f'{val:08x}')
        if self.first_read:
            # set noise_filter slider
            for item in self.mem_read_items:
                if item[0] == 0x7c44402c:
                    value = int(item[1].text(), base = 16)
                    break
            if value == 0:
                self.slider.setValue(0)
            else:
                self.slider.setValue(int(np.log2(value)*100/32))
            # set CFO_mode combobox
            for item in self.mem_read_items:
                if item[0] == 0x7c44401c:
                    idx = int(item[1].text(), base = 16)
                    break
            self.combo_box_cfo.setCurrentIndex(idx)

            # set detection shift combobox
            for item in self.mem_read_items:
                if item[0] == 0x7c444030:
                    idx = int(item[1].text(), base = 16) - 1
                    break
            self.combo_box_ds.setCurrentIndex(idx)
            
        self.first_read = False

if __name__ == "__main__":
    import sys
    app = QtWidgets.QApplication(sys.argv)
    window = MyWindow()
    window.show()
    sys.exit(app.exec_())

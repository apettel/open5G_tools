from PyQt5 import QtWidgets, QtCore
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QMessageBox
import socket

class MyWindow(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()
        #self.setupNetwork()
        #self.startTimer(100)  # start timer with 100ms interval

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

        # create 10 labels and add them to the first column of the group box
        self.labels = []
        for i in range(10):
            label = QtWidgets.QLabel(f"Label {i+1}")
            self.labels.append(label)
            group_box_layout.addWidget(label, i, 0)

        # create 10 edit boxes and add them to the second column of the group box
        self.edit_boxes = []
        for i in range(10):
            edit_box = QtWidgets.QLineEdit()
            self.edit_boxes.append(edit_box)
            group_box_layout.addWidget(edit_box, i, 1)

        group_box.setLayout(group_box_layout)

        # add the group box to the main layout
        layout.addWidget(group_box, 1, 0, 1, 1)

        # create Frame sync group box
        group_box = QtWidgets.QGroupBox("Frame sync")

        # create a grid layout inside the group box with 10 rows and 2 columns
        group_box_layout = QtWidgets.QGridLayout(group_box)

        # create 10 labels and add them to the first column of the group box
        self.labels = []
        for i in range(10):
            label = QtWidgets.QLabel(f"Label {i+1}")
            self.labels.append(label)
            group_box_layout.addWidget(label, i, 0)

        # create 10 edit boxes and add them to the second column of the group box
        self.edit_boxes = []
        for i in range(10):
            edit_box = QtWidgets.QLineEdit()
            self.edit_boxes.append(edit_box)
            group_box_layout.addWidget(edit_box, i, 1)

        group_box.setLayout(group_box_layout)

        # add the group box to the main layout
        layout.addWidget(group_box, 1, 1, 1, 2)        

        self.setLayout(layout)

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
        except socket.timeout:
            QMessageBox.warning(self, '','Connection timed out')
        except ConnectionRefusedError:
            QMessageBox.warning(self, '','Connection refused')

        # Start a timer to receive data from the socket every 100ms
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(100)

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
        pass

if __name__ == "__main__":
    import sys
    app = QtWidgets.QApplication(sys.argv)
    window = MyWindow()
    window.show()
    sys.exit(app.exec_())

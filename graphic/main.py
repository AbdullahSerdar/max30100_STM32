import sys
import time
from collections import deque

import serial
import serial.tools.list_ports

from PySide6.QtCore import QThread, Signal, QTimer, Qt
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

import pyqtgraph as pg


# ------------------------------------------------------------
# Parser
# STM32 format:
# MAX30100,bpm,spo2,valid,ppg_ir,ppg_red,raw_ir,timestamp_ms
# ------------------------------------------------------------
def parse_max30100_line(line: str):
    line = line.strip()

    if not line.startswith("MAX30100"):
        return None

    parts = line.split(",")

    if len(parts) != 8:
        return None

    try:
        return {
            "bpm": float(parts[1]),
            "spo2": float(parts[2]),
            "valid": int(float(parts[3])),
            "ppg_ir": float(parts[4]),
            "ppg_red": float(parts[5]),
            "raw_ir": int(float(parts[6])),
            "timestamp_ms": int(float(parts[7])),
        }
    except ValueError:
        return None


# ------------------------------------------------------------
# Serial reader thread
# ------------------------------------------------------------
class SerialReader(QThread):
    packet_received = Signal(dict)
    status_changed = Signal(str)

    def __init__(self, port: str, baudrate: int = 115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = False
        self.ser = None

    def run(self):
        self.running = True

        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.05,
            )
            self.ser.reset_input_buffer()
            self.status_changed.emit(f"Bağlandı: {self.port}")

        except Exception as e:
            self.status_changed.emit(f"Bağlantı hatası: {e}")
            return

        while self.running:
            try:
                raw_line = self.ser.readline()

                if not raw_line:
                    continue

                line = raw_line.decode(errors="ignore")
                packet = parse_max30100_line(line)

                if packet is not None:
                    self.packet_received.emit(packet)

            except Exception as e:
                self.status_changed.emit(f"Okuma hatası: {e}")
                time.sleep(0.1)

        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
        except Exception:
            pass

        self.status_changed.emit("Bağlantı kapatıldı")

    def stop(self):
        self.running = False
        self.wait(1000)


# ------------------------------------------------------------
# UI card
# ------------------------------------------------------------
class VitalCard(QFrame):
    def __init__(self, title: str, unit: str):
        super().__init__()

        self.setStyleSheet("""
            QFrame {
                background-color: white;
                border: 1px solid #d9e2ec;
                border-radius: 16px;
            }
            QLabel {
                color: #102a43;
                border: none;
            }
        """)

        self.title_label = QLabel(title)
        self.title_label.setAlignment(Qt.AlignCenter)
        self.title_label.setStyleSheet("font-size: 16px; font-weight: 600;")

        self.value_label = QLabel("--")
        self.value_label.setAlignment(Qt.AlignCenter)
        self.value_label.setStyleSheet("font-size: 44px; font-weight: 800;")

        self.unit_label = QLabel(unit)
        self.unit_label.setAlignment(Qt.AlignCenter)
        self.unit_label.setStyleSheet("font-size: 14px; color: #627d98;")

        layout = QVBoxLayout()
        layout.addWidget(self.title_label)
        layout.addWidget(self.value_label)
        layout.addWidget(self.unit_label)
        self.setLayout(layout)

    def set_value(self, text: str):
        self.value_label.setText(text)


# ------------------------------------------------------------
# Main window
# ------------------------------------------------------------
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("MAX30100 USB Monitor")
        self.resize(1200, 720)

        self.serial_thread = None

        self.max_points = 500
        self.x_data = deque(maxlen=self.max_points)
        self.ir_data = deque(maxlen=self.max_points)
        self.red_data = deque(maxlen=self.max_points)

        self.sample_counter = 0
        self.latest_packet = None
        self.packet_count = 0

        # ----------------------------------------------------
        # Contact / signal validation settings
        # ppg_ir / ppg_red → STM32'dan gelen filtreli değerler.
        # raw_ir           → STM32'dan gelen ham IR ADC değeri;
        #                    100'ün altındaysa parmak yok kabul edilir.
        # ----------------------------------------------------
        self.PPG_MIN_ABS_VALUE = 0.8
        self.SIGNAL_WINDOW_SIZE = 40
        self.MIN_PPG_VARIATION = 0.15

        # Ham IR eşiği: raw_ir bu değerin altındaysa anında NO FINGER.
        self.RAW_IR_MIN_VALUE = 100

        # IR / RED sıfıra yakınlık eşiği (filtreli kanal için ek kontrol).
        # Her iki kanal da bu eşiğin altındaysa parmak yok sayılır.
        self.NEAR_ZERO_THRESHOLD = 0.5

        # Tek bir kötü paket gelince resetleme.
        # Üst üste bu kadar kötü paket gelirse NO FINGER de.
        self.INVALID_LIMIT = 20
        self.invalid_counter = 0
        self.finger_present = False

        self.ir_signal_history = deque(maxlen=self.SIGNAL_WINDOW_SIZE)
        self.red_signal_history = deque(maxlen=self.SIGNAL_WINDOW_SIZE)

        self.build_ui()

        # Grafik güncellemesini seri okuma hızından ayırıyoruz.
        # Bu UI kasmasını azaltır.
        self.plot_timer = QTimer()
        self.plot_timer.timeout.connect(self.refresh_plot)
        self.plot_timer.start(33)  # yaklaşık 30 FPS

    def build_ui(self):
        root = QWidget()
        root.setStyleSheet("""
            QWidget {
                background-color: #f0f4f8;
                font-family: Segoe UI;
            }
            QPushButton {
                background-color: #102a43;
                color: white;
                border-radius: 10px;
                padding: 8px 16px;
                font-size: 14px;
                font-weight: 600;
            }
            QPushButton:disabled {
                background-color: #9fb3c8;
            }
            QComboBox {
                background-color: white;
                border: 1px solid #bcccdc;
                border-radius: 8px;
                padding: 6px;
                font-size: 14px;
            }
        """)

        self.port_combo = QComboBox()
        self.refresh_ports()

        self.refresh_button = QPushButton("Portları Yenile")
        self.connect_button = QPushButton("Bağlan")
        self.disconnect_button = QPushButton("Kes")
        self.disconnect_button.setEnabled(False)

        self.refresh_button.clicked.connect(self.refresh_ports)
        self.connect_button.clicked.connect(self.connect_serial)
        self.disconnect_button.clicked.connect(self.disconnect_serial)

        self.status_label = QLabel("Durum: Bağlı değil")
        self.status_label.setStyleSheet("font-size: 14px; color: #334e68;")

        top_bar = QHBoxLayout()
        top_bar.addWidget(QLabel("Port:"))
        top_bar.addWidget(self.port_combo)
        top_bar.addWidget(self.refresh_button)
        top_bar.addWidget(self.connect_button)
        top_bar.addWidget(self.disconnect_button)
        top_bar.addStretch()
        top_bar.addWidget(self.status_label)

        self.bpm_card = VitalCard("Heart Rate", "BPM")
        self.spo2_card = VitalCard("SpO₂", "%")
        self.valid_card = VitalCard("Signal", "STATUS")
        self.rate_card = VitalCard("Packets", "count")

        cards = QGridLayout()
        cards.addWidget(self.bpm_card, 0, 0)
        cards.addWidget(self.spo2_card, 0, 1)
        cards.addWidget(self.valid_card, 0, 2)
        cards.addWidget(self.rate_card, 0, 3)

        pg.setConfigOptions(antialias=False)

        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground("w")
        self.plot_widget.setTitle("PPG Waveform - Filtered IR / RED")
        self.plot_widget.setLabel("left", "Amplitude")
        self.plot_widget.setLabel("bottom", "Sample")
        self.plot_widget.showGrid(x=True, y=True, alpha=0.25)
        self.plot_widget.addLegend()

        self.ir_curve = self.plot_widget.plot(
            [],
            [],
            pen=pg.mkPen("#1f77b4", width=2),
            name="IR filtered",
        )

        self.red_curve = self.plot_widget.plot(
            [],
            [],
            pen=pg.mkPen("#d62728", width=2),
            name="RED filtered",
        )

        layout = QVBoxLayout()
        layout.addLayout(top_bar)
        layout.addLayout(cards)
        layout.addWidget(self.plot_widget, stretch=1)

        root.setLayout(layout)
        self.setCentralWidget(root)

    def refresh_ports(self):
        current = self.port_combo.currentText()
        self.port_combo.clear()

        ports = list(serial.tools.list_ports.comports())

        for port in ports:
            text = f"{port.device} - {port.description}"
            self.port_combo.addItem(text, port.device)

        # COM7 varsa otomatik seç.
        for i in range(self.port_combo.count()):
            if self.port_combo.itemData(i) == "COM7":
                self.port_combo.setCurrentIndex(i)
                return

        # Eski seçimi korumaya çalış.
        for i in range(self.port_combo.count()):
            if current and current.startswith(str(self.port_combo.itemData(i))):
                self.port_combo.setCurrentIndex(i)
                return

    def connect_serial(self):
        port = self.port_combo.currentData()

        if not port:
            self.status_label.setText("Durum: Port bulunamadı")
            return

        self.serial_thread = SerialReader(port=port, baudrate=115200)
        self.serial_thread.packet_received.connect(self.on_packet)
        self.serial_thread.status_changed.connect(self.on_status)
        self.serial_thread.start()

        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)

    def disconnect_serial(self):
        if self.serial_thread:
            self.serial_thread.stop()
            self.serial_thread = None

        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)

    def on_status(self, text: str):
        self.status_label.setText(f"Durum: {text}")

    # --------------------------------------------------------
    # Signal validation
    # --------------------------------------------------------
    def ir_red_near_zero(self, packet: dict) -> bool:
        """
        IR ve RED değerlerinin ikisi de NEAR_ZERO_THRESHOLD altındaysa
        sensörde sinyal yok demektir; parmak koyulmamış kabul edilir.
        """
        ir_near_zero = abs(packet["ppg_ir"]) < self.NEAR_ZERO_THRESHOLD
        red_near_zero = abs(packet["ppg_red"]) < self.NEAR_ZERO_THRESHOLD
        return ir_near_zero and red_near_zero

    def packet_looks_valid(self, packet: dict) -> bool:
        """
        Tek pakete göre kaba kontrol (filtreli ppg_ir / ppg_red üzerinden).
        raw_ir kontrolü on_packet'te daha önce yapılır.
        """
        if packet["valid"] != 1:
            return False

        ir_abs = abs(packet["ppg_ir"])
        red_abs = abs(packet["ppg_red"])

        # İki kanal da çok küçükse sinyal yok kabul edilir.
        if ir_abs < self.PPG_MIN_ABS_VALUE and red_abs < self.PPG_MIN_ABS_VALUE:
            return False

        return True

    def ppg_has_enough_variation(self) -> bool:
        """
        PPG dalgası tamamen düzleşmişse parmak yok veya temas kötü olabilir.
        Ama başlangıçta pencere dolmadan NO FINGER demiyoruz.
        """
        if len(self.ir_signal_history) < self.SIGNAL_WINDOW_SIZE:
            return True

        ir_range = max(self.ir_signal_history) - min(self.ir_signal_history)
        red_range = max(self.red_signal_history) - min(self.red_signal_history)

        if ir_range < self.MIN_PPG_VARIATION and red_range < self.MIN_PPG_VARIATION:
            return False

        return True

    def show_bad_signal(self):
        """
        Geçici kötü sinyal.
        Grafik temizlenmez; böylece ekranda reset hissi oluşmaz.
        """
        self.valid_card.set_value("BAD")

    def show_no_finger(self):
        """
        IR/RED sıfıra yakın ya da uzun süre kötü sinyal → parmak yok.
        Tüm değerler sıfırlanır ve grafik temizlenir.
        """
        self.bpm_card.set_value("-")
        self.spo2_card.set_value("-")
        self.valid_card.set_value("NO DETECT")

        self.x_data.clear()
        self.ir_data.clear()
        self.red_data.clear()

        self.ir_signal_history.clear()
        self.red_signal_history.clear()

        self.ir_curve.setData([], [])
        self.red_curve.setData([], [])

    def on_packet(self, packet: dict):
        self.latest_packet = packet
        self.packet_count += 1
        self.rate_card.set_value(str(self.packet_count))

        # raw_ir 100'ün altındaysa sensörde yeterli sinyal yok → anında NO FINGER.
        if packet["raw_ir"] < self.RAW_IR_MIN_VALUE:
            self.invalid_counter = self.INVALID_LIMIT  # sayacı doldur
            self.finger_present = False
            self.show_no_finger()
            return

        # Filtreli kanallar da sıfıra yakınsa yine NO FINGER.
        if self.ir_red_near_zero(packet):
            self.invalid_counter = self.INVALID_LIMIT  # sayacı doldur
            self.finger_present = False
            self.show_no_finger()
            return

        self.ir_signal_history.append(packet["ppg_ir"])
        self.red_signal_history.append(packet["ppg_red"])

        basic_valid = self.packet_looks_valid(packet)
        wave_valid = self.ppg_has_enough_variation()

        if not basic_valid or not wave_valid:
            self.invalid_counter += 1

            if self.invalid_counter >= self.INVALID_LIMIT:
                self.finger_present = False
                self.show_no_finger()
            else:
                self.show_bad_signal()

            return

        # Sinyal kabul edilebilir.
        self.invalid_counter = 0
        self.finger_present = True

        self.sample_counter += 1
        self.x_data.append(self.sample_counter)
        self.ir_data.append(packet["ppg_ir"])
        self.red_data.append(packet["ppg_red"])

        self.bpm_card.set_value(f"{packet['bpm']:.1f}")
        self.spo2_card.set_value(f"{packet['spo2']:.1f}")
        self.valid_card.set_value("OK")

    def refresh_plot(self):
        if not self.x_data:
            return

        x = list(self.x_data)
        self.ir_curve.setData(x, list(self.ir_data))
        self.red_curve.setData(x, list(self.red_data))

    def closeEvent(self, event):
        self.disconnect_serial()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)

    window = MainWindow()
    window.show()

    sys.exit(app.exec())
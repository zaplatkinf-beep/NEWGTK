# УСТАНОВИ БИБЛИОТЕКУ: pip install iec61850
from iec61850 import IedConnection

# Подключиться к серверу
conn = IedConnection()
conn.connect("localhost", 102)  # Адрес и порт

# Отправить команду "включить"
conn.control("GenericIO/GGIO1.SPCSO1.Oper", True)

# Прочитать состояние
value = conn.read("GenericIO/GGIO1.SPCSO1.stVal")
print(f"Состояние SPCSO1: {value}")

# Прочитать аналоговый вход
temp = conn.read("GenericIO/GGIO1.AnIn1.mag.f")
print(f"Датчик 1: {temp}°C")

# Питание после приёма миссии

GPIO4 LOW → CRC + parse + save → GPIO4 HIGH → новый heartbeat → upload → MISSION_ACK ACCEPTED → AUTO → COMMAND_ACK + heartbeat AUTO → ARM → COMMAND_ACK + heartbeat ARMED.
Старый файл после перезапуска не активирует питание. Потеря Wi-Fi/heartbeat не выключает ключ.
ESP/модем питаются перед ключом. Управление ручным GPIO через HTTP запрещено.
AP modem_bridge / 00000000, HTTP 192.168.0.4 (основной адрес), DNS legion.modem (дополнительно), выключение Wi-Fi через 300000 мс от запуска.
Статус показывает заданное питание (не обратную связь), этап, длительности и свежесть MAVLink.

AUTO/ARM не обходят проверки ArduPilot. Отказ/таймаут 30 с/потеря heartbeat прекращают дальнейшие команды, GPIO4 остаётся HIGH. Stop sequence прекращает последующие шаги, не делает DISARM и не отменяет уже отправленную команду.

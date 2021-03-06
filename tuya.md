# Обновление прошивки привода Tuya WiFi Curtains Motor

Для извлечения платы контроллера из двигателя достаточно открутить четыре самореза с нижнего торца 
двигателя и два удерживающих плату на пластиковой заглушке и 12в блоке питания:

![Расположение элементов на плате контроллера](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/01Layout.jpg)

К сожалению, штатная прошивка TYWE3S не предусматривает возможности обновления модуля "по воздуху" а 
MCU к тому же постоянно шлет пакеты, вызывая ошибки при попытке прошить его без отключения от основной платы контроллера.
Поэтому перед обновлением необходимо разорвать цепь питания между WiFi модулем и основной платой. 
Сделать это можно выпаяв (перекусив?) соответствующий пин либо перерезав дорожку на самом модуле.

Я просто выпаивал пин со стороны основной платы мощным паяльником (там большая контактная площадка, ее 
трудно прогреть) а после прошивки восстанавливал шину питания перемычкой.

![Шина питания WiFi модуля](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/02ControlBoard.jpg)

После разрыва цепи питания между модулем и контроллером к последовательному интерфейсу необходимо
подключить USB/UART адаптер а выход PIO-0 модуля временно (желательно через кнопку) замкнуть на землю.
А для этого понадобится собственно USB/UART адаптер. Купить его на aliexpress можно по цене семечек, но лично 
мне нравится чуть дороже, [вот такой](https://aliexpress.ru/item/4000409620491.html) 
(внимание, по ссылке 5шт!). 

![Подключение uart программатора к WiFi модулю](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/03Wiring.jpg)

Проверяем (дважды!) что на адаптере правильно установлено напряжение питания 3.3v, замыкаем PIO0 на GND кнопкой 
(если она есть), подключаем адаптер к компьютеру, в ArduinoIDE выбираем появившийся COM порт и:
  * Cкорость: 115200 
  * Board: "ESP8266 generic (Chip is ESP8266EX)"
  * Flash size: 2MB / FS:None
  * На всякий случай: "Erase Flash: ALL content"
  * Жмем "Build and Upload"
  * Дожидаемся завершения прошивки, отключаем программатор
  * Восстанавливаем питание, убеждаемся в работоспособности модуля
  * Собираем привод штор, пользуемся.

Я предполагаю, что дочитавший до этого места имеет представление о прошивке ESP8266, знает что RX/TX модуля
подключается к TX/RX адаптера (вперехлест), что кнопку Flash не стоит отпускать до окончания процесса прошивки
и умеет сам решать возникающие трудности. Я не готов устраивать ликбез, со вопросами идите в гугль :)

### Внешний разъем для подключения дополнительных устройств:

Для реализации некоторых сценариев управления УД требуется наличие датчика освещенности на улице. На этот случай 
на части приводов был установлен внешний разъем, на который выведены 3.3в, GPIO4 и GPIO5 :

![Вывод I2C на внешний разъем](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/04_I2C.jpg)
![I2C разъем на приводе](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/05_I2C.jpg)

# MQTT Firmware for Tuya WiFi Curtain Motor
### MQTT прошивка для привода раздвижных штор Tuya WiFi

Приводы раздвижных штор Tuya давно и заслуженно пользуются популярностью, являясь более дешевой альтернативой 
того же Somfy и предоставляя возможность интеграции в системы умного дома.

К сожалению, взаимодействие с WiFi версией моторов, как и с большинством WiFi IoT устройств, 
происходит через внешние сервера. А это (опуская отсылки к "большому китайскому брату") делает управление
как минимум зависимым от наличия и стабильности доступа в интернет.

Поэтому целью данного проекта была разработка альтернативной прошивки WiFi контроллера привода для прямого 
управления через MQTT сервер.

Отмечу, что это не первая реализация подобной прошивки. Вы можете воспользоваться, например,

  * [Прошивкой Tasmota](https://github.com/Greefon/docs/blob/master/Tuya-generic-wifi-curtain-motor-WIP.md)
  * Либо [прошивкой на базе ESPHome](https://github.com/iphong/esphome-tuya-curtain)

К сожалению, (как выяснилось позже) часть присланных мне приводов имеет не совпадающие с использованными
в Tasmota/ESPHome кодами команд управления, поэтому мне все равно пришлось ковыряться в протоколе самому.
Второй причиной появления этого проекта стало нежелание разводить зоопарк, поскольку все WiFi IoT устройства
у меня построены на базе [ESP8266 IoT Framework](https://github.com/mosave/AELib).

Подобно большинству WiFi IoT устройств Tuya, контроллер мотора построен по двухуровневой схеме:
  * Непосредственно железом (двигатель, энкодеры, защита от перегрузки, калибровка крайних положений, 
    ручное открытие/закрытие штор) управляет MCU. Он же принимает команды от опционального RF433 пульта
  * Возможность внешнего управления шторами реализует модуль [Tuya TYWE3S](https://tasmota.github.io/docs/devices/TYWE3S/), 
    который по факту является обычным ESP8266ex модулем с объемом флеш памяти 2мб. Модуль взаимодействует
    с MCU через последовательный интерфейс (9600/8/1/none) по стандартизованному Tuya протоколу. При этом 
    сам модуль не реализует логики по управлению устройством а является прозрачным шлюзом, через 
    который с MCU взаимодействует сервер Tuya и/или непосредственно мобильное приложение.
    Описание протокола можно найти в папке Docs и на [сайте Tuya](https://tasmota.github.io/docs/TuyaMCU/)
      
Благодаря такой двухуровневой архитектуре устройство выполняет базовые функции даже при отказе WiFi модуля,
позволяя без особых сложностей заменить стандартную прошивку модуля на альтернативную, реализующую иной 
способ взаимодействия с устройством. Например через прямые http запросы либо локально установленный MQTT брокер.


### Список MQTT топиков и соответствующих им команд для управления шторами

Как уже было отмечено, прошивка модуля собрана с использованием готового фреймворка, поэтому 
помимо описанных ниже топиков, поддерживает все 
["стандартные" функции, реализованные в фреймворке](https://github.com/mosave/AELib#comms-wifi-mqtt-%D0%B8-ota): 
конфигурация и текущее состояние устройства, перезагрузка, прошивка "по воздуху" и тд.

 * **State**: Текущее состояние привода. Может принимать значения 
   * **Idle**: шторы не двигаются
   * **Opening**, "Closing" или "Moving" (если направление движения установить н получилось)
   * **Failed**: вы не должны такого увидеть :)
 * **Position**: Текущее положение штор привода в процентах, целое число в диапазоне 0..100. 
   После включения питания принимает значение 50 и остается таковым до проведения полной калибровки штор, то есть их полного открытия и закрытия.
 * **SetPosition**: Задать новое положение штор. Доступно только после калибровки. В качестве payload 
   передается процент открытия штор, целое число в диапазоне 0..100
 * **Command**: Команды управления шторами. В качестве payload передается текст команды
   * **Open**: Полностью открыть шторы (==SetPosition 100)
   * **Close**: Полностью закрыть шторы (==SetPosition 0)
   * **Stop**: Остановить движение штор
   * **Continue**: Продолжить движение штор, остановленной командой Stop
   * **Reverse**: Изменить направление открытия/закрытия штор на противоположное.

* **Log**: Журнал обмена сообщений между WiFi модулем и MCU. Используется только для отладки и появляется 
  только если в файле конфигурации прошивки **config.h** определен символ **#define MCU_DEBUG**.
* **SendCommand**: Отправить команду MCU. Аналогично предыдущему топику, тоступен только при включении режима отладки.
  В качестве payload передается последовательность байт в виде шестнадцатеричной строки БЕЗ контрольной суммы. 
  Например отправка строки ```55aa000600056504000102``` приведет к полному открытию штор (если MCU поддерживает данную команду).

### Настройка прошивки

В каталоге [Firmware](https://github.com/mosave/Tuya2MQTT/tree/main/Firmware) находятся исходники прошивки. 
Для ее заливки я рекомендую использовать Arduino IDE либо любую другую среду разработки для Arduino: Visual Studio Code,
Visual Studio с пакетом Visual Micro, PlatformIO или аналогичные.

Перед началом процесса прошивки необходимо просмотреть и внести изменения в файл **config.h**.
Например, нужно задать параметры подключения к WiFi и адрес MQTT брокера (если в сети не настроено 
анонсирование брокера через сервис MDNS).

Не определяйте символ USE_SOFT_SERIAL, этот режим используется при разработке прошивки и приводит к тому что 
команды MCU отправляются не на стандартный последовательный интерфейс а на SoftSerial.

### Заливка прошивки в модуль управления

    Для извлечения платы контроллера из двигателя достаточно открутить четыре самореза с нижнего торца 
двигателя и два удерживающих плату на пластиковой заглушке и 12в блоке питания:

![Расположение элементов на плате контроллера](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/01Layout.jpg)

    К сожалению, штатная прошивка TYWE3S не предусматривает возможности обновления модуля "по воздуху" а 
MCU постоянно шлет пакеты WiFi модулю, вызывая ошибки при попытке прошить его не отключая от основной платы контроллера.
Поэтому перед обновлением необходимо разорвать цепь питания между WiFi модулем и основной платой. 
Сделать это можно выпаяв (перекусив?) соответствующий пин либо перерезав дорожку на самом модуле.

Я просто выпаивал пин со стороны основной платы мощным паяльником (там большая контактная площадка, которую 
трудно прогреть) а после прошивки восстанавливал шину питания "соплей". Это дает возможность в будущем, если вдруг 
понадобится перепрошивка именно по проводам, легко восстановить uart соединение. 

![Шина питания WiFi модуля](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/02ControlBoard.jpg)

    После разрывва цепи питания между модулем и контроллером - необходимо к последовательному интерфейсу 
подключить USB/UART адаптер, а выход PIO-0 модуля на время прошивки замкнуть на землю (можно через кнопку)
А для этого понадоб собственно USB/UART адаптер. Купить его на aliexpress можно по цене семечек, но лично 
мне навится чуть дороже, [вот такой](https://aliexpress.ru/item/4000409620491.html) 
(внимание, по ссылке 5шт!). 

![Подключение uart программатора к WiFi модулю](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/03Wiring.jpg)

Собственно все. Дважды проверяем что на адаптере правильно выбрано напряжение питания 3.3V, замыкаем Flash на GND кнопкой (если она есть), подключаем адаптер к компьютеру, 
в ArduinoIDE выбираем
  * Свежепоявившийся COM порт
  * Выставляем скорость 115200 
  * Board: "ESP8266 generic (Chip is ESP8266EX)"
  * Flash size: 2MB / FS:None
  * На всякий случай "Erase Flash: ALL content"
  * Жмем "Build and Upload"
  * Дожидаемся завершения прошивки, отключаем программатор
  * Восстанавливаем питание, убеждаемся в работоспособности модуля
  * Собираем привод штор, пользуемся.

Я предполагаю что дочитавший до этого места уже имеет представление о ESP8266, знает что RX/TX модуля
подключается к TX/RX адаптера (с перехлестом) и умеет сам решать возникающие трудности. Я не готов 
устраивать здесь соответствующий ликбез со всеми вопросами идите в гугль :)

### Бонусные плюшки

ESP8266 - достаточно мощная штука и помимо основной может выполнять еще кучу разных задач. Например,
к нему можно подключить датчики температуры и влажности (хотя расположение рядом с окном и за шторами 
не самое лучшее с этой точки зрения), открытия окна, движения или освещенности на улице. 
Для этих целей я вывел 3.3v, GPIO4 и GPIO5 на разъем на корпусе привода.

![Вывод I2C на внешний разъем](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/04_I2C.jpg)

![I2C разъем на приводе](https://github.com/mosave/Tuya2MQTT/raw/main/Photos/04_I2C.jpg)



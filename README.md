# INA219-sensor
INA219 Using external shunt resistance and display readings on the web

## Starting the program
- Download the code, copile and run. 
- Using this tool, https://github.com/esp8266/arduino-esp8266fs-plugin upload the data files to esp flash memory.
- Add  and < to credentials.h
- run the http://ina219-webserver.local/ina219.html for viewing sensor data
- INA219 needs to be connected to D1 (SCL) and D2 (SDA) of the wemos d1 mini
- Pull-up registers are required.

* Currently, we are able to send data from the esp8266+ina219 to the web-browser through websockets although the speed is quite slow. (about 10 messages/second)
* We are able to change the parameters of the INA219 through web interface
* Speed of sending data may be varied
* Incoming data is plotted on a graph
* Data is also displayed on an html table on the fly
## Debugging

* Most of the web browsers already have necessary tools required which can be accessed via Right Click>Inspect menu or F9
* FSBrowser example includes embedded editor in the ESP Webserver itself. To access, goto http://ina219-webserver.local/edit
* Edit the html/java script in the browser or the esp editor. Modifications can also be done inside the browser itself, in the Console.
* Editing arduino requires more effort, as the code needs to be compiled

## Description

* This program is based on the FSBrowser exapmple of https://github.com/me-no-dev/ESPAsyncWebServer
In addition following libraries are also needed. they can be installed from library manager of arduino
* https://github.com/flav1972/ArduinoINA219
* ArduinoJson


## To-do

So many things. A few things, in my mind are:

- Data logging to SPIFFSS
- Data logging to SD CARD, in case we need to write more data, without the fear of running out of memory
- Retreiving data from SPIFFS/SD CARD through webserver
- Downloading/Viewing Data stored in attached SPIFFS/SDCARD inside the webbrowser
- FFT of Data collected
- Sending data to Online platforms like Firebase/Thingspeak for remote monitoring and retrieval
- MQTT Support
- Some command structure
ABC-XYZ

Unfortunately, i am not a professional and i do not have much time. let's see how this proceeds.

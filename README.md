# ESP32 Linky Teleinfo

## About the project

**Linky Teleinfo** is a project to collect data from a **Linky meter** and send them to a **web server**. The system is powered by the pins A of the Linky meter and with the help of a supercapacitor.

#### Built With

This project as been created with : 

[![Visual Studio Code](https://img.shields.io/badge/Visual%20Studio%20Code-0078d7.svg?style=for-the-badge&logo=visual-studio-code&logoColor=white)](https://code.visualstudio.com/)

- Web interface 

[![figma](https://img.shields.io/badge/Figma-F24E1E?style=for-the-badge&logo=figma&logoColor=white)](https://www.figma.com/)
[![sass](https://img.shields.io/badge/SASS-hotpink.svg?style=for-the-badge&logo=SASS&logoColor=white)](https://sass-lang.com/)
[![socket_io](https://img.shields.io/badge/Socket.io-black?style=for-the-badge&logo=socket.io&badgeColor=010101)](https://socket.io/)

[![nodejs](https://img.shields.io/badge/node.js-6DA55F?style=for-the-badge&logo=node.js&logoColor=white)](https://nodejs.org/en/)
[![vue-js](https://img.shields.io/badge/Vue.js-35495E?style=for-the-badge&logo=vuedotjs&logoColor=4FC08D)](https://vuejs.org/)
[![char-js](https://img.shields.io/badge/Chart.js-FF6384?style=for-the-badge&logo=chartdotjs&logoColor=white)](https://www.chartjs.org/)

- Web server 

[![mysql](https://img.shields.io/badge/MySQL-005C84?style=for-the-badge&logo=mysql&logoColor=white)](https://www.mysql.com/fr/)
[![express-js](https://img.shields.io/badge/Express.js-000000?style=for-the-badge&logo=express&logoColor=white)](https://expressjs.com/fr/)
[![prisma](https://img.shields.io/badge/Prisma-3982CE?style=for-the-badge&logo=Prisma&logoColor=white)](https://www.prisma.io/)

- Microcontroller programming 

[![arduino](https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=Arduino&logoColor=white)](https://www.arduino.cc/)
[![espressif](https://img.shields.io/badge/espressif-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)

## Electronic

[`esp32/src`](/esp32/src) : The code is made to be sent on an ESP32 on an electronic card

| PCB without components | PCB with soldered components | PCB connected to the linky meter |
|-----|-----|-----|
| ![](img/PhotoPCB1.png) | ![](img/PhotoPCB2.png) | ![](img/PhotoPCB3.png) | 

#### Schematic

[`pcb/Schematic_LINKY_ESP32.pdf`](pcb/Schematic_LINKY_ESP32.pdf)

#### PCB

[`pcb/Gerber_PCB_LINKY_ESP32.zip`](pcb/Gerber_PCB_LINKY_ESP32.zip)

| Top Side  | Bottom Side |
| ------------- | ------------- |
| ![topside](img/TopSide.png) | ![bottomside](img/BottomSide.png) |

## Installation

#### ESP32 :satellite:
Compile and send [`esp32/src`](/esp32/src) files to the ESP32

#### DataBase MySQL :globe_with_meridians:
Create a `Linky` Database
Copy the [.env_sample](/webserver/.env_sample) to `.env` and fill with your database logins

#### WebPage :computer:
Host the website available on [`webserver/`](/webserver)
```bash
cd webserver
#Installation
npm install
npx prisma generate
npx prisma db push
#Start
npm start
```

## Running Tests
Start the system with USB cable and serial monitor opened and you should see

```
Starting...
Connecting to [WIFI_SSID] ...
WiFi connected
IP address: 192.168.43.185
Getting config from server...OK
Getting time from NTP...OK
Disconecting from wifi...OK
Data stored: 0 - BASE:1647
Data stored: 1 - BASE:1647
Data stored: 2 - BASE:1647
Preparing json data... OK
Connecting to [WIFI_SSID] ...
WiFi connected
IP address: 192.168.43.185
Getting config from server...OK
Getting time from NTP...OK
Sending data to server... OK: 200
Disconecting from wifi...OK
```
## Demo

WebPage example

![webpageimg](img/WebPage.png)

## Authors

- [@Dorian.local/](https://github.com/xmow49)
- [@Noah_](https://github.com/NoahJust)

[![youtube](https://img.shields.io/badge/YouTube-%23FF0000.svg?style=for-the-badge&logo=YouTube&logoColor=white)](https://www.youtube.com/gammatroniques)
[![siteweb](https://img.shields.io/badge/GammaTroniques-EE6B00?style=for-the-badge&logoColor=white)](https://gammatroniques.fr/)

>__Note__  
This project is still under development, it is possible that errors and problems are found in the code

:shipit:

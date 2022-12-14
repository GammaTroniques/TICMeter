# ESP32 Linky Téléinfo

## About the project

The **Linky Téléinfo** project aims to retrieve consumption information
from a **Linky meter** to send it to a **web server**. The system must be powered
without an external power supply.

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

`est32/src` : The code is made to be sent on an ESP32 on an electronic card

| Top Side  | Bottom Side |
| ------------- | ------------- |
| ![topside](https://github.com/xmow49/LinkyTeleinfoESP32/blob/5a6a4f057d743f156686f012e9a329d2cba7980d/img/TopSide.png) | ![bottomside](https://github.com/xmow49/LinkyTeleinfoESP32/blob/5a6a4f057d743f156686f012e9a329d2cba7980d/img/BottomSide.png) |

#### Schematic

#### PCB

## Installation

#### ESP32 :satellite:
Compile and send `esp32/src/` files to the ESP32

#### DataBase MySQL :globe_with_meridians:
Create Linky Database and execute
```bash
npx prisma db push
```

#### WebPage :computer:
Host the website available on `webserver/`
```bash
#Installation
npm install
npx prisma generate
#Start
npm start
```

## Running Tests
Start the system with USB cable and serial monitor opened and you should see

```bash
  1671030318
  Data stored: 1 - 1599
  1671030382
  Data stored: 2 - 1599
  error: number of start and end frames are not equal:9 10
  Error: decode failed
  1671030451
  Data stored: 3 - 1600
  Sending data ␃ 1671030318
  Sending data ␃ 1671030318
  Sending data ␃ 1671030318
```
## Demo

WebPage example

![webpageimg](https://raw.githubusercontent.com/xmow49/LinkySAE/0c7f3f8a5039b80d5e1c0aa4dec2789c06513440/img/WebPage.png)

## Authors

- [@xmow49](https://github.com/xmow49)
- [@Noah_](https://github.com/NoahJust)

[![youtube](https://img.shields.io/badge/YouTube-%23FF0000.svg?style=for-the-badge&logo=YouTube&logoColor=white)](https://www.youtube.com/gammatroniques)
[![siteweb](https://img.shields.io/badge/GammaTroniques-EE6B00?style=for-the-badge&logoColor=white)](https://gammatroniques.fr/)

>__Note__  
This project is still under development, it is possible that errors and problems are found in the code

:shipit:

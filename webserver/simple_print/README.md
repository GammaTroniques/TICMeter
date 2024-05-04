# Simple Test Webserver

Here you can find a simple webserver that prints all data received from the TICMeter.
This example also shows how to use the TICMeter config.

## How to use

1. Install packages

```bash
npm install
```

2. Change the TOKEN

In the file `index.js` change the `TOKEN` by any string you want (max 99 characters).

3. Run the server

Run the server with the following command:

```bash
npm start
```

The server will be running on `http://localhost:3000`.

4. Edit the config

In `index.js` you can find the config:

```javascript
const config = {
  refresh_rate: 60, // read data every X second. min 10s max 3600s
  store_before_send: 2, //send data every X read. min 0 max 10
};
```

You can change the `refresh_rate` and `store_before_send` values.

5. Configure the TICMeter

Start the pairing mode on the TICMeter, connect to his WiFi network and access the address to the web configuration page (`http://4.3.2.1`).

Next, enter all the required fields:

- **WiFi SSID**: `<Your WiFi SSID>`
- **WiFi Password**: `<Your WiFi Password>`
- **Server URL**: `<Your IP>:3000`
- **POST URL**: `/post`
- **CONFIG URL**: `/config`
- **TOKEN**: `<Your chosen token>`

Save the configuration.

6. Send data

After a minute, the TICMeter will send data to the server. You can check the data on the console.

You can see `TICMeter get config` on the console, this means that the TICMeter asked for the configuration.

When data is received, you will see the data on the console:

```json
{
  "TOKEN": "1234",
  "VCONDO": 5.081999778747559,
  "data": [
    {
      "ADCO": "XXXXXXXXXXXX",
      "OPTARIF": "BASE",
      "ISOUSC": 30,
      "pref": 6,
      "total": 6127346,
      "BASE": 6127346,
      "PTEC": "TH..",
      "IINST": 0,
      "IMAX": 90,
      "PAPP": 0,
      "HHPHC": "A",
      "MOTDETAT": "000000",
      "now-refresh": 25956,
      "mode-tic": 0,
      "mode-elec": 0,
      "timestamp": 1714851281,
      "uptime": 27622
    },
    {
      "ADCO": "XXXXXXXXXXXX",
      "OPTARIF": "BASE",
      "ISOUSC": 30,
      "pref": 6,
      "total": 6127346,
      "BASE": 6127346,
      "PTEC": "TH..",
      "IINST": 0,
      "IMAX": 90,
      "PAPP": 0,
      "HHPHC": "A",
      "MOTDETAT": "000000",
      "now-refresh": 16513,
      "mode-tic": 0,
      "mode-elec": 0,
      "timestamp": 1714851290,
      "uptime": 36385
    }
  ]
}
```

Now you can use the data as you want.

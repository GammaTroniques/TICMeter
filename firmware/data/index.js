const serverMode = document.getElementsByName("server-mode");
const web = document.querySelector(".web");
const mqtt = document.querySelector(".mqtt");
const tuya = document.querySelector(".tuya");
const zigbee = document.querySelector(".zigbee");
const radio = document.getElementsByName("server-mode");
const wifiList = document.querySelector(".wifi-list");
const wifiLists = document.getElementsByName("wifi-list");
const wifiHidden = document.getElementsByName("wifi-hidden");
const wifiSSIDField = document.getElementById("wifi-ssid-field");
const wifiSSID = document.getElementById("wifi-ssid");
const wifiScan = document.getElementById("wifi-scan");
function update_wifi_list(ap) {
  wifiList.innerHTML = "";
  if (ap.length == 0) {
    const label = document.createElement("label");
    const text = document.createElement("h3");
    text.textContent = "Aucun réseau Wi-Fi trouvé";
    text.className = "black";
    label.appendChild(text);
    wifiList.appendChild(label);
  }
  for (let i = 0; i < ap.length; i++) {
    const element = ap[i];
    if (element.ssid == "") {
      continue;
    }
    console.log(element);
    const label = document.createElement("label");
    const input = document.createElement("input");
    const text = document.createElement("h3");
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    const use = document.createElementNS("http://www.w3.org/2000/svg", "use");
    const div = document.createElement("div");
    input.type = "radio";
    input.name = "wifi-list";
    input.value = element.ssid;

    let level = 0;
    if (element.rssi > -50) {
      level = 4;
    } else if (element.rssi > -70) {
      level = 3;
    } else if (element.rssi > -80) {
      level = 2;
    } else if (element.rssi > -90) {
      level = 1;
    }

    use.setAttributeNS("http://www.w3.org/1999/xlink", "href", "#wifi-" + level);
    svg.appendChild(use);
    svg.setAttribute("width", "50");
    svg.setAttribute("height", "50");
    svg.setAttribute("aria-hidden", "true");
    svg.setAttribute("focusable", "false");
    svg.setAttribute("class", "wifi-level");

    text.textContent = element.ssid;
    text.className = "black";

    label.appendChild(input);
    label.className = "wifi-item-container";
    div.appendChild(svg);
    div.appendChild(text);
    div.className = "wifi-item";
    label.appendChild(div);
    wifiList.appendChild(label);
    wifiLists.forEach((element) => {
      element.onclick = function (e) {
        wifiSSID.value = this.value;
        wifiHidden[0].checked = false;
        wifiSSIDField.classList.add("hide");
      };
    });

    let ok = false;
    wifiLists.forEach((element) => {
      if (element.value == wifiSSID.value) {
        element.checked = true;
        ok = true;
      }
    });
    if (!ok) {
      wifiHidden[0].checked = true;
      wifiSSIDField.classList.remove("hide");
    }
  }
}

var wifi_scanning = false;
function wifi_scan() {
  if (wifi_scanning) {
    return;
  }
  wifi_scanning = true;
  wifiScan.textContent = "Scan en cours...";
  fetch("/wifi-scan")
    .then((response) => response.json())
    .then((data) => {
      console.log(data);
      const ap = data["ap"];
      update_wifi_list(ap);
      wifiScan.textContent = "Scanner les réseaux WiFi";
    })
    .catch((error) => {
      wifiScan.textContent = "Erreur lors du scan";
    })
    .finally(() => {
      wifi_scanning = false;
    });
}

function get_selected_wifi() {
  const wifi = document.querySelector('input[name="wifi-ssid"]:checked');
  if (wifi) {
    return wifi.value;
  }
  return "";
}

window.addEventListener("load", function () {
  web.style.display = "none";
  mqtt.style.display = "none";
  tuya.style.display = "none";
  zigbee.style.display = "none";
  for (let i = 0; i < radio.length; i++) {
    radio[i].checked = false;
  }

  serverMode.forEach((element) => {
    element.addEventListener("change", (event) => {
      console.log(event.target.value);
      web.style.display = "none";
      mqtt.style.display = "none";
      tuya.style.display = "none";
      zigbee.style.display = "none";
      switch (event.target.value) {
        case "1":
          web.style.display = "block";
          break;
        case "2":
          mqtt.style.display = "block";
          break;
        case "3":
          mqtt.style.display = "block";
          break;
        case "4":
          zigbee.style.display = "block";
          break;
        case "5":
          tuya.style.display = "block";
          break;
      }
    });
  });

  wifiHidden.forEach((element) => {
    console.log(element);
    element.addEventListener("change", (event) => {
      console.log(event.target.checked);
      if (event.target.checked) {
        wifiSSIDField.classList.remove("hide");
      } else {
        wifiSSIDField.classList.add("hide");
      }
    });
  });
  //fetch config
  fetch("/config")
    .then((response) => response.json())
    .then((data) => {
      for (const key in data) {
        if (data.hasOwnProperty(key)) {
          const element = data[key];
          if (key == "server-mode") {
            continue;
          }
          const input = document.querySelector(`[name="${key}"]`);
          if (input) {
            input.value = element;
          }
        }
      }
      if ("server-mode" in data) {
        web.style.display = "none";
        mqtt.style.display = "none";
        tuya.style.display = "none";
        zigbee.style.display = "none";
        for (let i = 0; i < radio.length; i++) {
          radio[i].checked = false;
        }

        switch (data["server-mode"]) {
          case 1:
            web.style.display = "flex";
            radio[0].checked = true;
            break;
          case 2:
          case 3:
            mqtt.style.display = "flex";
            radio[1].checked = true;
            if (data["server-mode"] == 2) {
              //mqtt + ha
              const mqttHaDiscovery = document.querySelector(`[name="mqtt-ha-discovery"]`);
              mqttHaDiscovery.checked = 1;
            }
            break;
          case 4:
            zigbee.style.display = "block";
            radio[2].checked = true;
            break;
          case 5:
            tuya.style.display = "block";
            radio[3].checked = true;
            break;
        }
      }

      if ("linky-mode" in data) {
        const linkyMode = document.querySelector(`[name="linky-mode"][value="${data["linky-mode"]}"]`);
        linkyMode.checked = true;
      }

      if ("tuya-device-uuid" in data) {
        const tuyaUUID = document.getElementsByName("tuya-device-uuid");
        tuyaUUID.value = data["tuya-device-uuid"];
      }
      if ("tuya-device-auth" in data) {
        const tuyaAuth = document.getElementsByName("tuya-device-auth");
        tuyaAuth.value = data["tuya-device-auth"];
      }
    });

  wifi_scan();
});

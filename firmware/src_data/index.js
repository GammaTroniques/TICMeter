const linky_tic_auto_switch = document.getElementById("linky-tic-auto");
const linky_tic_auto_radio = document.getElementById("linky-mode-auto");
const linky_tic_mode_values = document.getElementsByName("linky-mode");
const linky_tic_mode_containers = document.getElementsByClassName("linky-selector-container");
const mode_selectors = document.getElementsByName("server-mode");
const mode_config_http = document.getElementById("http-conf");
const mode_config_mqtt = document.getElementById("mqtt-conf");
const mode_config_zigbee = document.getElementById("zigbee-conf");
const mode_config_tuya = document.getElementById("tuya-conf");

const wifi_configurator = document.getElementById("wifi-configurator");
const wifi_password = document.getElementById("wifi-password");
const wifi_password_svg = document.getElementById("wifi-pw-svg");
let wifi_password_edited = false;

const save_button = document.getElementById("save-button");
const quit_button = document.getElementById("quit-button");

const test_container = document.getElementById("tests-container");
const test_error_text = document.getElementById("test-error-text");
const test_error_div = document.getElementById("test-error-div");
const test_success_div = document.getElementById("test-success-div");

const page_config = document.getElementById("page-config");
const page_tests = document.getElementById("page-tests");
const page_reboot = document.getElementById("page-reboot");

const wifi_ssid = document.getElementById("wifi-ssid");
const wifi_loading_svg = document.getElementById("wifi-loading-svg");

const popup = document.getElementById("popup");

const wifi_list = document.getElementById("wifi-list");
const wifi_text = document.getElementById("wifi-text");

const tuya_advanced = document.getElementById("tuya-advanced");
const tuya_device_uuid = document.getElementById("tuya-device-uuid");
const tuya_device_auth = document.getElementById("tuya-device-auth");
let tuya_device_uuid_edited = false;
let tuya_device_auth_edited = false;

const mqtt_password = document.getElementById("mqtt-password");
const mqtt_password_svg = document.getElementById("mqtt-pw-svg");
let mqtt_password_edited = false;

const PENDING = 0;
const RUNNING = 1;
const SUCCESS = 2;
const FAILURE = 3;

// prettier-ignore
const tests_list = [
  { id: 1, text: "Connexion au réseau WiFi",  state: PENDING },
  { id: 2, text: "Test du réseau WiFi",       state: PENDING },
  { id: 3, text: "Connexion au serveur MQTT", state: PENDING },
  { id: 4, text: "Envoi des données MQTT",    state: PENDING },
];

function update_config_view(mode) {
  console.log("Mode: " + mode);
  mode_config_http.classList.add("hide");
  mode_config_mqtt.classList.add("hide");
  mode_config_zigbee.classList.add("hide");
  mode_config_tuya.classList.add("hide");
  wifi_configurator.classList.add("hide");
  switch (mode) {
    case 1:
      mode_config_http.classList.remove("hide");
      wifi_configurator.classList.remove("hide");
      break;
    case 2:
    case 3:
      mode_config_mqtt.classList.remove("hide");
      wifi_configurator.classList.remove("hide");
      break;
    case 4:
      mode_config_zigbee.classList.remove("hide");
      break;
    case 5:
      mode_config_tuya.classList.remove("hide");
      wifi_configurator.classList.remove("hide");
      break;
  }
}

function update_linky_tic_mode(mode) {
  console.log("Linky mode: " + mode);
  for (let i = 0; i < linky_tic_mode_values.length; i++) {
    linky_tic_mode_values[i].checked = false;
  }
  if (mode == 2) {
    linky_tic_auto_switch.checked = true;
    linky_tic_auto_radio.checked = true;
    for (let i = 0; i < linky_tic_mode_containers.length; i++) {
      linky_tic_mode_containers[i].classList.add("disabled");
    }
  } else if (mode == 3) {
    //remove auto mode
    for (let i = 0; i < linky_tic_mode_values.length; i++) {
      if (linky_tic_mode_values[i].value == 0) {
        linky_tic_mode_values[i].checked = true;
      }
    }
    for (let i = 0; i < linky_tic_mode_containers.length; i++) {
      linky_tic_mode_containers[i].classList.remove("disabled");
    }
  } else {
    linky_tic_auto_switch.checked = false;
    linky_tic_auto_radio.checked = false;
    for (let i = 0; i < linky_tic_mode_containers.length; i++) {
      linky_tic_mode_containers[i].classList.remove("disabled");
    }
  }
  for (let i = 0; i < linky_tic_mode_values.length; i++) {
    if (linky_tic_mode_values[i].value == mode) {
      linky_tic_mode_values[i].checked = true;
    }
  }
}

function update_page_view(page) {
  page_config.classList.add("hide");
  page_tests.classList.add("hide");
  page_reboot.classList.add("hide");
  save_button.classList.add("hide");
  quit_button.classList.add("hide");
  switch (page) {
    case 1:
      page_config.classList.remove("hide");
      save_button.classList.remove("hide");
      break;
    case 2:
      page_tests.classList.remove("hide");
      quit_button.classList.remove("hide");
      break;
    case 3:
      page_reboot.classList.remove("hide");
      save_button.classList.add("hide");
      break;
  }
}

function update_tests_view() {
  test_container.innerHTML = "";
  tests_list.forEach((test, index) => {
    let test_div = document.createElement("div");
    test_div.classList.add("test-item");

    let svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    let use = document.createElementNS("http://www.w3.org/2000/svg", "use");

    let use_href = "";
    switch (test.state) {
      case PENDING:
        use_href = "#test-pending";
        break;
      case RUNNING:
        use_href = "#test-running";
        svg.classList.add("spin");
        break;
      case SUCCESS:
        use_href = "#test-ok";
        break;
      case FAILURE:
        use_href = "#test-error";
        break;
    }

    use.setAttributeNS("http://www.w3.org/1999/xlink", "href", use_href);
    svg.appendChild(use);
    svg.setAttribute("aria-hidden", "true");
    svg.setAttribute("focusable", "false");
    svg.classList.add("wifi-level");

    let text = document.createElement("h6");
    text.textContent = test.text;
    text.className = "black";

    test_div.appendChild(svg);
    test_div.appendChild(text);
    test_container.appendChild(test_div);
  });
}

function update_popup(state) {
  if (state) {
    wifi_scan();
    popup.classList.remove("hide");
  } else {
    popup.classList.add("hide");
  }
}

function update_wifi_list(ap) {
  wifi_list.innerHTML = "";
  if (ap.length == 0) {
    const label = document.createElement("label");
    const text = document.createElement("h3");
    text.textContent = "Aucun réseau Wi-Fi trouvé";
    text.className = "black";
    label.appendChild(text);
    wifi_list.appendChild(label);
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
    wifi_list.appendChild(label);

    wifi_list.addEventListener("click", (event) => {
      wifi_ssid.value = event.target.value;
      update_popup(0);
    });
  }
}

async function start_tests() {
  update_tests_view();
  for (let i = 0; i < tests_list.length; i++) {
    tests_list[i].state = PENDING;
  }

  test_error_text.textContent = "";
  test_error_div.classList.add("hide");
  test_success_div.classList.add("hide");

  for (let i = 0; i < tests_list.length; i++) {
    tests_list[i].state = RUNNING;
    update_tests_view();

    await fetch("/test-start?id=" + tests_list[i].id)
      .then((data) => {
        //if OK response
        if (!data.ok) {
          console.log("Test failed", data);
          throw new Error(data.statusText);
        }
        tests_list[i].state = SUCCESS;
        update_tests_view();
      })
      .catch((error) => {
        tests_list[i].state = FAILURE;
        test_error_text.textContent += "Test " + i + ": " + error + "\n";
        update_tests_view();
      });
  }

  let failed_count = 0;
  for (let i = 0; i < tests_list.length; i++) {
    if (tests_list[i].state == FAILURE) {
      failed_count++;
    }
  }

  if (failed_count > 0) {
    test_error_div.classList.remove("hide");
    test_success_div.classList.add("hide");
  } else {
    test_error_div.classList.add("hide");
    test_success_div.classList.remove("hide");
  }
}

var wifi_scanning = false;
function wifi_scan() {
  if (wifi_scanning) {
    return;
  }
  wifi_list.innerHTML = "";
  wifi_scanning = true;
  wifi_text.textContent = "Scan en cours...";
  wifi_loading_svg.classList.remove("hide");

  fetch("/wifi-scan")
    .then((response) => response.json())
    .then((data) => {
      console.log(data);
      const ap = data["ap"];
      update_wifi_list(ap);
      wifi_text.textContent = "";
    })
    .catch((error) => {
      wifi_text.textContent = "Erreur lors du scan.";
    })
    .finally(() => {
      wifi_scanning = false;
      wifi_loading_svg.classList.add("hide");
    });
}

function handleSubmit(event) {
  event.preventDefault();
  const data = new FormData(event.target);
  var config = {};
  for (const [key, value] of data) {
    config[key] = value;
  }

  if (!wifi_password_edited) {
    delete config["wifi-password"];
  }

  if (!tuya_device_uuid_edited) {
    delete config["tuya-device-uuid"];
  }

  if (!tuya_device_auth_edited) {
    delete config["tuya-device-auth"];
  }

  if (!mqtt_password_edited) {
    delete config["mqtt-password"];
  }

  fetch("/config", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(config),
  })
    .then((data) => {
      console.log("Success:", data);
      update_page_view(2); // switch to tests page
      start_tests();
    })
    .catch((error) => {
      console.error("Error:", error);
    });
}

function reboot() {
  update_page_view(3);
  fetch("/reboot")
    .then((data) => {
      console.log("Success:", data);
      update_page_view(3); // switch to reboot page
    })
    .catch((error) => {
      console.error("Error:", error);
    });
}

var tuya_advanced_state = false;
function update_tuya_advanced() {
  if (tuya_advanced_state) {
    tuya_advanced.classList.add("hide");
  } else {
    tuya_advanced.classList.remove("hide");
  }
  tuya_advanced_state = !tuya_advanced_state;
}

let wifi_password_shown = false;
function update_wifi_password() {
  if (wifi_password_shown) {
    wifi_password.type = "password";
    wifi_password_svg.setAttribute("href", "#password-hide");
  } else {
    wifi_password.type = "text";
    wifi_password_svg.setAttribute("href", "#password-show");
  }
  wifi_password_shown = !wifi_password_shown;
}

let mqtt_password_shown = false;
function update_mqtt_password() {
  if (mqtt_password_shown) {
    mqtt_password.type = "password";
    mqtt_password_svg.setAttribute("href", "#password-hide");
  } else {
    mqtt_password.type = "text";
    mqtt_password_svg.setAttribute("href", "#password-show");
  }
  mqtt_password_shown = !mqtt_password_shown;
}

window.addEventListener("load", function () {
  linky_tic_auto_switch.addEventListener("change", (event) => {
    update_linky_tic_mode(event.target.checked ? 2 : 3);
  });

  mode_selectors.forEach((selector) => {
    selector.addEventListener("change", (event) => {
      update_config_view(parseInt(event.target.value));
    });
  });

  wifi_password.addEventListener("input", (event) => {
    wifi_password_edited = true;
    console.log("Password edited");
  });

  tuya_device_uuid.addEventListener("input", (event) => {
    tuya_device_uuid_edited = true;
    console.log("UUID edited");
  });

  tuya_device_auth.addEventListener("input", (event) => {
    tuya_device_auth_edited = true;
    console.log("Auth edited");
  });

  mqtt_password.addEventListener("input", (event) => {
    mqtt_password_edited = true;
    console.log("MQTT password edited");
  });

  const form = document.querySelector("form");
  form.addEventListener("submit", handleSubmit);

  update_page_view(1);
  update_popup(0);
  update_config_view(1);

  fetch("/config")
    .then((response) => response.json())
    .then((data) => {
      for (const key in data) {
        if (data.hasOwnProperty(key)) {
          const element = data[key];
          if (key == "server-mode") {
            continue;
          }
          if (key == "wifi-password") {
            // generate a fake password of data["wifi-password"]
            wifi_password.value = "*".repeat(element);
            continue;
          }
          if (key == "mqtt-password") {
            // generate a fake password of data["mqtt-password"]
            mqtt_password.value = "*".repeat(element);
            continue;
          }
          if (key == "tuya-device-auth") {
            // generate a fake password of data["tuya-device-auth"]
            tuya_device_auth.value = "*".repeat(element);
            continue;
          }

          const input = document.querySelector(`[name="${key}"]`);
          if (input) {
            input.value = element;
          }
        }
      }
      update_config_view(parseInt(data["server-mode"]));
      for (let i = 0; i < mode_selectors.length; i++) {
        if (mode_selectors[i].value == data["server-mode"]) {
          mode_selectors[i].checked = true;
        }
      }
    });
});

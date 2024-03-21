const linky_tic_auto_switch = document.getElementById("linky-tic-auto");
const linky_tic_auto_radio = document.getElementById("linky-mode-auto");
const linky_tic_mode_values = document.getElementsByName("linky-mode");
const linky_tic_mode_containers = document.getElementsByClassName("linky-selector-container");
const mode_selectors = document.getElementsByName("server-mode");
const mode_config_http = document.getElementById("http-conf");
const mode_config_mqtt = document.getElementById("mqtt-conf");
const mode_config_zigbee = document.getElementById("zigbee-conf");
const mode_config_tuya = document.getElementById("tuya-conf");

const wifi_password = document.getElementById("wifi-password");
let wifi_password_edited = false;

const save_button = document.getElementById("save-button");
const quit_button = document.getElementById("quit-button");

const test_container = document.getElementById("tests-container");

const page_config = document.getElementById("page-config");
const page_tests = document.getElementById("page-tests");
const page_reboot = document.getElementById("page-reboot");

const wifi_ssid = document.getElementById("wifi-ssid");

const popup = document.getElementById("popup");

const wifi_list = document.getElementById("wifi-list");
const wifi_text = document.getElementById("wifi-text");

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
  switch (mode) {
    case 1:
      mode_config_http.classList.remove("hide");
      break;
    case 2:
    case 3:
      mode_config_mqtt.classList.remove("hide");
      break;
    case 4:
      mode_config_zigbee.classList.remove("hide");
      break;
    case 5:
      mode_config_tuya.classList.remove("hide");
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
    svg.setAttribute("width", "25");
    svg.setAttribute("height", "25");
    svg.setAttribute("aria-hidden", "true");
    svg.setAttribute("focusable", "false");
    svg.setAttribute("class", "wifi-level");

    let text = document.createElement("h6");
    text.textContent = test.text;
    text.className = "black";

    let svg_container = document.createElement("div");
    svg_container.appendChild(svg);

    test_div.appendChild(svg_container);
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
    tests_list[i].state = RUNNING;
    update_tests_view();

    await fetch("/test-start?id=" + tests_list[i].id)
      .then((data) => {
        //if OK response
        if (!data.ok) {
          throw new Error("Test failed", data);
        }
        tests_list[i].state = SUCCESS;
        update_tests_view();
      })
      .catch((error) => {
        tests_list[i].state = FAILURE;
        update_tests_view();
      });
  }
}

var wifi_scanning = false;
function wifi_scan() {
  if (wifi_scanning) {
    return;
  }

  wifi_scanning = true;
  wifi_text.textContent = "Scan en cours...";

  fetch("/wifi-scan")
    .then((response) => response.json())
    .then((data) => {
      console.log(data);
      const ap = data["ap"];
      update_wifi_list(ap);
      wifi_text.textContent = "";
    })
    .catch((error) => {
      wifi_text.textContent = "Erreur lors du scan";
    })
    .finally(() => {
      wifi_scanning = false;
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

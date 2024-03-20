const linky_tic_auto_switch = document.getElementById("linky-tic-auto");
const linky_tic_auto_radio = document.getElementById("linky-mode-auto");
const linky_tic_mode_values = document.getElementsByName("linky-mode");
const linky_tic_mode_containers = document.getElementsByClassName("linky-selector-container");
const mode_selectors = document.getElementsByName("server-mode");
const mode_config_http = document.getElementById("http-conf");
const mode_config_mqtt = document.getElementById("mqtt-conf");
const mode_config_zigbee = document.getElementById("zigbee-conf");
const mode_config_tuya = document.getElementById("tuya-conf");

const save_button = document.getElementById("save-button");

const test_container = document.getElementById("tests-container");

const page_config = document.getElementById("page-config");
const page_tests = document.getElementById("page-tests");

const popup = document.getElementById("popup");

const PENDING = 0;
const RUNNING = 1;
const SUCCESS = 2;
const FAILURE = 3;

// prettier-ignore
const tests_list = [
  { id: 0, text: "Connexion au réseau WiFi",  state: PENDING },
  { id: 1, text: "Test du réseau WiFi",       state: PENDING },
  { id: 2, text: "Connexion au serveur MQTT", state: PENDING },
  { id: 3, text: "Envoi des données MQTT",    state: PENDING },
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
  for (let i = 0; i < linky_tic_mode_values.length; i++) {
    linky_tic_mode_values[i].checked = false;
  }
  if (mode == 2) {
    linky_tic_auto_switch.checked = true;
    linky_tic_auto_radio.checked = true;
    for (let i = 0; i < linky_tic_mode_containers.length; i++) {
      linky_tic_mode_containers[i].classList.add("disabled");
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
  switch (page) {
    case 1:
      page_config.classList.remove("hide");
      save_button.disabled = false;
      break;
    case 2:
      page_tests.classList.remove("hide");
      save_button.disabled = true;
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
    popup.classList.remove("hide");
  } else {
    popup.classList.add("hide");
  }
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

  update_config_view(1);
  update_linky_tic_mode(3);
  update_page_view(1);
  update_tests_view();
});

const linky_tic_auto_switch = document.getElementById("linky-tic-auto");
const linky_tic_mode_containers = document.getElementsByClassName("linky-selector-container");
const mode_selectors = document.getElementsByName("server-mode");
const mode_config_http = document.getElementById("http-conf");
const mode_config_mqtt = document.getElementById("mqtt-conf");
const mode_config_zigbee = document.getElementById("zigbee-conf");
const mode_config_tuya = document.getElementById("tuya-conf");

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

window.addEventListener("load", function () {
  linky_tic_auto_switch.addEventListener("change", (event) => {
    if (event.target.checked) {
      for (let i = 0; i < linky_tic_mode_containers.length; i++) {
        linky_tic_mode_containers[i].classList.add("disabled");
      }
    } else {
      for (let i = 0; i < linky_tic_mode_containers.length; i++) {
        linky_tic_mode_containers[i].classList.remove("disabled");
      }
    }
  });

  mode_selectors.forEach((selector) => {
    selector.addEventListener("change", (event) => {
      update_config_view(parseInt(event.target.value));
    });
  });

  update_config_view(1);
});

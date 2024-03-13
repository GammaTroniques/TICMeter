const linky_tic_auto_switch = document.getElementById("linky-tic-auto");
const linky_tic_mode_containers = document.getElementsByClassName("linky-selector-container");

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
});

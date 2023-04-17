function sendConfig() {
  const wifiSSID = document.getElementById("wifi-ssid").value;
  const wifiPassword = document.getElementById("wifi-passwd").value;

  //post data to server using form data
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "/save-config", true);
  xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  xhr.send("wifi-ssid=" + wifiSSID + "&wifi-password=" + wifiPassword);
  xhr.onload = function () {
    alert("Config saved!");
  };
}

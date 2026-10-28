function send(value) {
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", value, true);
    xhttp.send();
}

// Fill the mode badge and highlight the matching button
// (button labels are the exact mode names returned by /mode).
function showMode(mode) {
    // Firmware flashed before the switch to English still says "Automatique".
    if (mode === "Automatique") mode = "Auto";
    document.getElementById("mode").textContent = mode || "…";
    var buttons = document.querySelectorAll(".remote .button");
    for (var i = 0; i < buttons.length; i++) {
        var active = buttons[i].textContent.trim() === mode;
        buttons[i].classList.toggle("button-outline", !active);
    }
}

function getData() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function()
    {
        if(this.readyState == 4 && this.status == 200)
        {
            showMode(this.responseText);
        }
    };
    xhttp.open("GET", "mode", true);
    xhttp.send();
}

// Ask right away instead of waiting 2 s for the first refresh.
document.addEventListener("DOMContentLoaded", getData);
setInterval(getData, 2000);

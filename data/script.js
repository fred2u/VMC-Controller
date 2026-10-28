// VMC Controller web interface: sends the buttons, shows the last mode
// sent (polled from /mode every 2 s) and the board details from /info.

var POLL_MS = 2000;
var TIMEOUT_MS = 3000;
// Offline after this many failed polls in a row (one miss can happen on Wi-Fi).
var MISSES_BEFORE_OFFLINE = 2;

// Fan turn time per mode, in seconds (0 = still).
var FAN_SPEED = {
    "Low": 3, "Medium": 1.6, "High": 0.9, "Auto": 2, "Default": 2,
    "Max 15 min": 0.5, "Max 30 min": 0.5, "Max 60 min": 0.5
};

var misses = 0;
var infoLoaded = false;

// GET a URL; done(status, text) with status 0 on error or timeout.
function get(url, done) {
    var xhttp = new XMLHttpRequest();
    xhttp.timeout = TIMEOUT_MS;
    xhttp.onload = function() { done(this.status, this.responseText); };
    xhttp.onerror = xhttp.ontimeout = function() { done(0, ""); };
    xhttp.open("GET", url, true);
    xhttp.send();
}

function $(id) { return document.getElementById(id); }

function buttons() { return document.querySelectorAll("button[data-cmd]"); }

function send(button) {
    button.classList.add("sending");
    get(button.getAttribute("data-cmd"), function(status, text) {
        button.classList.remove("sending");
        // 409: refused by the firmware (Max in High), the ESP is still online.
        setOnline(status != 0);
        if (status == 409) $("until").textContent = text;
        // The ESP presses the button within a second: show it soon.
        setTimeout(getData, 800);
    });
}

function setOnline(online) {
    misses = online ? 0 : misses + 1;
    var offline = misses >= MISSES_BEFORE_OFFLINE;
    var link = $("link");
    if (online) {
        link.textContent = "Online";
        link.className = "link online";
    } else if (offline) {
        link.textContent = "Offline";
        link.className = "link offline";
    }
    document.body.classList.toggle("offline", offline);
    if (online && !infoLoaded) getInfo();
}

// Show the mode and highlight the matching button.
function showMode(mode) {
    // Firmware flashed before the switch to English still says "Automatique".
    if (mode === "Automatique") mode = "Auto";
    $("mode").textContent = mode || "…";
    var isMax = mode.indexOf("Max") === 0;
    $("until").textContent = isMax ? "Then back to the previous mode" : "";
    $("fan").style.setProperty("--spin", (FAN_SPEED[mode] || 0) + "s");

    // The firmware refuses Max while in High (the remote has no boost there).
    var maxBlocked = mode === "High";
    $("boost-hint").textContent = maxBlocked ? "· not in High" : "";

    var list = buttons();
    for (var i = 0; i < list.length; i++) {
        var b = list[i];
        b.classList.toggle("active", b.getAttribute("data-mode") === mode);
        if (b.getAttribute("data-cmd").indexOf("max") === 0) b.disabled = maxBlocked;
    }
}

function getData() {
    get("mode", function(status, text) {
        setOnline(status == 200);
        if (status == 200) showMode(text);
    });
}

function fill(id, value) { $(id).textContent = value || "unknown"; }

// Board details, asked once (they only change on a new firmware or network).
// Firmware older than /info only has /version.
function getInfo() {
    get("info", function(status, text) {
        if (status == 200) {
            infoLoaded = true;
            var info = {};
            try { info = JSON.parse(text); } catch (e) {}
            fill("version", info.version);
            fill("ip", info.ip);
            fill("hostname", info.hostname);
            fill("rssi", info.rssi ? info.rssi + " dBm" : "");
        } else if (status == 404) {
            infoLoaded = true;
            fill("ip", location.hostname);
            fill("hostname", "");
            fill("rssi", "");
            get("version", function(s, t) { fill("version", s == 200 ? t : ""); });
        }
    });
}

document.addEventListener("DOMContentLoaded", function() {
    var list = buttons();
    for (var i = 0; i < list.length; i++) {
        list[i].addEventListener("click", function() { send(this); });
    }
    getData();
    setInterval(getData, POLL_MS);
});

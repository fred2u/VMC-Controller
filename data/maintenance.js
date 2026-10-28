// Maintenance page: pairing and reset of the remote, ESP details from
// /info, restart and Wi-Fi reset. The actions are POST requests.

var POLL_MS = 5000;
var TIMEOUT_MS = 3000;

function $(id) { return document.getElementById(id); }

// done(status, text) with status 0 on error or timeout.
function request(method, url, done) {
    var xhttp = new XMLHttpRequest();
    xhttp.timeout = TIMEOUT_MS;
    xhttp.onload = function() { done(this.status, this.responseText); };
    xhttp.onerror = xhttp.ontimeout = function() { done(0, ""); };
    xhttp.open(method, url, true);
    xhttp.send();
}

function setOnline(online) {
    var link = $("link");
    link.textContent = online ? "Online" : "Offline";
    link.className = "link " + (online ? "online" : "offline");
}

function notice(text) { $("notice").textContent = text; }

// 3725 -> "1 h 2 min", 200000 -> "2 d 7 h"
function duration(seconds) {
    var d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600),
        m = Math.floor(seconds % 3600 / 60);
    if (d) return d + " d " + h + " h";
    if (h) return h + " h " + m + " min";
    return m + " min";
}

function fill(id, value) { $(id).textContent = value === undefined || value === "" ? "unknown" : value; }

function getInfo() {
    request("GET", "info", function(status, text) {
        setOnline(status == 200);
        if (status != 200) return;
        var info = {};
        try { info = JSON.parse(text); } catch (e) {}
        fill("version", info.version);
        fill("uptime", info.uptime === undefined ? "" : duration(info.uptime));
        fill("reset", info.reset);
        fill("heap", info.heap ? Math.round(info.heap / 1024) + " KB" : "");
        fill("ssid", info.ssid);
        fill("rssi", info.rssi ? info.rssi + " dBm" : "");
        fill("ip", info.ip);
        fill("hostname", info.hostname);
        $("update").hidden = !info.update;
        $("update-off").hidden = !!info.update;
    });
}

function post(button) {
    var question = button.getAttribute("data-confirm");
    if (question && !confirm(question)) return;
    button.classList.add("sending");
    button.disabled = true;
    request("POST", button.getAttribute("data-post"), function(status, text) {
        button.classList.remove("sending");
        button.disabled = false;
        setOnline(status != 0);
        notice(status == 200 ? text + "…" : "Not sent (" + (status || "no answer") + ")");
    });
}

// Sends the chosen .bin file to /update/firmware or /update/web.
function update(event) {
    event.preventDefault();
    var file = $("update-file").files[0];
    if (!file) { notice("Choose a .bin file first"); return; }
    var kind = $("update-kind").value;
    if (!confirm("Update the " + (kind == "web" ? "web interface" : "firmware") + " with " + file.name + "?")) return;
    var data = new FormData();
    data.append("file", file, file.name);
    var xhttp = new XMLHttpRequest();
    var bar = $("update-progress");
    bar.hidden = false;
    xhttp.upload.onprogress = function(e) {
        if (e.lengthComputable) bar.value = Math.round(e.loaded * 100 / e.total);
    };
    xhttp.onload = function() {
        bar.hidden = true;
        notice(this.status == 200 ? this.responseText + "…" : this.responseText || "Update failed (" + this.status + ")");
    };
    xhttp.onerror = function() { bar.hidden = true; notice("Update failed (no answer)"); };
    xhttp.open("POST", "update/" + kind, true);
    xhttp.send(data);
    notice("Sending " + file.name + "…");
}

document.addEventListener("DOMContentLoaded", function() {
    $("update").addEventListener("submit", update);
    var list = document.querySelectorAll("button[data-post]");
    for (var i = 0; i < list.length; i++) {
        list[i].addEventListener("click", function() { post(this); });
    }
    getInfo();
    setInterval(getInfo, POLL_MS);
});

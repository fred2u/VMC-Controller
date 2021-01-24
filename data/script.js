function send(value) {
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", value, true);
    xhttp.send();
}

setInterval(function getData()
{
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function()
    {
        if(this.readyState == 4 && this.status == 200)
        {
            document.getElementById("mode").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "mode", true);
    xhttp.send();
}, 2000);
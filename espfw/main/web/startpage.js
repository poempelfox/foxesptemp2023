var getJSON = function(url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'json';
    xhr.onload = function() {
      var status = xhr.status;
      if (status === 200) {
        callback(null, xhr.response);
      } else {
        callback(status, xhr.response);
      }
    };
    xhr.send();
};
function updrcvd(err, data) {
  if (err != null) {
    document.getElementById("ts").innerHTML = "Update failed.";
  } else {
    for (let k in data) {
      if (document.getElementById(k) != null) {
        if ((k === "ts") || (k === "lastsht4xheat")) {
          var jsts = new Date(data[k] * 1000);
          document.getElementById(k).innerHTML = data[k] + " (" + ((data[k] == 0) ? "NEVER" : jsts.toISOString()
) + ")";
        } else {
          document.getElementById(k).innerHTML = data[k];
        }
      }
    }
  }
}
function updatethings() {
  getJSON('/json', updrcvd);
}
var myrefresher = setInterval(updatethings, 30000);


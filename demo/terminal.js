
var worker;
var sampleImageData;
var sampleVideoData;
var outputElement;
var isWorkerLoaded = false;
var isSupported = (function() {
  return document.querySelector && window.URL && window.Worker;
})();

function isReady() {
  return isWorkerLoaded && sampleImageData && sampleVideoData;
}

function showLoader() {

}

function retrieveSampleImage() {
  var oReq = new XMLHttpRequest();
  oReq.open("GET", "bigbuckbunny.jpg", true);
  oReq.responseType = "arraybuffer";

  oReq.onload = function (oEvent) {
    var arrayBuffer = oReq.response;
    if (arrayBuffer) {
      sampleImageData = new Uint8Array(arrayBuffer);
    }
  };

  oReq.send(null);
}

function retrieveSampleVideo() {
  var oReq = new XMLHttpRequest();
  oReq.open("GET", "bigbuckbunny.webm", true);
  oReq.responseType = "arraybuffer";

  oReq.onload = function (oEvent) {
    var arrayBuffer = oReq.response;
    if (arrayBuffer) {
      sampleVideoData = new Uint8Array(arrayBuffer);
    }
  };

  oReq.send(null);
}


function runCommand(text) {
  if (isReady()) {
    var args = text.split(" ");
    worker.postMessage({
      type: "command",
      arguments: args,
      files: [
        {
          "name": "input.jpeg",
          "data": sampleImageData
        },
        {
          "name": "input.webm",
          "data": sampleVideoData
        }
      ]
    });
  }
}

function getDownloadLink(fileData, fileName) {
  if (fileName.indexOf(".jpeg") > -1) {
    var blob = new Blob([fileData]);
    var src = window.URL.createObjectURL(blob);
    var img = document.createElement('img');

    img.src = src;
    return img;
  }
  else {
    var a = document.createElement('a');
    a.download = fileName;
    var blob = new Blob([fileData]);
    var src = window.URL.createObjectURL(blob);
    a.href = src;
    a.textContent = 'Click here to download ' + fileName + "!";
    return a;
  }
}

function initWorker() {
  worker = new Worker("worker.js");
  worker.onmessage = function (event) {
    var message = event.data;
    if (message.type == "ready") {
      isWorkerLoaded = true;
    } else if (message.type == "stdout") {
      outputElement.textContent += message.data + "\r\n";
    } else if (message.type == "start") {
      outputElement.textContent = "Worker has received command\r\n";
      showLoader();
    } else if (message.type == "done") {
      var buffers = message.data;
        console.log(buffers);
      buffers.forEach(function(file) {
        console.log(file);
        document.body.appendChild(getDownloadLink(file.data, file.name));
      });
    } else if (message.type == "ready") {
      workerReady();
    }
  };
}

document.addEventListener("DOMContentLoaded", function() {

  initWorker();
  retrieveSampleVideo();
  retrieveSampleImage();

  var inputElement = document.querySelector("#input");
  outputElement = document.querySelector("#output");

  inputElement.addEventListener("keydown", function(e) {
    console.log(e, e.keyCode);
    if (e.keyCode === 13) {
      runCommand(inputElement.value);
    }
  }, false);
  document.querySelector("#run").addEventListener("click", function() {
    runCommand(inputElement.value);
  });

  [].forEach.call(document.querySelectorAll(".sample"), function(link) {
    link.addEventListener("click", function(e) {
      inputElement.value = this.textContent;
      runCommand(inputElement.value);
      e.preventDefault();
    });
  });

});
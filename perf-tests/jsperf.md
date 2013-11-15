http://jsperf.com/videoencode/edit

Html Markup

    <script src="http://aaronm67.com/videoconverter.js/build/ffmpeg.js"></script>

    <pre></pre>


Setup Script

    var data = "data:image/png;base64,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";


    function dataURItoArrayBuffer(dataURI) {
        var byteString = atob(dataURI.split(',')[1]);
        var mimeString = dataURI.split(',')[0].split(':')[1].split(';')[0]
        var ab = new ArrayBuffer(byteString.length);
        var ia = new Uint8Array(ab);
        for (var i = 0; i < byteString.length; i++) {
            ia[i] = byteString.charCodeAt(i);
        }
        return ia;
    }

    var byteArray = dataURItoArrayBuffer(data);



Test 

    function run() {
        var mod = {
            print: function () { },
            printErr: function () { },
                'arguments': ['-i', 'input.png', 'output/output.jpeg'],
                'files': [{
                    data: data,
                    name: 'input.png'
                }]
        };
        ffmpeg_run(mod);
    }

    run();



Test 2

    function run() {
        var mod = {
            print: function (text) {
                document.querySelector("pre").textContent += "\n" + text;
            },
            printErr: function (text) {
                document.querySelector("pre").textContent += "\n" + text;
            },
            'arguments': ['-i', 'input.png', 'output/output.jpeg'],
            'files': [{
                data: data,
                name: 'input.png'
            }]
        };
        ffmpeg_run(mod);
    }

    run();


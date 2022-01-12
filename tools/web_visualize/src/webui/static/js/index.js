
function imageURL(url) {
    var fnode = document.createElement("form");
    fnode.role = "form";
    fnode.action = "detection_url";
    fnode.method = "get";
    var inode = document.createElement("input");
    inode.type = "text";
    inode.name = "image_url";
    inode.id = "image_url";
    inode.value = url;
    fnode.appendChild(inode);
    document.body.appendChild(fnode);
    let opt = document.createElement('textarea')
    opt.style.display = 'none'
    opt.name = 'web_type'
    opt.value = $('#demo-selector').value
    fnode.appendChild(opt)
    fnode.submit();
};
function imageUpLoad() {
    var fnode = $("#form-upload");
    let opt = document.createElement('textarea')
    opt.style.display = 'none'
    opt.name = 'web_type'
    opt.value = $('#demo-selector').value
    fnode.appendChild(opt)
    fnode.submit();
};
function mainWindowImg(src) {
    var drawing = document.getElementsByClassName("demo-canvas-centerlize")[0];
    var cxt = drawing.getContext("2d");
    var img = new Image();
    var dwidth = drawing.width;
    var dheight = drawing.height;
    var x, y, width, height;
    img.src = src;
    img.onload = function() {
         if ((img.width * 1.0 / dwidth) > (img.height * 1.0 / dheight)) {
             var scale = img.width * 1.0 / dwidth;
             if (scale < 1){
                 x = (dwidth - img.width) / 2;
                 y = (dheight - img.height) / 2;
                 width = img.width;
                 height = img.height;
             } else {
                 width = img.width / scale;
                 height = img.height / scale;
                 x = 0;
                 y = (dheight - height) / 2;
             }
         } else {
             var scale = img.height * 1.0 / dheight;
             if (scale < 1){
                 x = (dwidth - img.width) / 2;
                 y = (dheight - img.height) / 2;
                 width = img.width;
                 height = img.height;
             } else {
                 width = img.width / scale;
                 height = img.height / scale;
                 x = (dwidth - width) / 2;
                 y = 0;
             }
        }
        cxt.drawImage(img, x, y, width, height);
    }
};
function mainWindowError(str) {
    var drawing = document.getElementsByClassName("demo-canvas-centerlize")[0];
    var cxt = drawing.getContext("2d");
    cxt.font = '30px "微软雅黑"';
    cxt.fillStyle = "black";
    cxt.textBaseline = "middle";
    cxt.textAlign = "center";
    cxt.fillText(str, 500, 300);
};
function parseJSON(str) {
    var objs = str.objs;
    var jnode = document.getElementsByClassName("demo-json-content")[0];
    var snode = document.createTextNode("{\n");
    jnode.appendChild(snode);
    var objs_node = document.createElement("span");
    var objs_txt = document.createTextNode('\t\"objs\" :');
    objs_node.className = "hljs-attr";
    objs_node.appendChild(objs_txt);
    jnode.appendChild(objs_node);
    snode = document.createTextNode("[\n");
    jnode.appendChild(snode);
    for (i = 0; i < objs.length; i++){
        var attr_id = document.createElement("span");
        var txt_id = document.createTextNode('\t\t\t\"id\" : ');
        var value_id = document.createElement("span");
        var str_id = document.createTextNode(objs[i].id);
        var attr_name = document.createElement("span");
        var txt_name = document.createTextNode('\t\t\t\"name\" : ');
        var value_name = document.createElement("span");
        var str_name = document.createTextNode(objs[i].name);
        var attr_score = document.createElement("span");
        var txt_score = document.createTextNode('\t\t\t\"score\" : ');
        var value_score = document.createElement("span");
        var num_score = document.createTextNode(objs[i].score);
        var attr_x = document.createElement("span");
        var txt_x = document.createTextNode("\t\t\t\t\"x\" : ");
        var value_x = document.createElement("span");
        var num_x = document.createTextNode(objs[i].bbx.x);
        var attr_y = document.createElement("span");
        var txt_y = document.createTextNode("\t\t\t\t\"y\" : ");
        var value_y = document.createElement("span");
        var num_y = document.createTextNode(objs[i].bbx.y);
        var attr_w = document.createElement("span");
        var txt_w = document.createTextNode("\t\t\t\t\"w\" : ");
        var value_w = document.createElement("span");
        var num_w = document.createTextNode(objs[i].bbx.w);
        var attr_h = document.createElement("span");
        var txt_h = document.createTextNode("\t\t\t\t\"h\" : ");
        var value_h = document.createElement("span");
        var num_h = document.createTextNode(objs[i].bbx.h);
        snode = document.createTextNode("\t\t{\n");
        jnode.appendChild(snode);
        attr_id.className = "hljs-attr";
        attr_id.appendChild(txt_id);
        jnode.appendChild(attr_id);
        value_id.className = "hljs-string";
        value_id.appendChild(str_id);
        jnode.appendChild(value_id);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        attr_name.className = "hljs-attr";
        attr_name.appendChild(txt_name);
        jnode.appendChild(attr_name);
        value_name.className = "hljs-string";
        value_name.appendChild(str_name);
        jnode.appendChild(value_name);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        attr_score.className = "hljs-attr";
        attr_score.appendChild(txt_score);
        jnode.appendChild(attr_score);
        value_score.className = "hljs-number";
        value_score.appendChild(num_score);
        jnode.appendChild(value_score);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        var attr_bbx = document.createElement("span");
        var txt_bbx = document.createTextNode('\t\t\t\"bbx\" : ');
        attr_bbx.className = "hljs-attr";
        attr_bbx.appendChild(txt_bbx);
        jnode.appendChild(attr_bbx);
        var symbol_node = document.createTextNode("{\n");
        jnode.appendChild(symbol_node);
        attr_x.className = "hljs-attr";
        attr_x.appendChild(txt_x);
        jnode.appendChild(attr_x);
        value_x.className = "hljs-number";
        value_x.appendChild(num_x);
        jnode.appendChild(value_x);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        attr_y.className = "hljs-attr";
        attr_y.appendChild(txt_y);
        jnode.appendChild(attr_y);
        value_y.className = "hljs-number";
        value_y.appendChild(num_y);
        jnode.appendChild(value_y);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        attr_w.className = "hljs-attr";
        attr_w.appendChild(txt_w);
        jnode.appendChild(attr_w);
        value_w.className = "hljs-number";
        value_w.appendChild(num_w);
        jnode.appendChild(value_w);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        attr_h.className = "hljs-attr";
        attr_h.appendChild(txt_h);
        jnode.appendChild(attr_h);
        value_h.className = "hljs-number";
        value_h.appendChild(num_h);
        jnode.appendChild(value_h);
        var comma_node = document.createTextNode(",\n");
        jnode.appendChild(comma_node);
        var symbol_node = document.createTextNode("\t\t\t}\n");
        jnode.appendChild(symbol_node);
        var symbol_node = document.createTextNode("\t\t},\n");
        jnode.appendChild(symbol_node);
    }
    var symbol_node = document.createTextNode("\t]\n");
    jnode.appendChild(symbol_node);
    var symbol_node = document.createTextNode("}");
    jnode.appendChild(symbol_node);
};

function scrollIsAtBottom() {
    let element = document.getElementById("pipeline-status");
    return (element.scrollHeight - element.clientHeight - 21 <= element.scrollTop);
}

function updateScroll() {
    let element = document.getElementById("pipeline-status");
    element.scrollTop = element.scrollHeight;
}

function switchWindow(){
    var cnodes = document.getElementsByClassName("demo-collapse-item-cnt");
    var htmp = cnodes[0].style.height;
    var otmp = cnodes[0].style.opacity;
    cnodes[0].style.height = cnodes[1].style.height;
    cnodes[0].style.opacity = cnodes[1].style.opacity;
    cnodes[1].style.height = htmp;
    cnodes[1].style.opacity = otmp;
    var req = $("#request");
    var res = $("#response");
    var qtmp = req.className;
    req.className = res.className;
    res.className = qtmp;
    if (cnodes[1].style.height != "0px") {
        updateScroll();
    }
};

function showStatus() {
    if (document.getElementsByClassName("demo-collapse-item-cnt")[1].style.height == "0px") {
        switchWindow();
    }
}
function showConfig() {
    if (document.getElementsByClassName("demo-collapse-item-cnt")[0].style.height == "0px") {
        switchWindow();
    }
}
// function ResetDisplay() {
//     $("#demo-source-video").css("display", "none");
//     $("#demo-source-image").css("display", "none");
//     $("#demo-result").css("display", "none");
// }

$(document).ready(function(){
    var current_json = "";
    // var src_file_name = "";
    var demo_is_running = false;
    // Get json from design
    var custom_json = window.sessionStorage.getItem("designedJson");
    sessionStorage.clear();

    // If dump from design page, show designed json
    if (custom_json != null) {
      $("#demo-selector").val("custom");
      $("#demo-selector").find("option[text='Custom Design']").attr("selected",true);
    }
    handleDemoSelectChange();

    var demo_mode = "preview";
    const bindEvent = () => {
        /*
        $("#source-selector").change(() => {
            handleSourceSelectChange();
        });
        */
        $("#demo-selector").change(() => {
            handleDemoSelectChange();
        });
        $(".btn-design").click(() => {
            window.location.href="/design";
        });
        $(".btn-run").click(() => {
            if (!demo_is_running) {
                runDemo(demo_mode);
            } else {
                showMsg("Demo is running");
            }
        });
        $(".btn-stop").click(() => {
            if (demo_is_running) {
                stopDemo(demo_mode);
            } else {
                showMsg("Demo is NOT running");
            }
        });
        $("#source-file").click(() => {
            $('#source-file').prop("value", "");
        });
        $("#source-file").change(() => {
            // ResetDisplay()
            // var reader = new FileReader();
            file = $("#source-file")[0].files[0];
            if (file) {
                $('#upload-file-name').text(file.name);
                // if (FileIsImage(file.name)) {
                //     reader.readAsDataURL(file);
                //     reader.onload = function(e) {
                //         $("#demo-source-image").css("display", "block");
                //         $("#demo-source-image").attr("src", this.result);
                //     };
                // } else if (FileIsVideo(file.name)){
                //     var url = URL.createObjectURL(file);
                //     $("#demo-source-video").css("display", "block");
                //     $("#demo-source-video").attr("src", url);
                // } else {
                // }
                uploadFile();
            }
        });
        // $(".demo-preview").click(() => {
        //     if (demo_is_running) {
        //         showMsg("Please Stop first!");
        //         return;
        //     }
        //     $(".demo-preview").css("background-color", "rgb(186, 216, 213)");
        //     $(".demo-status").css("background-color", "white");
        //     $(".preview-container").css("display", "block");
        //     $(".demo-json").width("600px");
        //     demo_mode = "preview";
        // });
    };

    bindEvent();

    var imgExtension = ['jpeg', 'jpg'];
    var videoExtension = ['mp4', 'h264', 'h265', 'hevc'];
    function FileIsImage(filename) {
        if ($.inArray(filename.split('.').pop().toLowerCase(), imgExtension) == -1) {
            return false;
        }
        return true;
    }
    function FileIsVideo(filename) {
        if ($.inArray(filename.split('.').pop().toLowerCase(), videoExtension) == -1) {
            return false;
        }
        return true;
    }
    function FileIsJson(filename) {
        if ($.inArray(filename.split('.').pop().toLowerCase(), ['json']) == -1) {
            return false;
        }
        return true;
    }
    function displayJson(json, is_dir) {
        $("#json").css("display", "block");
        if (is_dir) {
            $.getJSON(json, {"random":Math.random()}, function (data) {
                current_json = JSON.stringify(data, null, 4);
                $("#json").val(current_json);
            });
        } else {
            if (json) {
              $("#json").val(json);
            } else {
              $("#json").val("Please click the 'Pipeline Design' button at the top right of the page to design your pipeline.");
            }
            current_json = json;
        }
    }
    function getJsonFile(selected_demo, selected_demo_text) {
        json = "";
        if (selected_demo === "classification") {
            json = "apps/resnet50.json";
        } else if (selected_demo === "detection") {
            json = "apps/ssd.json";
        } else if (selected_demo === "tracking") {
            json = "apps/yolov3_track.json";
        } else if (selected_demo === "yolov3_mlu370") {
            json = "apps/yolov3_mlu370.json";
        } else {
            json = "/user/" + selected_demo_text;
        }
        return json;
    }

    function handleDemoSelectChange(){
        let select_demo = $("#demo-selector").val();
        let select_demo_text = $("#demo-selector option:selected").text();
        $("#json").css("display", "none");
        let is_dir = false;
        let json = "";
        if (select_demo === "custom") {
            json = custom_json;
        } else {
            json = "../static/json/" + getJsonFile(select_demo, select_demo_text)
            is_dir = true;
        }
        displayJson(json, is_dir);
    }

    function postJsonAndFilename(mode) {
        var formData = new FormData();
        formData.append("filename", $("#source-selector option:selected").text());
        let select_demo = $("#demo-selector").val();
        let select_demo_text = $("#demo-selector option:selected").text();
        if (select_demo === "custom") {
            formData.append("json", "/user/custom_config.json");
        } else {
            formData.append("json", getJsonFile(select_demo, select_demo_text));
        }
        // formData.append("json", current_json);
        formData.append("mode", mode);
        postData("./runDemo", formData/*, function logReturnValue(callback) {showMsg(callback);}*/);
    }

    function getDataPeriodic(url, processFunc, interval) {
        var intervalMethod = self.setInterval(function() {
            getData(url, processFunc, intervalMethod);
        }, interval);
    }

    function getDemoConsole() {
        document.getElementById("demo-console-result").innerHTML = "";
        function printConsoleOutput(callback, intervalMethod) {
          postData("./getPreviewStatus", 0,
            function (callback) {
              if (callback == "False") {
                demo_is_running = false;
                clearInterval(intervalMethod);
              }
            }
          );
          if(callback == "") return;
          callback.split(/[\n]/).forEach(line => {
            if(line == "") return;
            let is_bottom = scrollIsAtBottom();
            document.getElementById("demo-console-result").innerHTML += line + "<br>";
            if (is_bottom) updateScroll();
        });

        }
        getDataPeriodic("/getDemoResult", printConsoleOutput, 1000);
    }

    function getPreviewStatus() {
        var max_timeout_num = 5;
        var timeout = 0;
        function setDemoRunningFlag(callback, intervalMethod) {
            if (callback == "False") {
                demo_is_running = false;
                clearInterval(intervalMethod);
            } else if (callback == "True") {
                timeout = 0;
            } else {
                timeout += 1;
                if (timeout == max_timeout_num) {
                    demo_is_running = false;
                    clearInterval(intervalMethod);
                }
            }
        }
        getDataPeriodic("/getPreviewStatus", setDemoRunningFlag, 1000);
    }

    function getDemoResult(mode) {
        $.get("/getPreview");
        getDemoConsole();
    }

    function runDemo(mode) {
        demo_is_running = true;
        postJsonAndFilename(mode);
        getDemoResult(mode);
    }

    function stopDemo(mode) {
        var formData = new FormData();
        formData.append("mode", mode);
        postData("./stopDemo", formData,
            function (callback) {
                demo_is_running = false;
            }
        );
    }
    // add source
    var selection_id = 0;
    function addSelection(file_name) {
        if (FileIsJson(file_name)) {
            selector_name = "demo-selector";
            selector_group_name = "user-json-selector";
        } else {
            selector_name = "source-selector"
            selector_group_name = selector_name;
        }
        let current_file = $("#" + selector_name + "option:selected").val();
        let element = document.getElementById(selector_name);
        let exist = false;
        let option_text;
        for (var i = 0, len = element.options.length; i < len; i++ ) {
            opt = element.options[i];
            if (opt.text === file_name) {
                exist = true;
                option_text = opt.value;
                break;
            }
        }
        if (!exist) {
            option_text = "custom" + selection_id;
            $("<option>").val(option_text).text(file_name).appendTo("#" + selector_group_name);
            selection_id += 1;
        }

        if (!demo_is_running) {
            $("#" + selector_name).val(option_text).change();
        }
    }

    function uploadFile() {
        var file = $("#source-file")[0].files[0];
        var formData = new FormData();
        if (file) {
            formData.append("data", file);
            if (FileIsJson(file.name)) {
                formData.append("type", "json");
            } else if (FileIsImage(file.name) || FileIsVideo(file.name)) {
                formData.append("type", "media");
            } else {
                formData.append("type", "other");
            }
            function addFile(callback){
                showMsg(callback);
                if (formData.get("type") == "json" || formData.get("type") == "media") {
                  addSelection(file.name);
                }
            }
            postData("./uploadFile", formData, addFile, showMsg);
        }
    }
});

function showMsg(msg) {
    $("#prompt-message").html(msg).show(300).delay(1000).hide(300);
}

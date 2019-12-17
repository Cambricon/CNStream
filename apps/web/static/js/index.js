
function handleSelectChange(){
  console.log(document.getElementById('web_type_selector').value)
}

function showMessage(){
    var cnodes = document.getElementsByClassName("demo-collapse-item-cnt");
    var htmp = cnodes[0].style.height;
    var otmp = cnodes[0].style.opacity;
    cnodes[0].style.height = cnodes[1].style.height;
    cnodes[0].style.opacity = cnodes[1].style.opacity;
    cnodes[1].style.height = htmp;
    cnodes[1].style.opacity = otmp;
    var req = document.getElementById("request");
    var res = document.getElementById("response");
    var qtmp = req.className;
    req.className = res.className;
    res.className = qtmp;
};
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
    opt.value = document.getElementById('web_type_selector').value
    fnode.appendChild(opt)
    fnode.submit();
};
function imageUpLoad() {
    var fnode = document.getElementById("form-upload");
    let opt = document.createElement('textarea')
    opt.style.display = 'none'
    opt.name = 'web_type'
    opt.value = document.getElementById('web_type_selector').value
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

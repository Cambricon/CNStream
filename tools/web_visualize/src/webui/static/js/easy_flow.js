var chart;
var current_item = 0;

function checkDAG(json) {
    if (json.hasOwnProperty("graph")) {
        if (json["graph"] == "notDAG") {
            alert("Json config is invalid\nThe pipeline must be a DAG.");
            return false;
        } else if (json["graph"] == "invalid") {
            alert("Json config is invalid\nPlease make sure all modules are linked and the pipeline must be a DAG.");
            return false;
        }
    }
    return true;
}

function showJsonConfig() {
    var json = chart.toJson();
    if (checkDAG(json) == false) {
        return false;
    }
    var json_str = JSON.stringify(chart.toJson(), null, 4);
    $('#json-output').val(json_str);
    return true;
}

function showMessage(index) {
    if (index == 2 && !showJsonConfig()) return false;
    var cnodes = document.getElementsByClassName("demo-collapse-item-cnt");
    var otmp = cnodes[current_item].style.opacity;
    cnodes[current_item].style.height = cnodes[index].style.height;
    cnodes[current_item].style.opacity = cnodes[index].style.opacity;
    cnodes[index].style.height = ($("#right").height() - 50 * 4) + "px";
    cnodes[index].style.opacity = otmp;
    var req = document.getElementById("item-" + current_item);
    var res = document.getElementById("item-" + index);
    var qtmp = req.className;
    req.className = res.className;
    res.className = qtmp;
    current_item = index;
    return true;
};

function showJson() {
    return showMessage(2);
}

Chart.ready(() => {
    let start_x = 50;
    let start_y = 20;
    let layout_step = 80;
    let layout_count = 0;

    let current_node_id = null;
    let key_down = false;


    changeName = function() {
        let new_name = $('#txt_' + current_node_id).val();
        $("#" + current_node_id).text(new_name);
        $(this).parent('div').remove();

        let node = chart.changeNodeName(current_node_id, new_name);
        $("#name").val(new_name);
        let node_id = '#' + current_node_id;
        if (chart.getRemovable(current_node_id)) {
            let removeIcon = $('<div>').addClass('remove');
            $(node_id).append(removeIcon);
        }
        UpdateParamInfo();
    }


    onBlurChangeName = function() {
        if (!key_down) {
            changeName();
        }
    }


    onKeyEnterChangeName = function() {
        let e = window.event || arguments[0];
        if (e.keyCode == 13) {
            key_down = true;
            changeName();
        }
    }

    let createChart = function() {
        return new Chart($('#demo-chart'), {
            clickNode(data) {
                if (!data) return;
                showNodeInfo(data);
                current_node_id = data.node_id;
            },
            delNode(data) {
                hideNodeInfo();
            },
            dblClickNode(data) {
                if (!data) return;
                key_down = false;
                current_node_id = data.node_id;
                let org_name = $("#" + data.node_id).text();
                $("#" + data.node_id).text("");
                let input_text = $('<input>').addClass('input_txt')
                    .attr("id", "txt_" + data.node_id)
                    .attr("onBlur", "onBlurChangeName()")
                    .attr('onkeydown', "onKeyEnterChangeName()");
                input_text.val(org_name);
                $("#" + data.node_id).append(input_text);
                $('#txt_' + data.node_id).focus();
            }
        })
    };


    let createStartNode = function() {
        let start = chart.addNode(MODULES[0].name, start_x, start_y, {
            class: "node-start",
            data: MODULES[0],
            removable: false
        });
        start.addPort({ is_source: true });
        return start
    };


    const addNodeToChart = (name, params) => {
        params = params || {};
        params.data = params.data || {};
        params.class = name;

        let position = getPosition();
        let node = chart.addNode(name, position[0], position[1], params);
        if (node) {
          node.addPort({ is_source: true });
          node.addPort({ is_target: true, position: 'Top' });
        }
    };


    let getPosition = function() {
        if (layout_count == 23) { layout_count = 0; }
        layout_count = layout_count + 1;
        return new Array(start_x + layout_step * layout_count / 3.5, start_y + layout_step * (layout_count % 7));
    };


    let showNodeInfo = (data) => {
        if (!data) return;
        $('.module-name')[0].style.display = 'block';
        $('.module-desc')[0].style.display = 'block';
        $('.module-params')[0].style.display = 'block';
        $('.param-desc')[0].style.display = 'block';

        $('.right').find('.module-name').text(data.class_name || '');
        $('.right').find('.module-desc').text(data.desc || '');
        UpdateParamInfo();

        let list_html_params = `<br>`;
        let needed_params = data["name"] == "source" ? [] : ['name', 'parallelism', 'max_input_queue_size',];
        needed_params.forEach(v => {
            if (data.hasOwnProperty(v)) {
                list_html_params += `<div class='param-item'>${v}:<input id='${v}' value='${data[v]}'/></div>`;
            }
        });
        list_html_params += `<div class='hidden-custom-btn'>${"custom_params"} 🔽</div><div class='custom-params'>`;
        if (data.hasOwnProperty("custom_params")) {
            $.each(data["custom_params"], function(key, value) {
                list_html_params += `<div class='param-item'>· ${key}:<input id='c_${key}' value='${value}'/></div>`;
            });
        }
        list_html_params += `</div><br><div class='submit-params'>Submit</div>`;

        $('.module-params').html(list_html_params);
        $('.custom-params').toggle();
        $('#name').focus();

        let list_html_param_desc = `<br>`;
        if (PARAM_DESC.hasOwnProperty(data.class_name)) {
            $.each(PARAM_DESC[data.class_name], function(key, value) {
                if (key != "custom_params") {
                  list_html_param_desc += `<div class='param-desc-item'>${key}:<br><a class='desc-val'> ${value}</a></div>`;
                }
            });
            if (PARAM_DESC[data.class_name].hasOwnProperty("custom_params")) {
                list_html_param_desc += `<div class='hidden-custom-desc-btn'>${"custom_params"} 🔽</div><div class='custom-params-desc'>`;
                $.each(PARAM_DESC[data.class_name]["custom_params"], function(key, value) {
                    list_html_param_desc += `<div class='param-desc-item'>· ${key}:<br><a class='desc-val'> ${value}</a></div>`;
                });
            }

        }
        list_html_param_desc += `<br>`
        $('.param-desc').html(list_html_param_desc);
        $('.custom-params-desc').toggle();


        // add event listeners
        $('.hidden-custom-btn')[0].addEventListener('click', () => {
            $('.custom-params').toggle();
        })

        $('.submit-params')[0].addEventListener('click', () => {
            // update node parameters
            needed_params.forEach(v => {
                let str = $('#' + v).val();
                if (str == "") {
                    data[v] = "";
                } else {
                    var n = Number(str); // return NaN if isnot a Number
                    data[v] = !isNaN(n) ? n : str;
                }
            });
            if (data.hasOwnProperty("custom_params")) {
                $.each(data["custom_params"], function(key, value) {
                    let str = $('#c_' + key).val();
                    if (str == "")
                        data["custom_params"][key] = "";
                    else {
                        var n = Number(str); // return NaN if isnot a Number
                        data["custom_params"][key] = !isNaN(n) ? n : str;
                    }
                });
            }
            // replace module name
            let node_id = '#' + current_node_id;
            $(node_id).text(data.name);
            if (chart.getRemovable(current_node_id)) {
                let removeIcon = $('<div>').addClass('remove');
                $(node_id).append(removeIcon);
            }
        })

        if ($('.hidden-custom-desc-btn')[0] != null) {
            $('.hidden-custom-desc-btn')[0].addEventListener('click', () => {
                $('.custom-params-desc').toggle();
            })
        }
    };


    let hideNodeInfo = () => {
        $('.module-name')[0].style.display = 'none';
        $('.module-desc')[0].style.display = 'none';
        $('.module-params')[0].style.display = 'none';
        $('.param-desc')[0].style.display = 'none';
    };

    downloadFile = function() {
        let elementA = document.createElement('a');
        var json = chart.toJson();
        if (checkDAG(json) == false) {
            return;
        }
        elementA.setAttribute('href', 'data:text/plain;charset=utf-8,' + JSON.stringify(json, null, 4));
        elementA.setAttribute('download', "config.json");
        elementA.style.display = 'none';
        document.body.appendChild(elementA);
        elementA.click();
        document.body.removeChild(elementA);
    }

    function UpdateParamInfo() {
        if (current_item != 0 && current_item != 1) {
            showMessage(0);
        } else if (current_item == 2) {
            showJson();
        } else {
            showMessage(current_item);
        }
    }

    const bindEvent = () => {
        $(".pipeline-panel").on('click', '.btn-add', function(event) {
            let target = $(event.target);
            let node = target.data('node');
            addNodeToChart(node.name, {
                data: node
            });
        });

        $(".btn-gen").click(() => {
            showJson();
        });

        $(".btn-save").click(() => {
            if (showJson()) downloadFile();
        });

        $(".btn-clear").click(() => {
            $('#demo-chart').remove();
            chart.clear();
            $('<div id="demo-chart"></div>').appendTo($('.middle'));
            chart = createChart();
            start_node = createStartNode();
            hideNodeInfo();
            layout_count = 0;
            UpdateParamInfo();
        });

        $(".btn-return").click(() => {
	    alert("Current pipeline will be cleared!");
            window.location.href="/home";
        });

        $(".btn-done").click(() => {
            var json = JSON.stringify(chart.toJson(), null, 4);
            if (checkDAG(json) == true) {
              postData("./saveJson", json, function() {}, function(){}, 'application/json; charset=UTF-8');
              window.sessionStorage.setItem("designedJson", json);
              window.location.href="/home";
            }
        });
    };


    // chart begin here
    chart = createChart();
    chart.setMaxNodeNum(64);
    let start_node = createStartNode();
    bindEvent();

    let list_html_nodes = '';
    MODULES.forEach(node => {
        if (node.name != "source") {
            list_html_nodes += `<li><button class='btn-add' href='javascript:void(0)'>${node.label}</button></li>`;
        }
    });
    $('.nodes').html(list_html_nodes);
    $('.nodes').find('.btn-add').each(function(index) {
        $(this).data('node', $.extend(true, {}, MODULES[index + 1]));
    });
});

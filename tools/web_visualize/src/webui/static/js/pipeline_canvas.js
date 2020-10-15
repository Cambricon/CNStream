/**
 * @class Chart Node
 *
 * @param {String} id            node id
 * @param {String} name          node name
 * @param {Number} x             node coordinate x
 * @param {Number} y             node coordinate y
 * @param {String} class_name    node class name
 * @param {Object} data          node data
 * @param {String} data.node_id  node id
 * @param {Boolean} removable    whether node is removable
 * @param {Object} container     Chart
 */
let ChartNode = function(id, name, x, y, options) {
    this._id = id;
    this._name = name;
    this._x = x;
    this._y = y;
    this._class_name = options.class || '';
    this._data = $.extend(true, {}, options && options.data || {});
    this._data.node_id = id;
    if (options.removable != null) {
      this._removable = options.removable
    } else {
      this._removable = true;
    }
    this._element = null;
    this._container = null;
    this._js_plumb = null;
};


ChartNode.prototype.setPlumb = function (plumb) {
    this._js_plumb = plumb;
};


ChartNode.prototype.getId = function() {
    return this._id;
};

ChartNode.prototype.getName = function() {
    return this._name;
};

ChartNode.prototype.getRemovable = function() {
    return this._removable;
};

ChartNode.prototype.getData = function() {
    return this._data || {};
};


ChartNode.prototype.appendTo = function(container) {
    if (!container) {
        console.error('node container is null!');
        return;
    }

    // create and insert dom node
    let node = $('<div>').addClass(`window task ${this._class_name}`)
        .attr('id', this._id)
        .css({
            left: this._x + 'px',
            top: this._y + 'px'
        })
        .text(this._name)
        .data('node_data', this._data)
        .data('chart_node', this);

    // add remove icon
    if (this._removable) {
        let removeIcon = $('<div>').addClass('remove');
        node.append(removeIcon);
    }

    container.append(node);
    this._js_plumb.draggable(node, { grid: [0.1, 0.1] });

    this._element = node;
    this._container = container;
};


// port label position
ChartNode.label_position = {
    'Bottom': [6, 2.5],
    'Top': [6, -2.5],
    'Left': [0, 0],
    'right': [0, 0],
};


/**
 * Add link port
 * @param {Object} options parameter
 * @param {String} [options.color=#0096f2]    port color
 * @param {Boolean} [options.is_source=false]
 * @param {Boolean} [options.is_target=false]
 * @param {String} [options.label]            port name
 * @param {String} [options.position=bottom]  port position
 */
ChartNode.prototype.addPort = function(options) {
    let pos = options.position || 'Bottom';
    let label_position = ChartNode.label_position[pos];
    let endpointConf = {
        endpoint: "Dot",
        paintStyle: {
            strokeStyle: options.color || '#0096f2',
            radius: 6,
            lineWidth: 2
        },
        anchor: pos,
        isSource: !!options.is_source,
        isTarget: !!options.is_target,
        maxConnections: -1,
        Connector:["Flowchart"],
        dragOptions: {},
        overlays: [
            ["Label", {
                location: label_position,
                label: options.label || '',
                cssClass: "endpoint-label-lkiarest"
            }]
        ],
        allowLoopback:false
    };

    this._js_plumb.addEndpoint(this._element, endpointConf);
};


ChartNode.prototype.dispose = function() {
    let element = this._element;
    let dom_element = element.get(0);
    this._js_plumb.detachAllConnections(dom_element);
    this._js_plumb.remove(dom_element);
    element.remove();
};


/**
 * @class Chart
 *
 * @param {Object} container     canvas
 * @param {Object} nodes         nodes of the chart
 * @param {Number} seed_id       for generating node id
 */
let Chart = function(container, options) {
    this._container = container;
    this._nodes = [];
    this._seed_id = 0;
    this._max_node_num = 0;
    this._js_plumb = null;

    this.init(options);
};


/**
 * Init chart
 * @param {Object} [options] param
 * @param {Function} [options.clickNode] callback of clicking node event. param is the data of the clicked node
 * @param {Function} [options.delNode] callback of remove node event. param is the data of the removed node
 * @param {Function} [options.connNode] callback of remove node event. param is the data of the removed node
 * @param {Function} [options.disconnNode] callback of remove node event. param is the data of the removed node
 */
Chart.prototype.init = function(options) {
    this._js_plumb = jsPlumb.getInstance();
    this._js_plumb.importDefaults({
        ConnectionOverlays: [
            ["PlainArrow", {
                width: 10,
                location: 1,
                id: "arrow",
                length: 8
            }]
        ],
        DragOptions : { cursor: 'pointer', zIndex:2000 },
        EndpointStyles : [{ fillStyle:'#225588' }, { fillStyle:'#558822' }],
        Endpoints : [ [ "Dot", { radius:2 } ], [ "Dot", { radius: 2 } ]],
        Connector:["Flowchart"],
    });

    this._container.addClass('pipeline-canvas');

    // click event
    if (options && options.clickNode) {
        this._container.on('click', '.task', event => {
            options.clickNode.call(this, $(event.target).data('node_data'));
        });
    }

    // del node event
    this._container.on('click', '.remove', event => {
        let del_node = $(event.target).parent().data('chart_node');
        if (del_node) {
            del_node.dispose();
            this.removeNode(del_node.getId());

            if (options && options.delNode) {
                options.delNode.call(this, del_node.getData());
            }
        }

        event.stopPropagation();
    });

    // click event
    if (options && options.dblClickNode) {
        this._container.on('dblclick', '.task', event => {
            options.dblClickNode.call(this, $(event.target).data('node_data'));
        });
    }

    // connect
    if (options && options.connNode) {
        this._js_plumb.bind("connection", function(info) {
            options.connNode.call(this);
        });
    }
    // disconnect
    if (options && options.disconnNode) {
        this._js_plumb.bind("connectionDetached", function(info) {
            options.disconnNode.call(this);
        });
    }
};


Chart.prototype.createNodeId = function() {
    return this._seed_id++;
};

Chart.prototype.setMaxNodeNum = function(num) {
    return this._max_node_num = num;
};

Chart.prototype.isUniqueName = function(name) {
    let nodes = this._nodes;
    for (let i = 0, len = nodes.length; i < len; i++) {
        if (nodes[i].getName() === name) {
            return false;
        }
    }
    return true;
}

/**
 * Add node to chart
 * @param {String} name    node name
 * @param {Number} x       node coordinate x
 * @param {Number} y       node coordinate y
 * @param {Object} options node parameter，refer to class ChartNode
 */
Chart.prototype.addNode = function(name, x, y, options) {
    if (this._max_node_num > 0 && this._nodes.length >= this._max_node_num) {
        alert("only support max nodes number: " + this._max_node_num);
        return;
    }
    let id = this.createNodeId();
    if (!this.isUniqueName(name)) {
        name = name + '-' + id;
    }
    let node = new ChartNode(id , name, x, y, options);
    node.setPlumb(this._js_plumb);
    node.appendTo(this._container);
    node._data.name = node._name;
    this._nodes.push(node);
    return node;
};


Chart.prototype.getNode = function(node_id) {
    let nodes = this._nodes;
    for (let i = 0, len = nodes.length; i < len; i++) {
        if (nodes[i].getId() === node_id) {
            return nodes[i];
        }
    }
};


Chart.prototype.changeNodeName = function(node_id, name) {
    let node = this.getNode(node_id);
    node._name = name;
    node._data.name = name;
};


Chart.prototype.removeNode = function(node_id) {
    let nodes = this._nodes;
    for (let i = 0, len = nodes.length; i < len; i++) {
        if (nodes[i].getId() === node_id) {
            nodes[i].dispose();
            nodes.splice(i, 1);
            break;
        }
    }
};

Chart.prototype.getRemovable = function(node_id) {
    let nodes = this._nodes;
    for (let i = 0, len = nodes.length; i < len; i++) {
        if (nodes[i].getId() === node_id) {
            return nodes[i].getRemovable();
        }
    }
};

function isDAG(names, node, nodes, valid_nodes_name) {
    var ret = true;
    if (node.hasOwnProperty("next_modules")) {
        node["next_modules"].forEach( next_name => {
            names.forEach( name => {
                if (next_name === name) {
                    ret = false;
                    return;
                }
            });
            if (!ret) {
                return;
            }
            valid_nodes_name.add(next_name);
            names.push(next_name);
            ret = isDAG(names, nodes[next_name], nodes, valid_nodes_name);
            names.pop();
        });
    }
    return ret;
}

Chart.prototype.toJson = function() {
    let nodes = {};
    let repeat_node_name = false;
    // get useful data from each node
    this._nodes.forEach(item => {
        let data = $.extend(true, {}, item._data);
        if (data["desc"]) delete data["desc"];
        if (data["label"]) delete data["label"];
        // check if nodes have unique name
        if (nodes[data.name]) {
            repeat_node_name = true;
            console.log("[Warnning] The name of nodes can not be the same. Assign a different name for [" + data.name + "]");
            data.name = data.name + "-" + item._id;
            item._name = data.name;
            item._data.name = data.name;
            $('#' + item._id).text(data.name);

            if (item._removable) {
                let removeIcon = $('<div>').addClass('remove');
                item._element.append(removeIcon);
            }
        }
        // delete data with empty string
        $.each(data, function(key, value) {
            if (value === "") delete data[key];
            if (key == "custom_params") {
                $.each(data["custom_params"], function(key, value) {
                    if (value === "") delete data["custom_params"][key];
                });
            }
        });
        nodes[data.name] = data;
    });

    // get all connections
    this._js_plumb.getConnections().forEach(connection => {
        let up_node = '';
        let down_node = '';
        $.each(nodes, function(key, value) {
            if (value['node_id'] == connection.sourceId) up_node = value.name;
            if (value['node_id'] == connection.targetId) down_node = value.name;
        });

        if (up_node != '' && down_node != '') {
            let up = nodes[up_node];
            let down_node_is_repeat = false;
            if (!up["next_modules"]) {
                up["next_modules"] = new Array();
            } else {
                $.each(up["next_modules"], function(i, value){
                    if (value == down_node) down_node_is_repeat = true;
                });
            }
            if (!down_node_is_repeat) {
                up["next_modules"].push(down_node);
            }
        }
    });

    let node_set = new Set();
    node_set.add("source");
    if(!isDAG(["source"], nodes["source"], nodes, node_set)) {
        return {graph: "notDAG"};
    }
    if (node_set.size != this._nodes.length) {
        return {graph: "invalid"};
    }

    $.each(nodes, function(key, value) {
        if (value.hasOwnProperty("node_id")) delete value["node_id"];
        if (value.hasOwnProperty("name")) delete value["name"];
    });
    return nodes;
};


Chart.prototype.clear = function() {
    this._nodes && this._nodes.forEach(item => {
        item.dispose();
    });

    this._nodes = [];
    this._js_plumb.detachAllConnections(this._container);
    this._js_plumb.removeAllEndpoints(this._container);
};


Chart.ready = (callback) => {
    jsPlumb.ready(callback);
};

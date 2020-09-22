window.PARAM_DESC = {}
PARAM_DESC["cnstream::DataSource"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        output_type: "Optional.<br> Default value: [cpu].<br> Optional values: [cpu] [mlu]<br> Desc: The output type.",
        decoder_type: "Optional.<br> Default value: [cpu].<br> Optional values: [cpu] [mlu]<br> Desc: The decoder type.",
        reuse_cndec_buf: "Optional.<br> Default value: [false].<br> Optional values: [false] [true]<br> Desc: Whether the codec buffer will be reused.<br> Note: This parameter is used when decoder type is [mlu]",
        input_buf_number: "Optional.<br> Default value: [2].<br> Optional values: integer <br> Desc: The input buffer number.",
        output_buf_number: "Optional.<br> Default value: [3].<br> Optional values: integer<br> Desc: The output buffer number.",
        interval: "Optional.<br> Default value: [1].<br> Optional values: integer<br> Desc: Process one frame for every ``interval`` frames.",
        device_id: "Required when MLU is used.<br> Optional values: integer <br> Desc: The device id.<br> Note: Set the value to -1 for CPU. Set the value for MLU in the range 0 - N.",
    },
};
PARAM_DESC["cnstream::Inferencer"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        model_path: "Optional.<br>The path of the offline model. <br> Optional values: [path]",
        func_name: "Optional. <br>The function name that is defined in the offline model. <br> Default value: [subnet0] <br> note:It could be found in Cambricon twins file. For most cases, it is 'subnet0' ",
        use_scaler: "Optional. <br> Default value: [false] <br> Optional value: [true] [false] <br> note: Whether use the scaler to preprocess the input. The scaler will not be used by default",
        preproc_name: "Optional. <br> Required. The class name for postprocess. The class specified by this name must inherited from class cnstream::Postproc when [object_infer] is false",
        postproc_name: "Optional. <br> The class name for preprocessing on CPU.",
        batching_timeout: "Optional. <br> Default value: [100] <br> Optional value: integer <br> note: The type of value is 100[ms]. type[float]. unit[ms]",
        threshold: "Optional. <br> Default value: [0] <br> Optional value: integer <br> Desc: The threshold of the confidence.",
        data_order: "Optional. <br> Default value: [NCHW] <br> Optional value: [NCHW] [NHWC] <br> Desc: the format of data",
        infer_interval: "Optional. <br> Optional value: integer <br> Desc: Process one frame for every ``infer_interval`` frames.",
        object_infer: "Optional. <br> Default value: [false] <br> Optional value: [true] [false] <br> Desc: Whether to infer with frame or detection object.",
        obj_filter_name: "Optional. <br> Optional value: [CarFilter] <br> Desc: The object filter method name.",
        keep_aspect_ratio: "Optional. <br> Default value: [false] <br> Optional value: [true] [false] <br> Desc: Keep aspect ratio, when the mlu is used for image processing.",
        device_id: "Optional. <br> Default value: [0] <br> Optional value: integer, less than 4 <br> Desc:  MLU device ordinal number",
        show_stats: "Optional. <br> Default value: [false] <br> Optional value: [true] [false] <br> Desc: Whether show inferencer performance statistics.",
        stats_db_name: "Optional. <br> Optional value: string <br> Desc: The directory to store the db file. e.g., ``dir1/dir2/detect.db``.",
    },
};

PARAM_DESC["cnstream::Osd"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        chinese_label_flag: "Optional. <br> Default value [false] <br> Optional value [false] [true] Desc: chinese label in the image",
        label_path: "Optional. <br> Default value [false] <br> Optional value [false] [true] Desc",
    }
};

PARAM_DESC["cnstream::Tracker"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        model_path: "Optional. <br> Desc: path of offline model",
        func_name: "Optional. <br> Default value: subnet0 <br> Desc: function name defined in the offline model. It can be found in the Cambricon twins description file",
        track_name: "Optional. <br> Default value: KCF <br> Desc: The algorithm name for track",
    }
};

PARAM_DESC["cnstream::RtspSink"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        http_port: "Optional. <br> Default value: [8080]",
        udp_port: "Optional. <br> Default value: [9554]",
        frame_rate: "Optional. <br> Default value: [25]",
        gop_size: "Optional. <br> Default value: [30]",
        kbit_rate: "Optional. <br> Default value: [512]",
        preproc_type: "Optional. <br> Default value: [cpu]",
        encoder_type: "Optional. <br> Default value: [mlu]",
        color_mode: "Optional. <br> Default value: [bgr]",
        view_mode: "Optional. <br> Default value: [single]",
        view_rows: "Optional. <br> Desc: rows of video to exhibit",
        view_cols: "Optional. <br> Desc: cols of video to exhibit",
        dst_width: "Optional. <br> Desc: destination of width",
        dst_height: "Optional. <br> Desc: destination of height",
        device_id: "Optional. <br> Default value: [0]",
    }
};

PARAM_DESC["cnstream::Encoder"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        dump_dir: "Optional. <br> Desc: output dir"
    }
};

PARAM_DESC["cnstream::Displayer"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        window_width: "Optional.<br> Default value: [50].<br> Optional values: [integer] <br> Desc: display window width",
        window_height: "Optional.<br> Default value: [50].<br> Optional values: integer <br> Desc:  display window height",
        refresh_rate: "Optional.<br> Default value: [30].<br> Optional values: integer <br> Desc: display refresh rate",
        max_channels: "Optional.<br> Default value: [64].<br> Optional values: integer <br> Desc: max channels to display",
        full_screen: "Optional.<br> Default value: [true].<br> Optional values: [false] [true] <br> Desc: whether to full screen",
        show: "Optional.<br> Default value: [true].<br> Optional values: [false] [true] <br> Desc: whether to display or not",
    }
};

PARAM_DESC["cnstream::ModuleIPC"] = {
    show_perf_info: "Optional.<br> Default value: [false].<br> Optional values: [false] [true] <br> Desc: Whether show perf information of this module.",
    custom_params: {
        ipc_type: "Optional. <br> Default value: [client] <br> Optional values: [clinet][server] <br> Desc: An enumerated type that identifies the ipc type of the module ipc",
        memmap_type: "Optional. <br> Default value: [mlu]<br> Optional values: [cpu][mlu]<br> Desc: device memory map",
        socket_address: "Optional. <br> Default value: [test_ipc] <br> Optional values: string <br> Desc: communication socket adress",
        max_cachedframe_size: "Optional. <br> Default value: [80] <br> Optional values: integer <br> Desc: max size for cached processed frame map",
        device_id: "Optional. <br> Default value: [0]<br> Optional values: integer <br> Desc: device id of mlu",
    }
};
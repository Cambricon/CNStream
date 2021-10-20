window.PARAM_DESC = {}
PARAM_DESC["cnstream::DataSource"] = {
    custom_params: {
        output_type: "Optional. <br> Default value: [cpu] <br> Optional values: [cpu] [mlu] <br> Desc: Where the outputs will be stored.",
        decoder_type: "Optional. <br> Default value: [cpu] <br> Optional values: [cpu] [mlu] <br> Desc: The decoder type.",
        reuse_cndec_buf: "Optional. <br> Default value: [false] <br> Optional values: [false] [true] <br> Desc: Whether to enable the mechanism to reuse MLU codec's buffers by next modules. <br> Note: This parameter is used when decoder type is [mlu]",
        input_buf_number: "Optional. <br> Default value: [2] <br> Optional values: integer <br> Desc: Input buffer's number used by MLU codec.",
        output_buf_number: "Optional. <br> Default value: [3] <br> Optional values: integer <br> Desc: Output buffer's number used by MLU codec.",
        interval: "Optional. <br> Default value: [1] <br> Optional values: integer <br> Desc: The interval of outputting one frame. It outputs one frame every n (interval_) frames.",
        apply_stride_align_for_scaler: "Optional. <br> Default value: [false] <br> Optional values: [false] [true] <br> Desc: Whether to set outputs meet the Scaler alignment requirement.",
        device_id: "Required when MLU is used. <br> Optional values: integer <br> Desc: The device ordinal. <br> Note: Set the value to -1 for CPU. Set the value for MLU in the range 0 - N.",
    },
};
PARAM_DESC["cnstream::Inferencer"] = {
    custom_params: {
        model_path: "Required. <br> Optional values: model path <br> Desc: The path of the offline model.",
        func_name: "Required. <br> Default value: [subnet0] <br> Desc: The function name that is defined in the offline model. <br> note: It could be found in Cambricon twins file. For most cases, it is 'subnet0' ",
        use_scaler: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Whether use the scaler to preprocess the input.",
        preproc_name: "Optional. <br> Desc: The class name for custom preprocessing. Use mlu preproc by default.",
        postproc_name: "Required. <br> Desc: The class name for postprocess.",
        batching_timeout: "Optional. <br> Default value: [3000] <br> Optional values: integer <br> Desc: The batching timeout. unit[ms].",
        threshold: "Optional. <br> Default value: [0] <br> Optional values: float <br> Desc: The threshold pass to postprocessing function.",
        data_order: "Optional. <br> Default value: [NHWC] <br> Optional value: [NHWC] [NCHW] <br> Desc: The order in which the output data of the model are placed.",
        infer_interval: "Optional. <br> Optional value: integer <br> Desc: Inferencing one frame every ``infer_interval`` frames.",
        object_infer: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: If object_infer is set to true, the detection target is used as the input to inferencing. If it is set to false, the video frame is used as the input to inferencing.",
        obj_filter_name: "Optional. <br> Desc: The class name for object filter. See cnstream::ObjFilter.",
        keep_aspect_ratio: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Keep aspect ratio, when the mlu is used for image processing.",
        dump_resized_image_dir: "Optional. <br> Desc: Where to dump the resized image.",
        model_input_pixel_format: "Optional. <br> Default value: [RGBA32] <br> Optional value: [ARGB32][ABGR32][RGBA32][BGRA32] <br> Desc: The pixel format of the model input image.",
        mem_on_mlu_for_postproc: "Optional. <br> Default value: [false] <br> Optional value: [true][false] <br> Desc: Pass a batch mlu pointer directly to post-processing function without making d2h copies.",
        saving_infer_input: "Optional. <br> Default value: [false] <br> Optional value: [true][false] <br> Desc: Save the inputs of the inference.",
        pad_method: "Optional. <br> Default value: [center] <br> Optional values: [center][origin] <br> Desc: origin means image at top left corner.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device ordinal.",
    },
};

PARAM_DESC["cnstream::Inferencer2"] = {
    custom_params: {
        model_path: "Required when backend is cnrt. <br> Optional values: model path <br> Desc: The path of the offline model.",
        func_name: "Required when backend is cnrt. <br> Default value: [subnet0] <br> Desc: The function name that is defined in the offline model. <br> note: It could be found in Cambricon twins file. For most cases, it is 'subnet0' ",
        model_data: "Required when backend is magicmind. <br> Optional values: model data path <br> Desc: The path of magicmind model data.",
        model_graph: "Required when backend is magicmind. <br> Optional values: model graph path <br> Desc: The path of magicmind model graph.",
        preproc_name: "Optional. <br> Default value: [cncv] <br> Optional values: [rcop][scaler][cncv] or the class name for preprocess <br> Desc: The preprocess method. If use custom preprocess, set the class name of it. The class specified by this name must inherit from class cnstream::VideoPreproc.",
        postproc_name: "Required. <br> Desc: The class name for postprocess. The class specified by this name must inherit from class cnstream::VideoPostproc.",
        engine_num: "Optional. <br> Default value: [1] <br> Optional values: integer <br> Desc: Infer server engine number. Increase the engine number to improve performance.",
        batching_timeout: "Optional. <br> Default value: [1000] <br> Optional values: integer <br> Desc: The batching timeout. unit[ms].",
        batch_strategy: "Optional. <br> Default value: [dynamic] <br> Optional values: [dynamic][static] <br> Desc: The batch strategy. The dynamic strategy: high throughput but high latency. The static strategy: low latency but low throughput.",
        priority: "Optional. <br> Default value: [0] <br> Optional values: integer[0-9] <br> Desc: The priority of this infer task in infer server. The lager the number is, the higher the priority is.",
        threshold: "Optional. <br> Default value: [0] <br> Optional values: float <br> Desc: The threshold pass to postprocessing function.",
        data_order: "Optional. <br> Default value: [NHWC] <br> Optional value: [NHWC] [NCHW] <br> Desc: The order in which the output data of the model are placed.",
        show_stats: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Whether show performance statistics.",
        object_infer: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: If object_infer is set to true, the detection target is used as the input to inferencing. If it is set to false, the video frame is used as the input to inferencing.",
        obj_filter_name: "Optional. <br> Desc: The class name for object filter. See cnstream::ObjFilter.",
        keep_aspect_ratio: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Keep aspect ratio, when the mlu is used for image processing.",
        mean: "Optional. <br> Default value: {} <br> Optional value: float vector(size 3-4) e.g. \"104, 117, 123\" <br> Desc: Only when mean_std preprocess is used, it is valid. For getting the mean value of each pixel in input image.",
        std: "Optional. <br> Default value: {} <br> Optional value: float vector(size 3-4) e.g. \"1, 1, 1\" <br> Desc: Only when mean_std preprocess is used, it is valid. For getting the std value of each pixel in input image.",
        normalize: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Only when mean_std preprocess is used, it is valid. For multiplying 255 in std and mean array.",
        model_input_pixel_format: "Optional. <br> Default value: [RGBA32] <br> Optional value: [RGB24][BGR24][ARGB32][ABGR32][RGBA32][BGRA32] <br> Desc: The pixel format of the model input image. For using RCOP preproc ARGB32/ABGR32/RGBA32/BGRA32 are supported. For using Custom preproc RGB24/BGR24/ARGB32/ABGR32/RGBA32/BGRA32 are supported. This parameter does not take effect when SCALER preproc is used.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device ordinal.",
    },
};

PARAM_DESC["cnstream::Osd"] = {
    custom_params: {
        label_path: "Optional. <br> Desc: The path of the label file.",
        font_path: "Optional. <br> Desc: The path of font.",
        label_size: "Optional. <br> Default value: [normal] <br> Optional value [normal] [large] [larger] [small] [smaller] float <br> Desc: The size of the label.",
        text_scale: "Optional. <br> Default value: [1] <br> Optional value float <br> Desc: The scale of the text, which can change the size of text put on image. scale = label_size * text_scale",
        text_thickness: "Optional. <br> Default value: [1] <br> Optional value float <br> Desc: The thickness of the text, which can change the thickness of text put on image. thickness = label_size * text_thickness",
        box_thickness: "Optional. <br> Default value: [1] <br> Optional value float <br> Desc: The thickness of the box drawn on the image. thickness = label_size * box_thickness",
        secondary_label_path: "Optional. <br> Desc: The path of the secondary label file.",
        attr_keys: "Optional. <br> Optional value string, e.g., [attr_key1, attr_key2] <br> Desc: The keys of attribute which you want to draw on image.",
        logo: "Optional. Desc: Draw 'logo' on each frame.",
    }
};

PARAM_DESC["cnstream::Tracker"] = {
    custom_params: {
        model_path: "Optional. <br> Desc: path of offline model",
        func_name: "Optional. <br> Default value: [subnet0] <br> Desc: function name defined in the offline model. It can be found in the Cambricon twins description file",
        track_name: "Optional. <br> Default value: [FeatureMatch] <br> Optional values: [FeatureMatch] [KCF] <br> Desc: Track algorithm name.",
        max_cosine_distance: "Optional. <br> Default value: [0.2] <br> Optional values: float <br> Desc: Threshold of cosine distance.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device id.",
    }
};

PARAM_DESC["cnstream::RtspSink"] = {
    custom_params: {
        http_port: "Optional. <br> Default value: [8080] <br> Desc: Http port.",
        udp_port: "Optional. <br> Default value: [9554] <br> Desc: UDP port.",
        frame_rate: "Optional. <br> Default value: [25] <br> Desc: Frame rate of the encoded video.",
        gop_size: "Optional. <br> Default value: [30] <br> Desc: Group of pictures is known as GOP.",
        kbit_rate: "Optional. <br> Default value: [2048] <br> Desc: The amount data encoded for a unit of time.",
        preproc_type: "Optional. <br> Default value: [cpu] <br> Optional values: [cpu] <br> Desc: Resize and colorspace convert type.",
        encoder_type: "Optional. <br> Default value: [mlu] <br> Optional values: [cpu] [mlu] <br> Desc: Encode type.",
        color_mode: "Optional. <br> Default value: [nv] <br> Optional values: [nv] [bgr] <br> Desc: Input picture color mode.",
        view_mode: "Optional. <br> Default value: [single] <br> Optional values: [single] [mosaic] <br> Desc: View mode",
        view_rows: "Optional. <br> Default value: [4] <br> Desc: Divide the screen horizontally.",
        view_cols: "Optional. <br> Default value: [4] <br> Desc: Divide the screen vertically.",
        dst_width: "Optional. <br> Default value: source width <br> Desc: The image width of the output.",
        dst_height: "Optional. <br> Default value: source height <br> Desc: The image height of the output.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device id.",
    }
};

PARAM_DESC["cnstream::Encode"] = {
    custom_params: {
        encoder_type: "Optional. <br> Default value: [cpu] <br> Optional values: [cpu] [mlu] <br> Desc: Use cpu encoding or mlu encoding.",
        preproc_type: "Optional. <br> Default value: [cpu] <br> Optional values: [cpu] [mlu] <br> Desc: Preprocessing data on cpu or mlu(mlu is not supported yet).",
        codec_type: "Optional. <br> Default value: [h264] <br> Optional values: [h264] [hevc] [jpeg] <br> Desc: Encoder type.",
        use_ffmpeg: "Optional. <br> Default value: [false] <br> Optional values: [true] [false] <br> Desc: Do resize and color space convert using ffmpeg.",
        dst_width: "Optional. <br> Default value: source width <br> Optional values: integer <br> Desc: The width of the output.",
        dst_height: "Optional. <br> Default value: source height <br> Optional values: integer <br> Desc: The height of the output.",
        frame_rate: "Optional. <br> Default value: [25] <br> Optional values: integer <br> Desc: Frame rate of the encoded video.",
        kbit_rate: "Optional. <br> Default value: [1024] <br> Optional values: integer <br> Desc: The amount data encoded for a unit of time. Only valid when encode on mlu.",
        gop_size: "Optional. <br> Default value: [30] <br> Optional values: integer <br> Desc: Group of pictures is known as GOP. Only valid when encode on mlu.",
        output_dir: "Optional. <br> Default value: [{CURRENT_DIR}/output] <br> Optional values: string <br> Desc: Where to store the encoded video.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device id.",
    }
};

PARAM_DESC["cnstream::Displayer"] = {
    custom_params: {
        "window-width": "Required. <br> Optional values: integer <br> Desc: display window width",
        "window-height": "Required. <br> Optional values: integer <br> Desc:  display window height",
        "refresh-rate": "Required. <br> Optional values: integer <br> Desc: display refresh rate",
        "max-channels": "Required. <br> Optional values: integer <br> Desc: max channels to display",
        "full-screen": "Optional. <br> Default value: [false] <br> Optional values: [false] [true] <br> Desc: whether to full screen",
        show: "Required. <br> Optional values: [false] [true] <br> Desc: whether to display or not",
    }
};

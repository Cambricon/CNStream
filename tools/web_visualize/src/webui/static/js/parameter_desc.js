window.PARAM_DESC = {}
PARAM_DESC["cnstream::DataSource"] = {
    custom_params: {
        interval: "Optional. <br> Default value: [1] <br> Optional values: integer <br> Desc: The interval of outputting one frame. It outputs one frame every n (interval_) frames.",
        bufpool_size: "Optional. <br> Default value: [16] <br> Optional values: integer <br> Desc: The bufpool size for the stream.",
        device_id: "Required when MLU is used. <br> Optional values: integer <br> Desc: The device ordinal. <br> Note: Set the value to -1 for CPU. Set the value for MLU in the range 0 - N.",
    },
};

PARAM_DESC["cnstream::Inferencer"] = {
    custom_params: {
        model_path: "Required. <br> Desc: The path of the offline model.",
        preproc: "Required. <br> Desc: The class name for preprocess. Parameters related to postprocessing including name and use_cpu. The class specified by this name must inherit from class cnstream::Preproc",
        postproc: "Required. <br> Desc: The class name for postprocess. Parameters related to postprocessing including name and threshold. The class specified by this name must inherit from class cnstream::Postproc.",
        engine_num: "Optional. <br> Default value: [1] <br> Optional values: integer <br> Desc: Infer server engine number. Increase the engine number to improve performance.",
        batch_timeout: "Optional. <br> Default value: [300] <br> Optional values: integer <br> Desc: The batching timeout. unit[ms].",
        batch_strategy: "Optional. <br> Default value: [dynamic] <br> Optional values: [dynamic][static] <br> Desc: The batch strategy. The dynamic strategy: high throughput but high latency. The static strategy: low latency but low throughput.",
        model_input_pixel_format: "Optional. <br> Default value: [RGBA32] <br> Optional value: For using custom preproc RGB24/BGR24/GRAY/TENSOR are supported. <br> Desc: The pixel format of the model input image.",
        filter: "Optional. <br> Default value: [] <br> Desc: Parameters related to filter including name and categories. The class name for custom object filter must inherit from class cnstream::ObjectFilterVideo.. ",
        interval: "Optional. <br> Optional value: integer <br> Desc: Inferencing one frame every ``interval`` frames.",
        priority: "Optional. <br> Default value: [0] <br> Optional values: integer[0-9] <br> Desc: The priority of this infer task in infer server. The lager the number is, the higher the priority is.",
        show_stats: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Whether show performance statistics.",
        label_path: "Optional. <br> Desc: The path of the label.",
        custom_preproc_params: "Optional. <br> Default value: empty string <br> Optional value: json string <br> Desc: Custom preprocessing parameters.",
        custom_postproc_params: "Optional. <br> Default value: empty string <br> Optional value: json string <br> Desc: Custom postprocessing parameters.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device ordinal.",
    },
};

PARAM_DESC["cnstream::Osd"] = {
    custom_params: {
        label_path: "Optional. <br> Desc: The path of the label file.",
        osd_handler: "Optional. <br> Desc: The name of osd handler.",
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
        track_name: "Optional. <br> Default value: [FeatureMatch] <br> Optional values: [FeatureMatch] [IoUMatch] <br> Desc: Track algorithm name.",
        max_cosine_distance: "Optional. <br> Default value: [0.2] <br> Optional values: float <br> Desc: Threshold of cosine distance.",
        engine_num: "Optional. <br> Default value: [1] <br> Optional values: integer <br> Desc: Infer server engine number. Increase the engine number to improve performance.",
        batch_timeout: "Optional. <br> Default value: [300] <br> Optional values: integer <br> Desc: The batching timeout. unit[ms].",
        model_input_pixel_format: "Optional. <br> Default value: [RGBA32] <br> Optional value: RGB24/BGR24/TENSOR are supported. <br> Desc: The pixel format of the model input image.",
        priority: "Optional. <br> Default value: [0] <br> Optional values: integer[0-9] <br> Desc: The priority of this infer task in infer server. The lager the number is, the higher the priority is.",
        show_stats: "Optional. <br> Default value: [false] <br> Optional value: [true] [1] [TRUE] [True] [false] [0] [FALSE] [False] <br> Desc: Whether show performance statistics.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device id.",
    }
};

PARAM_DESC["cnstream::VEncode"] = {
    custom_params: {
        file_name: "Optional. <br> Default value: [output/output.mp4] <br> Optional values: string <br> Desc: File name and path to store, the final name will be added with stream id or frame count.",
        rtsp_port: "Optional. <br> Default value: [-1] <br> Optional values: integer <br> Desc: RTSP port. If this value is greater than 0, stream will be delivered by RTSP protocol.",
        frame_rate: "Optional. <br> Default value: [30] <br> Optional values: integer <br> Desc: Frame rate of the encoded video.",
        hw_accel: "Optional. <br> Default value: [true] <br> Optional values: [true] [false] <br> Desc: Whether use hardware to encode.",
        dst_width: "Optional. <br> Default value: source width <br> Optional values: integer <br> Desc: The width of the output.",
        dst_height: "Optional. <br> Default value: source height <br> Optional values: integer <br> Desc: The height of the output.",
        resample: "Optional. <br> Default value: [false] <br> Optional values: [true] [false] <br> Desc: Resample. If set true, some frame will be dropped.",
        view_rows: "Optional. <br> Default value: [4] <br> Desc: Divide the screen horizontally.",
        view_cols: "Optional. <br> Default value: [4] <br> Desc: Divide the screen vertically.",
        bit_rate: "Optional. <br> Default value: [4000000] <br> Desc: The amount data encoded for a unit of time.",
        gop_size: "Optional. <br> Default value: [10] <br> Desc: Group of pictures is known as GOP.",
        device_id: "Optional. <br> Default value: [0] <br> Optional values: integer <br> Desc: The device id.",
    }
};

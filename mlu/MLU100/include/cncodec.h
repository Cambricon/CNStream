/*************************************************************************
 * Copyright (C) [2018] by Cambricon, Inc.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#ifndef INCLUDE_CNCODEC_H_
#define INCLUDE_CNCODEC_H_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef unsigned char                       CN_U8;
typedef uint16_t                            CN_U16;
typedef unsigned int                        CN_U32;
typedef float                               CN_FLOAT;
typedef double                              CN_DOUBLE;
typedef signed char                         CN_S8;
typedef int16_t                             CN_S16;
typedef int                                 CN_S32;

#ifndef _M_IX86
    typedef uint64_t                        CN_U64;
    typedef int64_t                         CN_S64;
#else
    typedef __int64                         CN_U64;
    typedef __int64                         CN_S64;
#endif

typedef char                                CN_CHAR;
#define CN_VOID                             void

typedef enum {
    CN_FALSE                                = 0,
    CN_TRUE                                 = 1,
} CN_BOOL;

#define CN_INVALID_HANDLE                  0

typedef enum {
    CN_SUCCESS                              = 0,        /*成功*/
    CN_ERROR_INVALID_VALUE                  = 1,        /*参数非法*/
    CN_ERROR_OUT_OF_MEMORY                  = 2,        /*内存不足*/
    CN_ERROR_NOT_INITIALIZED                = 3,        /*非初始化*/
    CN_ERROR_DEINITIALIZED                  = 4,        /*已经销毁*/
    CN_ERROR_PROFILER_DISABLED              = 5,        /*Profiler被禁用*/
    CN_ERROR_PROFILER_NOT_INITIALIZED       = 6,        /*Profiler被禁用*/
    CN_ERROR_ALREADY_STARTED                = 7,        /*已经开启*/
    CN_ERROR_ALREADY_STOPPED                = 8,        /*已经停止*/
    CN_ERROR_OS_CALL                        = 9,        /*系统调用失败*/
    CN_ERROR_INVALID_FORMAT                 = 10,       /*不支持的编码格式*/
    CN_ERROR_NO_RESOURCE                    = 11,       /*资源不足*/
    CN_ERROR_INCOMPATIBLE_DRIVER_VERSION    = 12,       /*不兼容的驱动版本*/
    CN_ERROR_MULTIPLE_RELEASE               = 13,       /*多次释放*/
    CN_ERROR_NO_DEVICE                      = 100,      /*设备不存在*/
    CN_ERROR_INVALID_DEVICE                 = 101,      /*非法设备*/
    CN_ERROR_DEVICE_EXCEPTION               = 102,      /*设备异常*/
    CN_ERROR_INVALID_IMAGE                  = 200,      /*非法图片*/
    CN_ERROR_INVALID_CONTEXT                = 201,      /*非法上下文*/
    CN_ERROR_INVALID_DATA                   = 202,      /*非法数据*/
    CN_ERROR_INVALID_SOURCE                 = 300,      /*输入源非法*/
    CN_ERROR_FILE_NOT_FOUND                 = 301,      /*文件不存在*/
    CN_ERROR_INVALID_HANDLE                 = 400,      /*非法句柄*/
    CN_ERROR_NOT_FOUND                      = 500,      /*没有找到*/
    CN_ERROR_NOT_READY                      = 600,      /*没有就位*/
    CN_ERROR_LAUNCH_FAILED                  = 700,      /*开启失败*/
    CN_ERROR_LAUNCH_OUT_OF_RESOURCES        = 701,      /*开启过程中，内存分配失败*/
    CN_ERROR_LAUNCH_TIMEOUT                 = 702,      /*开启超时失败*/
    CN_ERROR_UNKNOWN                        = 999,      /*未知错误*/
    CN_ERROR_SYSCALL                        = 1000,     /*系统调用失败*/
} CN_ERROR;

typedef CN_S32 CNResult;

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    #define CNAPI __stdcall
#else
    #define CNAPI __attribute ((visibility("default")))
#endif

#define MAX_INPUT_DATA_SIZE           (25<<20)

#define MAX_JPEG_INPUT_WIDTH 4096
#define MAX_JPEG_INPUT_HEIGHT 2160

#define MAX_VIDEO_INPUT_WIDTH 4096
#define MAX_VIDEO_INPUT_HEIGHT 2160

#define MAX_OUTPUT_WIDTH 4096
#define MAX_OUTPUT_HEIGHT 2160

#define H264_ENCODE_MIN_ALIGN 2
#define JPEG_ENCODE_MIN_ALIGN 4

#define MLU_P2P_ONE_CHN_BUFFER_NUM 2

typedef         CN_U64      CN_HANDLE_VDEC;                            /**<  解码器句柄类型  */
typedef         CN_U64      CN_HANDLE_VENC;                            /**<  编码器句柄类型  */

/* Video Codec Types*/
typedef enum {
    CN_VIDEO_CODEC_MPEG4 = 0,                                          /**<  MPEG4   */
    CN_VIDEO_CODEC_H264,                                               /**<  H264   */
    CN_VIDEO_CODEC_HEVC,                                               /**<  HEVC   */
    CN_VIDEO_CODEC_JPEG,                                               /**<  JPEG   */
    CN_VIDEO_CODEC_MJPEG
} CN_VIDEO_CODEC_TYPE_E;

typedef enum {
    CN_VIDEO_MODE_FRAME = 0,                                           /**<  按帧方式发送码流   */
    CN_VIDEO_MODE_STREAM,                                              /**<  按流方式发送码流   */
} CN_VIDEO_MODE_E;

/*像素格式*/
typedef enum {
    CN_PIXEL_FORMAT_YUV420SP = 0,                                      /**< YUV420 SemiPlanar */
    CN_PIXEL_FORMAT_RGB24     ,                                        /**< RGB24  */
    CN_PIXEL_FORMAT_BGR24
} CN_PIXEL_FORMAT_E;

/*De-interlace 模式*/
typedef enum {
    CN_VIDEO_DIE_MODE_NODIE = 0,                                       /**< 强制不做De-interlace */
    CN_VIDEO_DIE_MODE_AUTO,                                            /**< 自适应是否做Deinterlace */
    CN_VIDEO_DIE_MODE_DIE                                              /**< 强制做De-interlace  */
}CN_VIDEO_DIE_MODE_E;

/*解码模式*/
typedef enum {
    CN_VIDEO_CREATE_AUTO = 0x00,                                       /**< 自适应选择解码模式 */
    CN_VIDEO_CREATE_HARD,                                              /**< 强行使用硬件解码 */
    CN_VIDEO_CREATE_SOFT,                                              /**< 强行使用软件解码 */
} CN_VIDEO_CREATE_MODE_E;

/*图片信息*/
#define CN_MAX_PIC_CHANNELS  (4)
typedef struct {
    CN_PIXEL_FORMAT_E                enPixelFormat;                     /**< 像素格式 */
    CN_U32                           u32FrameSize;                      /**< 帧大小 */
    CN_U32                           u32Height;                         /**< 图片高度 */
    CN_U32                           u32Width;                          /**< 图片宽度 */
    CN_U64                           u64Pts;                            /**< PTS */
    CN_U32                           u32Stride[CN_MAX_PIC_CHANNELS];    /**< 通道Stride */
    CN_U64                           u64PhyAddr;                        /**< 图片物理地址 */
    CN_U64                           u64VirAddr;                        /**< 图片虚拟地址 */
    CN_U64                           u64FrameIndex;                     /**< 解码帧号 */
    CN_U64                           u64VideoIndex;                     /**< 显示帧号: 暂不支持 */
    CN_U64                           u32BufIndex;                       /**< 当前帧所在的MLU缓存index */
    CN_U64                           u64TransferUs;                     /**< 当前帧解码图像从codec到MLU的传输延时(微秒) */
    CN_U64                           u64DecodeDelayUs;                  /**< 当前帧codec内部解码延时(微秒) */
    CN_U64                           u64SendCallbackDelayUs;            /**< 当前帧从CN_MPI_VDEC_Send发送到回调的延时(微秒) */
    CN_U64                           u64InputUs;                        /**< 当前帧压缩数据从host传输到codec的延时(微秒) */
} CN_VIDEO_IMAGE_INFO_S;

/*图片回调函数*/
typedef CN_VOID (*CN_VDEC_IMAGE_CALLBACK)(CN_VIDEO_IMAGE_INFO_S *pImageOutput, CN_U64 u64UserData);

/*位置坐标*/
typedef struct {
    CN_U32                          u32X;                              /**< 绝对坐标是 2 像素对齐*/
    CN_U32                          u32Y;                              /**< 绝对坐标是 2 像素对齐 */
    CN_U32                          u32Width;                          /**< 绝对坐标是 4 像素对齐 */
    CN_U32                          u32Height;                         /**< 绝对坐标是 4 像素对齐 */
}CN_RECT_S;

/*坐标模式*/
/*相对坐标模式中，CN_RECT_S的成员的取值范围是[0,1000]，每个单位表示图像宽度或高度的1/1000*/
typedef enum {
    CN_RECT_RATIO_COOR = 0,                                            /**< 相对坐标 */
    CN_RECT_ABS_COOR                                                   /**< 绝对坐标 */
}CN_RECT_COORDINATE_E;

/*图片裁剪*/
typedef struct {
    CN_BOOL                         bEnable;                           /**< 是否使能图片裁剪 */
    CN_RECT_COORDINATE_E            enCropCoordinate;                  /**< 裁剪区域的坐标模式 */
    CN_RECT_S                       stCropRect;                        /**< 裁剪区域的坐标 */
}CN_VIDEO_CROP_ATTR_S;

/*帧率控制*/
/*要得到准确的实际输出帧率，CN_MPI_VDEC_Send发送的帧率需要与输入帧率保持一致*/
typedef struct {
    CN_BOOL                         bEnable;                           /**< 是否使能帧率控制 */
    CN_S32                          s32SrcFrmRate;                     /**< 输入帧率 */
    CN_S32                          s32DstFrmRate;                     /**< 输出帧率 */
} CN_VIDEO_FRAME_RATE_S;

/*图片后处理属性*/
typedef struct {
    CN_VIDEO_FRAME_RATE_S           stFrameRate;                       /**< 帧率控制属性 */
    CN_VIDEO_CROP_ATTR_S            stCropAttr;                        /**< 图片裁剪属性 */
    CN_VIDEO_DIE_MODE_E             enDieMode;                         /**< De-interlace 去隔行模式 */

    CN_BOOL                         bIeEn;                             /**< 保留，必须设置为0 */
    CN_BOOL                         bDciEn;                            /**< 是否使能动态对比度调节 */
    CN_BOOL                         bNrEn;                             /**< 是否使能降噪 */
    CN_BOOL                         bHistEn;                           /**< 保留，必须设置为0 */
    CN_BOOL                         bEsEn;                             /**< 保留，必须设置为0 */
    CN_BOOL                         bSpEn;                             /**< 是否使能图像锐化 */

    CN_U32                          u32Contrast;                       /**< 动态对比度调节强度0-64，默认值32 */
    CN_U32                          u32DieStrength;                    /**< 保留，必须设置为0 */
    CN_U32                          u32IeStrength;                     /**< 保留，必须设置为0 */
    CN_U32                          u32SfStrength;                     /**< 空域去噪强度0-2047，默认值128 */
    CN_U32                          u32TfStrength;                     /**< 保留，必须设置为0 */
    CN_U32                          u32CfStrength;                     /**< 色域去噪强度0-255，默认值8 */
    CN_U32                          u32CTfStrength;                    /**< 保留，必须设置为0 */
    CN_U32                          u32CvbsStrength;                   /**< 保留，必须设置为0 */
    CN_U32                          u32DeMotionBlurring;               /**< 保留，必须设置为0 */
    CN_U32                          u32SpStrength;                     /**< 图像锐化强度0-100，默认值32 */
}CN_VIDEO_PP_ATTR_S;

typedef struct {
    CN_U64                          addr;                              /**< P2P模式MLU缓存地址 */
    CN_U64                          len;                               /**< P2P模式MLU缓存长度 */
}CN_MLU_P2P_BUFFER_S;

typedef enum {
    CN_MLU_BUFFER = 0,                                                 /**< MLU buffer */
    CN_CPU_BUFFER                                                      /**< CPU buffer */
}CN_BUFFER_TYPE_E;

typedef struct {
    CN_U32                          buffer_num;
    CN_BUFFER_TYPE_E                buffer_type;
    CN_MLU_P2P_BUFFER_S             *p_buffers;
}CN_MLU_P2P_ATTR_S;


/*解码器创建属性*/
typedef struct {
    CN_U32                         u32VdecDeviceID;                   /**< 板卡设备号 0~(device_num-1)*/
    CN_VIDEO_CODEC_TYPE_E          enInputVideoCodec;                 /**< 视频编码格式：支持MPEG-4、H.264、HEVC（H.265）、JPEG*/
    CN_VIDEO_MODE_E                enVideoMode;                       /**< 码流发送方式*/
    CN_U32                         u32MaxWidth;                       /**< 最大支持的分辨率(宽度) */
    CN_U32                         u32MaxHeight;                      /**< 最大支持的分辨率(高度) */
    CN_U32                         u32TargetWidth;                    /**< 输出分辨率(宽度) 2 像素对齐，0=原始分辨率输出 */
    CN_U32                         u32TargetHeight;                   /**< 输出分辨率(高度) 2 像素对齐，0=原始分辨率输出 */
    CN_U32                         u32TargetWidthSubstream;           /**< 子码流输出分辨率(宽度) 2 像素对齐，0=关闭子码流输出 */
    CN_U32                         u32TargetHeightSubstream;          /**< 子码流输出分辨率(高度) 2 像素对齐，0=关闭子码流输出 */
    CN_U32                         u32MaxFrameSize;                   /**< 最大ES帧大小：暂不支持 */
    CN_U32                         u32EsBufCount;                     /**< ES流缓冲区个数：暂不支持 */

    CN_U32                         u32ImageBufCount;                  /**< 图片缓冲区个数：暂不支持 */

    CN_PIXEL_FORMAT_E              enOutputPixelFormat;               /**< 输出像素格式：支持YUV420SP和RGB24 */

    CN_VIDEO_CREATE_MODE_E         enVideoCreateMode;                 /**< 解码模式：暂不支持 */

    CN_VIDEO_PP_ATTR_S             stPostProcessAttr;                 /**< 图片后处理属性 */

    CN_U64                         u64UserData;                       /**< 用户上下文 */
    CN_VDEC_IMAGE_CALLBACK         pImageCallBack;                    /**< 解码图片回调函数指针 */

    CN_MLU_P2P_ATTR_S              mluP2pAttr;                        /**< P2P模式MLU缓存配置信息 */
    CN_U64                         Reserved2[5];                      /**< 预留字段 */
} CN_VIDEO_CREATE_ATTR_S;

/* Picture Parameters for Decoding */
typedef struct {
    CN_U32                          nBitstreamDataLen;                /**< Number of bytes in bitstream data buffer */
    CN_U64                          pBitstreamData;                   /**< Ptr to bitstream data for this picture (slice-layer) */
    CN_U64                          u64FrameIndex;                    /**< 帧号 */
    CN_U64                          u64Pts;
    CN_U32                          u32Width;
    CN_U32                          u32Height;
}CN_VIDEO_PIC_PARAM_S;

typedef struct {
    CN_U32                          u32DeviceID;
    CN_U32                          u32MluIndex;                       /**< mlu index，用于申请mlu内存时向cnrt接口提供device index */
    CN_U32                          u32FreeChannels;                   /**< 空闲解码通道数 */
    CN_U32                          u32UsedChannels;                   /**< 占用解码通道数 */
}CN_VDEC_DEVICE_CAPABILITY_S;

#define MAX_VDEC_DEVICE_NUM         (16)
/*解码器能力集*/
typedef struct {
    CN_U32                          u32VdecDeviceNum;
    CN_VDEC_DEVICE_CAPABILITY_S     VdecDeviceList[MAX_VDEC_DEVICE_NUM];
}CN_VDEC_CAPABILITY_S;

////////////////////////////////////////////////////////////////////////////////
// 函数名：CN_MPI_CN_MPI_SoftwareVersion
// 描述：获取SDK lib版本号
// 参数：
//  无
// 返回值： 版本号字符串，x.x.x
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNAPI const char * CN_MPI_SoftwareVersion();

////////////////////////////////////////////////////////////////////////////////
// 函数名：CN_MPI_Init
// 描述：初始化SDK
// 参数：
//  [in]u32Flags - 暂未使用
// 返回值： CNResult
//  错误代码。
// 说明：
//  SDK使用之前，必须调用该借口
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_Init();

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_Exit
// 描述：析构SDK
// 参数：
//  [in]u32Flags - 暂未使用
// 返回值： CNResult
//  错误代码。
// 说明：
//  释放资源
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_Exit();

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VDEC_GetCapability
// 描述：解码器 送流
// 参数：
//  [in]hDecoder - 解码器句柄
//  [in]pstPicParams - Parser返回的帧数据
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VDEC_GetCapability(CN_VDEC_CAPABILITY_S *pstCapability);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VDEC_Create
// 描述：创建解码器
// 参数：
//  [out]phDecoder - 返回解码器句柄
//  [in]pstCreateAttr -  解码器属性
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VDEC_Create(CN_HANDLE_VDEC *phDecoder, CN_VIDEO_CREATE_ATTR_S *pstCreateAttr);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VDEC_Destroy
// 描述：销毁解码器
// 参数：
//  [in]hDecoder - 解码器句柄
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VDEC_Destroy(CN_HANDLE_VDEC hDecoder);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VDEC_Send
// 描述：解码器 送流
// 参数：
//  [in]hDecoder - 解码器句柄
//  [in]pstPicParams - Parser返回的帧数据
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VDEC_Send(CN_HANDLE_VDEC hDecoder, CN_VIDEO_PIC_PARAM_S *pstPicParams);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VDEC_SendEx
// 描述：解码器 送流
// 参数：
//  [in]hDecoder - 解码器句柄
//  [in]pstPicParams - Parser返回的帧数据
//  [in]u32MilliSec - 送流方式
//                  -1：阻塞
//                   0：非阻塞
//                 正值：超时时间，没有上限值，以ms为单位
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VDEC_SendEx(CN_HANDLE_VDEC hDecoder, CN_VIDEO_PIC_PARAM_S *pstPicParams, CN_S32 s32MilliSec);

// CN_MPI_MLU_P2P_ReleaseBuffer
// 描述：P2P模式释放MLU缓存
// 参数：
//  [in]hDecoder - 解码器句柄
//  [in]buffer_index - 缓存index
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_MLU_P2P_ReleaseBuffer(CN_HANDLE_VDEC hDecoder, int buffer_index);


typedef enum {
    CN_LOG_NONE,
    CN_LOG_ERR,
    CN_LOG_WARN,
    CN_LOG_INFO,
    CN_LOG_DEBUG
}CN_LOG_LEVEL;

/*日志回调函数*/
typedef CN_VOID (*CN_LOG_CALLBACK)(CN_LOG_LEVEL level, const char *msg);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_SetLogCallback
// 描述：设置全局日志回调函数。如果不设置，或设置为NULL，则将日志输出到stderr
// 参数：
//  [in]callback - 日志回调函数
////////////////////////////////////////////////////////////////////////////////
CN_VOID CNAPI CN_MPI_SetLogCallback(CN_LOG_CALLBACK callback);


/*致命异常回调函数*/
typedef CN_VOID (*CN_FATAL_CALLBACK)(CN_U32 err, CN_U64 u64UserData);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_SetFatalCallback
// 描述：设置全局Fatal回调函数。如果不设置，或设置为NULL，则将Fatal输出到stderr
// 参数：
//  [in]callback - 日志回调函数
////////////////////////////////////////////////////////////////////////////////
CN_VOID CNAPI CN_MPI_SetFatalCallback(CN_FATAL_CALLBACK callback, CN_U64 u64UserData);

/************************ENCODE************************************/
typedef struct{
    uint32_t                        frame_size;             /**< 帧大小*/
    uint32_t                        data_type;
    uint64_t                        phy_addr;
    uint64_t                        vir_addr;
    uint64_t                        pts;
    uint64_t                        buf_index;
    uint64_t                        input_transfer_delay;   // host -> codec
    uint64_t                        encode_delay;           // codec
    uint64_t                        output_transfer_delay;  // codec -> mlu
    uint64_t                        send_callback_delay;    // host send -> host callback
}CN_VENC_FRAME_DATA_S;

/*编码回调函数*/
typedef CN_VOID (*CN_VENC_CALLBACK)(CN_VENC_FRAME_DATA_S *pFrameOutput, void* pu64UserData);

/*编码器码率控制模式*/
typedef enum {
    CBR    = 0,                    /**< 固定比特率*/
    VBR    = 1                     /**< 可变比特率*/
}CN_VENC_RC_t;

/*裁剪编码*/
typedef struct{
  CN_BOOL             bEnable;          /**< 是否使能裁剪编码 */
  CN_U32              reserved;
  CN_RECT_S           crop_rect;        /**< 裁剪区域 */
}CN_VENC_CropCfg_t;

typedef struct _CN_VENC_ATTR_H264_CBR_S{
    CN_U32      u32Gop;                                  /* the interval of ISLICE. */
    CN_U32      u32StatTime;                             /* the rate statistic time, the unit is senconds(s) */
    CN_U32      u32SrcFrmRate;                           /* the input frame rate of the venc chnnel */
    CN_U32      fr32DstFrmRate;                         /* the target frame rate of the venc chnnel */
    CN_U32      u32BitRate;                              /* average bitrate */
    CN_U32      u32FluctuateLevel;                       /* level [0..5].scope of bitrate fluctuate. 1-5: 10%-50%. 0: SDK optimized, recommended; */
} CN_VENC_ATTR_H264_CBR_S;

typedef struct _CN_VENC_ATTR_H264_VBR_S{
    CN_U32      u32Gop;                                  /* the interval of ISLICE. */
    CN_U32      u32StatTime;                             /* the rate statistic time, the unit is senconds(s) */
    CN_U32      u32SrcFrmRate;                           /* the input frame rate of the venc chnnel */
    CN_U32      fr32DstFrmRate;                         /* the target frame rate of the venc chnnel */
    CN_U32      u32MaxBitRate;                           /* the max bitrate */
    CN_U32      u32MaxQp;                                /* the max qp */
    CN_U32      u32MinQp;                                /* the min qp */
}CN_VENC_ATTR_H264_VBR_S;

typedef enum {
    CN_PROFILE_BASELINE = 0,
    CN_PROFILE_MAIN,
    CN_PROFILE_HIGH
}CN_VENC_H264_PROFILE_E;

typedef enum {
    CN_H264_PSLICE = 1,
    CN_H264_ISLICE = 5,
    CN_H264_SEI = 6,
    CN_H264_SPS = 7,
    CN_H264_PPS = 8
}CN_VENC_H264_DATA_TYPE;


/*编码器创建属性*/
typedef struct{
    CN_U32                         u32VencDeviceID;             /**< 板卡设备号*/
    CN_VIDEO_CODEC_TYPE_E          VideoCodecType;              /**< 编码格式：支持MJPEG、H264、JPEG*/
    CN_VENC_RC_t                   rate_control_mode;           /**< 码率控制模式*/
    CN_U32                         u32MaxWidth;                 /**< 编码图像最大宽度*/
    CN_U32                         u32MaxHeight;                /**< 编码图像最大高度*/
    CN_PIXEL_FORMAT_E              pixel_format;                /**< 输入像素格式：支持YUV420SP,BGR24,RGB24*/
    CN_U32                         u32TargetWidth;              /**< 保留，设置为0 */
    CN_U32                         u32TargetHeight;             /**< 保留，设置为0 */
    union {
    CN_VENC_ATTR_H264_CBR_S        H264CBR;                     /**< h264 CBR码率控制参数*/
    CN_VENC_ATTR_H264_VBR_S        H264VBR;                     /**< h264 VBR码率控制参数*/
    };
    CN_BOOL                        bcolor2gray;                 /**< 灰度编码设置 */
    CN_VENC_CropCfg_t              encode_crop;                 /**< 裁剪编码配置 */
    CN_U32                         h264_profile;                /**< h264 profile*/
    CN_U32                         jpeg_qfactor;                /**< jpeg qulity factor, 1-99*/
    CN_MLU_P2P_ATTR_S              mluP2pAttr;                  /**< P2P模式MLU缓存配置信息 */
    CN_VENC_CALLBACK               pEncodeCallBack;             /**< 编码图像回调函数指针*/
    void*                          pu64UserData;                /**< 回调函数用户上下文*/
}CN_VENC_CREATE_ATTR_S;

typedef struct {
    CN_U32                          u32DeviceID;
    CN_U32                          u32MluIndex;                       /**< mlu index，用于申请mlu内存时向cnrt接口提供device index */
    CN_U32                          u32FreeChannels;                   /**< 空闲编码通道数 */
    CN_U32                          u32UsedChannels;                   /**< 占用编码通道数 */
}CN_VENC_DEVICE_CAPABILITY_S;

#define MAX_VENC_DEVICE_NUM         (16)

typedef struct {
    CN_U32                          u32VencDeviceNum;
    CN_VENC_DEVICE_CAPABILITY_S     VencDeviceList[MAX_VENC_DEVICE_NUM];
}CN_VENC_CAPABILITY_S;

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VENC_GetCapability
// 描述：编码器能力集
// 参数：
//  [in/out]pstCapability - Parser返回的帧数据
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VENC_GetCapability(CN_VENC_CAPABILITY_S *pstCapability);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VENC_Create
// 描述：创建编码器
// 参数：
//  [out]phEncoder - 返回编码器句柄
//  [in]pstCreateAttr -  编码器属性
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VENC_Create(CN_HANDLE_VENC *phEncoder, CN_VENC_CREATE_ATTR_S *pstCreateAttr);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VENC_Destroy
// 描述：销毁编码器
// 参数：
//  [in]hEncoder - 编码器句柄
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VENC_Destroy(CN_HANDLE_VENC hEncoder);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VENC_Send
// 描述：编码器 送图
// 参数：
//  [in]hEncoder - 编码器句柄
//  [in]pstPicParams - 图片数据
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VENC_Send(CN_HANDLE_VENC hEncoder, CN_VIDEO_PIC_PARAM_S *pstPicParams);

////////////////////////////////////////////////////////////////////////////////
// CN_MPI_VENC_SendEx
// 描述：解码器 送流
// 参数：
//  [in]hEncoder - 编码器句柄
//  [in]pstPicParams - 图片数据
//  [in]u32MilliSec - 送流方式
//                  -1：阻塞
//                   0：非阻塞
//                 正值：超时时间，没有上限值，以ms为单位
// 返回值： CNResult
//  错误代码。
////////////////////////////////////////////////////////////////////////////////
CNResult CNAPI CN_MPI_VENC_SendEx(CN_HANDLE_VENC hEncoder, CN_VIDEO_PIC_PARAM_S *pstPicParams, CN_S32 s32MilliSec);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif  //  INCLUDE_CNCODEC_H_



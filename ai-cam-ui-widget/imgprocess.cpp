#include "imgprocess.h"

const char *model_path = "/root/rknn_yolov5_demo/model/yolov5s_relu.rknn";

ImgProcess::ImgProcess()
{
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    init_post_process();
    int ret = init_yolov5_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        printf("init_yolov5_model fail! ret=%d model_path=%s\n", ret, model_path);
    }
}

void ImgProcess::doProcess(image_buffer_t src_image)
{
    // 执行推理
    object_detect_result_list od_results;
    int ret = inference_yolov5_model(&rknn_app_ctx, &src_image, &od_results);
    if (ret != 0)
    {
        printf("inference_yolov5_model fail! ret=%d\n", ret);
    }

    // 画框和概率
    char text[256];
    for (int i = 0; i < od_results.count; i++)
    {
        object_detect_result *det_result = &(od_results.results[i]);
        printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
                det_result->box.left, det_result->box.top,
                det_result->box.right, det_result->box.bottom,
                det_result->prop);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

        sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
        draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
    }

    // 转换为RGB888格式
    int height = 640;
    int width = 640;
    cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, src_image.virt_addr);
    cv::Mat frame(height, width, CV_8UC3);
    cv::cvtColor(yuv420sp, frame, cv::COLOR_YUV420sp2BGR); // COLOR_YUV2RGB_NV12

    std::cout << "imgprocess" << std::endl;

    emit proFinished(frame);

}

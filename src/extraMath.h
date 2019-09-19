#ifndef _EXTRAMATH_H_
#define _EXTRAMATH_H_
#include <opencv2/core/utility.hpp>

namespace tmpst{
    //void corrolateMatrix(cv::Mat & frame_matrix, int shift_max);
    //void shiftRight(cv::Mat & matrix_out, cv::Mat matrix_in, int amount);
    double corrolation(const cv::Mat & one, const cv::Mat & two);

    void shiftImage(const cv::Mat image_in, cv::Mat & image_out, int x, int y); // shifts the image a certain amount left and right
}

#endif

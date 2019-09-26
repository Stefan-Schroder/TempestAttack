#ifndef _EXTRAMATH_H_
#define _EXTRAMATH_H_
#include <opencv2/core/utility.hpp>
#include <unordered_map>

namespace tmpst{
    double corrolation(const cv::Mat & one, const cv::Mat & two);
    void shiftImage(cv::Mat image_in, cv::Mat & image_out, int x, int y); 

    std::pair<int,unsigned int> mapMode(std::unordered_map<int, unsigned int> map);

}

#endif

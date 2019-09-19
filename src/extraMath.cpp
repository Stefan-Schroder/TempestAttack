#include "extraMath.h"
#include <iostream>
#include <opencv2/core/utility.hpp>

using namespace cv;

namespace tmpst{

    /**
     * corrolation of two matrices
     */
    double corrolation(const cv::Mat & one, const cv::Mat & two){
        double answer = one.cols*sum(one.mul(two))[0];
        answer -= sum(one)[0]*sum(two)[0];
        answer /= sqrt(
                (one.cols*sum(one.mul(one))[0] - pow(sum(one)[0],2)) *
                (two.cols*sum(two.mul(two))[0]-pow(sum(two)[0],2))
                );


        return answer;
    }

    /**
     * shifts the image a certain amount left and right
     */
    void shiftImage(cv::Mat image_in, cv::Mat & image_out, int x, int y){
        // === shift columns (x) ===
        
        bool left = x < 0;

        if(left && x!=0){
            x *= -1; //invert the shift
            x = x%image_out.cols;
            
            image_in.colRange(0,x).copyTo(image_out.colRange(image_out.cols-x, image_out.cols));
            image_in.colRange(x,image_out.cols).copyTo(image_out.colRange(0,image_out.cols-x));
        }else if(x!=0){
            x = x%image_out.cols;

            image_in.colRange(image_out.cols-x,image_out.cols).copyTo(image_out.colRange(0, x));
            image_in.colRange(0,image_out.cols-x).copyTo(image_out.colRange(x,image_out.cols));
        }

        // === shift rows (y) ===
        image_in = image_out.clone();

        bool down = y < 0;

        if(down && y!=0){
            y *= -1; //invert the shift
            y = y%image_out.rows;
            
            image_in.rowRange(0,y).copyTo(image_out.rowRange(image_out.rows-y, image_out.rows));
            image_in.rowRange(y,image_out.rows).copyTo(image_out.rowRange(0,image_out.rows-y));
        }else if(y!=0){
            y = y%image_out.rows;

            image_in.rowRange(image_out.rows-y,image_out.rows).copyTo(image_out.rowRange(0, y));
            image_in.rowRange(0,image_out.rows-y).copyTo(image_out.rowRange(y,image_out.rows));
        }

    }
}

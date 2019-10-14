#include "extraMath.h"
#include <iostream>
#include <opencv2/core/utility.hpp>

using namespace cv;
using namespace std;

namespace tmpst{

    /**
     * correlation of two matrices
     */
    double correlation(const cv::Mat & one, const cv::Mat & two){
        /* Poisons correlation
        double answer = one.cols*sum(one.mul(two))[0];
        answer -= sum(one)[0]*sum(two)[0];
        answer /= sqrt(
                (one.cols*sum(one.mul(one))[0] - pow(sum(one)[0],2)) *
                (two.cols*sum(two.mul(two))[0] - pow(sum(two)[0],2))
                );
        */

        double answer = sum(one.mul(two))[0];

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


    pair<int,unsigned int> mapMode(std::unordered_map<int, unsigned int> map){
        int max_index = 0;
        unsigned int max_value = 0;

        auto begin = map.begin(), end = map.end();

        while(begin!=end){
            cout << "shift: " << begin->first << " has " << begin->second << endl;
            if(begin->second > max_value){
                
                max_index = begin->first;
                max_value = begin->second;
            }

            begin++;

        }
        return make_pair(max_index, max_value);
    }

}

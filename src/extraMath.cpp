#include "extraMath.h"
#include <iostream>
#include <opencv2/core/utility.hpp>

using namespace cv;

namespace tmpst{
    /* unsued
    void shiftRight(Mat & matrix_out, Mat matrix_in, int amount){
        bool left = amount < 0;

        if(left){
            amount = -1*amount;
            amount = amount%matrix_out.cols;
            

            matrix_in.colRange(0,amount).copyTo(matrix_out.colRange(matrix_out.cols-amount, matrix_out.cols));
            matrix_in.colRange(amount,matrix_out.cols).copyTo(matrix_out.colRange(0,matrix_out.cols-amount));

        }else{
            amount = amount%matrix_out.cols;

            matrix_in.colRange(matrix_out.cols-amount,matrix_out.cols).copyTo(matrix_out.colRange(0, amount));
            matrix_in.colRange(0,matrix_out.cols-amount).copyTo(matrix_out.colRange(amount,matrix_out.cols));

        }

    }

    void corrolateMatrix(Mat & frame_matrix, int shift_max){
        vector<int> shift_amount(frame_average);
        shift_amount[0] = 0;

        //calculate the best shifts (first frame does not shift)
        for(int i=frame_average-1; i>=1; i--){
            Mat shifting_frame = frame_matrix.row(i);
            double highest_corr = 0;
            int best_shift = 0;

            for(int j=-shift_max; j<=shift_max; j++){
                shiftRight(shifting_frame, shifting_frame, j);
                double corr = corrolation(shifting_frame, frame_matrix.row(i-1));
                if(corr > highest_corr){
                    highest_corr = corr;
                    best_shift = j;
                }
            }

            shift_amount[i] = best_shift;
            cout << best_shift << endl;
        }

        int total_shift = 0;
        for(int i=1; i<frame_average; i++){
            total_shift += shift_amount[i];
            Mat placeHolder = frame_matrix.row(i);
            shiftRight(placeHolder, placeHolder, total_shift);
            frame_matrix.row(i) = placeHolder;
        }

    }
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

    void shiftImage(const cv::Mat image_in, cv::Mat & image_out, int x, int y){
        // === shift columns (x) ===
        
        bool left = x < 0;

        if(left){
            x *= -1; //invert the shift
            x = x%image_out.cols;
            
            image_out.colRange(image_out.cols-x, image_out.cols) = image_in.colRange(0,x).clone();
            image_out.colRange(0,image_out.cols-x) = image_in.colRange(x,image_out.cols).clone();

            //image_in.colRange(0,amount).copyTo(image_out.colRange(image_out.cols-amount, image_out.cols));
            //image_in.colRange(amount,image_out.cols).copyTo(image_out.colRange(0,image_out.cols-amount));
        }else{
            x = x%image_out.cols;

            image_out.colRange(0, x) = image_in.colRange(image_out.cols-x,image_out.cols).clone();
            image_out.colRange(x,image_out.cols) = image_in.colRange(0,image_out.cols-x).clone();

            //image_in.colRange(image_out.cols-x,image_out.cols).copyTo(image_out.colRange(0, x));
            //image_in.colRange(0,image_out.cols-x).copyTo(image_out.colRange(x,image_out.cols));
        }

        // === shift rows (y) ===

        bool down = x < 0;

        if(down){
            y *= -1; //invert the shift
            y = y%image_out.rows;
            
            image_out.rowRange(image_out.rows-y, image_out.rows) = image_in.rowRange(0,y).clone();
            image_out.rowRange(0,image_out.rows-y) = image_in.rowRange(y,image_out.rows).clone();

            //image_in.rowRange(0,y).copyTo(image_out.rowRange(image_out.rows-y, image_out.rows));
            //image_in.rowRange(y,image_out.rows).copyTo(image_out.rowRange(0,image_out.rows-y));
        }else{
            y = y%image_out.rows;

            image_out.rowRange(0, y) = image_in.rowRange(image_out.rows-y,image_out.rows).clone();
            image_out.rowRange(y,image_out.rows) = image_in.rowRange(0,image_out.rows-y).clone();

            //image_in.rowRange(image_out.rows-y,image_out.rows).copyTo(image_out.rowRange(0, y));
            //image_in.rowRange(0,image_out.rows-y).copyTo(image_out.rowRange(y,image_out.rows));
        }
    }
}

#ifndef _FRAMESTREAM_H_
#define _FRAMESTREAM_H_

#include <uhd/usrp/multi_usrp.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>


namespace tmpst{
    class frameStream{

    private:
        cv::Mat all_samples;

        // ================================ USER INPUT ========================================
        int width;
        int height;
        double refresh;
        double frequency;
        double sample_rate;
        int frame_average;
        bool verbose;



        // ================================= CALCULATED =======================================

        
        // the amount of samples in all_samples that are usable (not including extra frame)
        int total_sample_count;

        long pixels_per_image;       // the number of pixels the sampling rate allows for

        cv::Mat final_image;
        cv::Mat final_mini_image;
        
        // ========================= SAMPLE PROCESSORS Internal ===============================

        cv::Mat makeMatrix(int start_index);
        int shiftIndex(int index, int amount); //just normal summantion, but with error checking
        void corrolateFrames(std::vector<int> & indices, int shift_max);
        cv::Mat averageFrames(std::vector<int> & indices);

        void centerImage(cv::Mat & image);
        void writeMiniFrame(cv::Mat & samples);

    public:

        // =============================== INITIALIZERS ======================================
        
        frameStream();
        frameStream(int width, int height, 
                    double refresh, double frequency, 
                    int frame_average,
                    double sample_rate,
                    bool verbose);
        
        // =============================== LOADING DATA ======================================

        bool loadDataRx(uhd::usrp::multi_usrp::sptr usrp, double offset, size_t channel, int frame_ignore);

        bool loadDataFile(std::string filename, int frame_ignore);

        // ============================== SAMPLE PROCESSORS ==================================

        int processSamples(int shift_max);

        void createFinalFrame(int shiftAmount);

        // ==================================== EXTRA  =======================================

        bool saveImage(std::string filename);


    };

}
#endif

#ifndef _FRAMESTREAM_H_
#define _FRAMESTREAM_H_
#include <uhd/usrp/multi_usrp.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <unordered_map>
#ifndef _TEMPEST_H_
#include "extraMath.h"
#endif


namespace tmpst{
    class frameStream{

    private:
        // =============================== DATA SOURCES ========================================
        cv::Mat all_samples;
        cv::Mat final_image;
        cv::Mat final_mini_image;

        std::vector<int> indices;

        // =============================== USER SPECIFIC =======================================
        int width;
        int height;
        double refresh;
        double frequency;
        double sample_rate;
        int frame_average;
        bool verbose;
        bool inverted;
        bool interlaced;
        std::string output_directory;

        // ================================= CALCULATED =======================================

        
        // the amount of samples in all_samples that are usable (not including extra frame)
        int total_sample_count;

        long pixels_per_image;       // the number of pixels the sampling rate allows for
        
        // ========================= SAMPLE PROCESSORS Internal ===============================

        cv::Mat makeMatrix(int start_index, cv::Mat & samples);
        int shiftIndex(int index, int amount); //just normal summantion, but with error checking
        std::unordered_map<int, unsigned int> corrolateFrames(int shift_max);
        cv::Mat averageFrames(std::vector<int> & indices);

        std::pair<int,int> centerImage(cv::Mat & image);
        float writeMiniFrame(cv::Mat & samples);

    public:

        // =============================== INITIALIZERS ======================================
        
        frameStream();
        frameStream(int width, int height, 
                    double refresh, double frequency, 
                    int frame_average,
                    double sample_rate,
                    bool verbose,
                    std::string dir_name);
        
        double getFrequency();

        cv::Mat getFinalImage();
        // =============================== LOADING DATA ======================================

        bool loadDataRx(uhd::usrp::multi_usrp::sptr usrp, double offset, size_t channel, int frame_ignore);

        bool loadDataFile(std::string filename, int frame_ignore);

        // ============================== SAMPLE PROCESSORS ==================================

        std::pair<int, unsigned int> processSamples(int shift_max);

        void createFinalFrame(int shiftAmount);

        // ==================================== EXTRA  =======================================

        cv::Mat reconInterlace(cv::Mat interlaced);

        bool saveImage(std::string filename);
        
        // ============================== FFT PROCESSORS =====================================

        //void shiftFrequency(double amount);


    };

}
#endif

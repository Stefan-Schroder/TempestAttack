#ifndef _FRAMESTREAM_H_
#define _FRAMESTREAM_H_

#include <uhd/usrp/multi_usrp.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs.hpp>

namespace tmpst{
    class frameStream{
    private:
        cv::Mat all_samples;
        cv::Mat final_image;
        int width;
        int height;
        double refresh;
        double frequency;
        double sample_rate;

        int frame_average;
        int pixels_per_image;       // the number of pixels the sampling rate allows for


        bool verbose;

    public:

        frameStream();
        frameStream(int width, int height, 
                    double refresh, double frequency, 
                    int frame_average,
                    double sample_rate,
                    bool verbose);
        
        bool loadDataRx(uhd::usrp::multi_usrp::sptr usrp, double offset, size_t channel, int frame_ignore);

        bool loadDataFile(std::string filename, int frame_ignore);

        void processSamples();

        bool saveImage(std::string filename);

        bool saveStream(std::string filename);

    };

}
#endif

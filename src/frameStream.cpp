#include "frameStream.h"
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <fstream>
#include <iterator>

using namespace cv;
using namespace std; 

namespace tmpst{

    frameStream::frameStream() {};
    frameStream::frameStream( int width, int height, 
                        double refresh, 
                        double frequency, 
                        int frame_average,
                        double sample_rate,
                        bool verbose):

                        width(width), height(height), refresh(refresh),
                        frequency(frequency),
                        frame_average(frame_average),
                        sample_rate(sample_rate),
                        verbose(verbose){

        pixels_per_image = sample_rate/refresh;

        int sample_size = pixels_per_image*frame_average;
        if(verbose) cout << "Samples to read in: " << sample_size << endl;

        all_samples = Mat::zeros(1,sample_size, CV_16U);
        final_image = Mat::zeros(height, width, CV_8U);
    }

    bool frameStream::loadDataFile(string filename, int frame_ignore){
        cout << filename << endl;
        //reading in file
        ifstream input_stream(filename.c_str(), ios::binary);

        input_stream.seekg(pixels_per_image*frame_ignore*2*2, std::ios::cur);

        short I_sample, Q_sample;
        int counter = 0;
        while(counter<pixels_per_image*frame_average && 
                input_stream.read((char *)&I_sample, sizeof(short)) &&
                input_stream.read((char *)&Q_sample, sizeof(short))){
            
            all_samples.at<short>(0,counter++) = sqrt(pow(I_sample,2)+pow(Q_sample,2));
        }

        input_stream.close();

        return true;
    }

    bool frameStream::loadDataRx(uhd::usrp::multi_usrp::sptr usrp, double offset, size_t channel, int frame_ignore){
        uhd::tune_request_t tune_request(frequency, offset);
        usrp->set_rx_freq(tune_request, channel);


    }

    /**
     * Main processing of images done here.
     * All of the samples, captured either by receiver of input file are processed into one image.
     * This single image is the average of the average_amount of images given by the user.
     * The images are aligned then processed
     */
    void frameStream::processSamples(){
        Mat one_frame = Mat(1, pixels_per_image, CV_16U);

        all_samples.colRange(1, pixels_per_image).copyTo(one_frame);

        Mat stretch = Mat(1,width*height, CV_16U);
        resize(one_frame, stretch, Size(width*height,1));

        final_image = stretch.reshape(0,height);

        // normalize frame to usigned char
        normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);
    }

    bool frameStream::saveImage(string filename){
        imwrite("data/"+filename+"-image.jpg", final_image);
        return true;
    }


}

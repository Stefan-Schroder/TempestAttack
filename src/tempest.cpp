#include "tempest.h"
#include <uhd/usrp/multi_usrp.hpp>
#include "frameStream.h"
#include <thread>
#include <omp.h>
#include <unordered_map>
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

namespace tmpst{

    tempest::tempest(   uhd::usrp::multi_usrp::sptr usrp,
                        string name,
                        int width, int height, double refresh,
                        int bndwidth_mult, int frame_av, double overlap,
                        double center_frequ, double sample_rate,
                        double offset,
                        size_t channel,
                        int frame_ignore,
                        int max_shift,
                        bool inverted,
                        bool interlaced,
                        bool verbose):

                            usrp(usrp),
                            name(name),
                            width(width), height(height), refresh(refresh),
                            bandwidth_multiples(bndwidth_mult), frame_av_num(frame_av),
                            bandwidth_overlap(overlap),
                            base_center_freq(center_frequ), sample_rate(sample_rate),
                            offset(offset),
                            channel(channel),
                            frame_ignore(frame_ignore),
                            max_shift(max_shift), 
                            interlaced(interlaced),
                            inverted(inverted),
                            verbose(verbose){

        full_spectrum_size = width*height*refresh;
        if(bandwidth_multiples==0){
            bandwidth_multiples = full_spectrum_size/(sample_rate*(overlap));
            if(verbose) cout << "Number of bandwidths: " << bandwidth_multiples << endl;
        }
    }

    tempest::tempest(   string input_file,
                        string name,
                        int width, int height, double refresh,
                        int frame_av,
                        double sample_rate,
                        int frame_ignore,
                        int max_shift,
                        bool inverted,
                        bool interlaced,
                        bool verbose):

                            input_file(input_file),
                            name(name),
                            width(width), height(height), refresh(refresh),
                            frame_av_num(frame_av),
                            sample_rate(sample_rate),
                            frame_ignore(frame_ignore),
                            max_shift(max_shift), 
                            interlaced(interlaced),
                            inverted(inverted),
                            verbose(verbose){

        full_spectrum_size = width*height*refresh;
        from_file = true;

        base_center_freq = 0;
        bandwidth_multiples = 1;
    }

    void tempest::initializeBands(){
        bands = vector<tmpst::frameStream>(bandwidth_multiples);

        if(verbose) cout << endl << "Adding new recordings with base band: " << endl;
        for(int i=0; i<bandwidth_multiples; i++){
            double band_center = base_center_freq+i*sample_rate*bandwidth_overlap;
            if(verbose) cout << "\t" << band_center << endl;

            tmpst::frameStream newFrame(width, height, refresh,
                                        band_center,
                                        frame_av_num, sample_rate, inverted, interlaced, verbose, name);

            bands[i] = newFrame;
        }

    }

    void tempest::processBands(){

        if(from_file){ // there can only be one band
            // ====================== READING FROM FILE ==============================
            bands[0].loadDataFile(input_file, frame_ignore);
            int shifting = bands[0].processSamples(max_shift).first;
            cout << "tmpst: " << shifting << endl;
            bands[0].createFinalFrame(shifting);
            bands[0].saveImage("final_image-"+to_string(bands[0].getFrequency()));

        }else{
            // ===================== READING FROM RECIEVER ============================

            unordered_map<int, unsigned int> best_shifts; // storing the best shifts
#pragma omp parallel for
            for(int i=0; i<bands.size(); i++){

#pragma omp critical
                //======== Loading file ==========
                {
                if(verbose) cout << endl << "Loading in data for band " << i << endl;
                bands[i].loadDataRx(usrp, offset, channel, frame_ignore);
                if(verbose) cout << "Reading complete" << endl;
                }

                if(verbose) cout << endl << "Processing data." << endl;

                //======== Processing file ==========
                pair<int, unsigned int> shift = bands[i].processSamples(max_shift);
                if(verbose) cout << "Band " << i << " shifted by " << shift.first << " percentage of shifted frames " << double(shift.second) << endl;


                //======== Finding best shift ==========
#pragma omp critical
                {
                best_shifts[shift.first]++;
                }
            }

            //======== final processing file ==========
            int shift_amount = mapMode(best_shifts).first;
            for(int i=0; i<bands.size(); i++){
                bands[i].createFinalFrame(shift_amount);
                bands[i].saveImage("final_image-"+to_string(bands[i].getFrequency()));
            }

        }
    }

    void tempest::combineBands(){
        Mat main_band = bands[0].getFinalImage();
        
        int xboard = 600, yboard = 200;

        Mat big_band = Mat::zeros(height+yboard, width+xboard, CV_8U);
        randn(big_band, Scalar(5), Scalar(20));

        main_band.copyTo(big_band(Rect((big_band.cols - main_band.cols)/2, (big_band.rows - main_band.rows)/2, main_band.cols, main_band.rows)));

        bands[0].saveImage("shifted_image-"+to_string(bands[0].getFrequency()));

        for(int i=1; i<bands.size(); i++){
            Mat next_band = bands[i].getFinalImage();
            int result_cols = big_band.cols - next_band.cols + 1;
            int result_rows = big_band.rows - next_band.rows + 1;

            Mat result = Mat::zeros(result_rows, result_cols, CV_32F);

            // match then normalize
            //matchTemplate( big_band, next_band, result, CV_TM_SQDIFF_NORMED);
            matchTemplate( big_band, next_band, result, CV_TM_CCOEFF_NORMED);

            // normalize
            normalize(result, result, 0, 255, NORM_MINMAX, -1, Mat() );

            double minVal, maxVal;
            Point minLoc, maxLoc;
            Point matchLoc;

            minMaxLoc( result, &minVal, &maxVal, &minLoc, &maxLoc, Mat());

            matchLoc = maxLoc;

            shiftImage(next_band.clone(), next_band, -(xboard/2 - matchLoc.x), -(yboard/2 - matchLoc.y));

            bands[i].saveImage("shifted_image-"+to_string(bands[i].getFrequency()));

        }

        // combine bands (weighted average of frames, ie first band is 50% second 25% third 12.5% etc)
        Mat combine_image = bands[bands.size()-1].getFinalImage().clone(); //Mat::zeros(height,width, CV_8U);
        for(int i=bands.size()-2; i>=0; i--){
            combine_image = (combine_image+bands[i].getFinalImage())/2;
        }

        //normalize the result
        normalize(combine_image, combine_image, 0, 255, NORM_MINMAX, CV_8UC1);

        //save final image
        imwrite(name+"combined_bands.jpg", combine_image);

    }



}

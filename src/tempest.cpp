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
                        bool verbose):

                            input_file(input_file),
                            name(name),
                            width(width), height(height), refresh(refresh),
                            frame_av_num(frame_av),
                            sample_rate(sample_rate),
                            frame_ignore(frame_ignore),
                            max_shift(max_shift), 
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
                                        frame_av_num, sample_rate, verbose, name);

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
        
        int xboard = 300 , yboard = 300;

        Mat big_band = Mat::zeros(height+yboard, width+xboard, CV_8U);

        main_band.copyTo(big_band(Rect((big_band.cols - main_band.cols)/2, (big_band.rows - main_band.rows)/2, main_band.cols, main_band.rows)));

        bands[0].saveImage("shifted_image-"+to_string(bands[0].getFrequency()));
        //imwrite("0-"+to_string(bands[0].getFrequency())+".jpg", big_band);
        for(int i=1; i<bands.size(); i++){
            Mat next_band = bands[i].getFinalImage();
            int result_cols = big_band.cols - next_band.cols + 1;
            int result_rows = big_band.rows - next_band.rows + 1;

            Mat result = Mat::zeros(result_rows, result_cols, CV_32F);

            // match then normalize
            matchTemplate( big_band, next_band, result, CV_TM_SQDIFF_NORMED);

            // normalize
            normalize(result, result, 0, 255, NORM_MINMAX, -1, Mat() );

            double minVal, maxVal;
            Point minLoc, maxLoc;
            Point matchLoc;

            minMaxLoc( result, &minVal, &maxVal, &minLoc, &maxLoc, Mat());

            matchLoc = minLoc;

            //cout << " corrolation shift: " << matchLoc.x << "x" << matchLoc.y << endl;

            //test
            /*
            Mat other_big = Mat::zeros(height+yboard, width+xboard, CV_8U);
            rectangle( big_band, matchLoc, Point( matchLoc.x + next_band.cols , matchLoc.y + next_band.rows ), Scalar::all(0), 2, 8, 0 );

            next_band.copyTo(other_big(Rect(matchLoc.x, matchLoc.y, next_band.cols, next_band.rows )));
            imshow("big image", big_band);
            waitKey(0);
            imwrite(to_string(i)+"-"+to_string(bands[0].getFrequency())+".jpg", other_big);
            //stop test
            */

            shiftImage(next_band.clone(), next_band, -(xboard/2 - matchLoc.x), -(yboard/2 - matchLoc.y));

            bands[i].saveImage("shifted_image-"+to_string(bands[i].getFrequency()));

        }

        // combine frequencys
        //bands[0].shiftFrequency(0.3);

    }

}

#include "frameStream.h"
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <fstream>
#include <iterator>
#include <vector>
#include "extraMath.h"
#include "omp.h"
#include <functional>

using namespace cv;
using namespace std; 

namespace tmpst{

    // ===================================================================================
    // =============================== INITIALIZERS ======================================
    // ===================================================================================

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

        pixels_per_image = round(sample_rate/refresh);

        int sample_size = pixels_per_image*(frame_average+1); // +1 for extra frame
        if(verbose) cout << "Samples to read in: " << sample_size << endl;

        all_samples = Mat::zeros(1,sample_size, CV_16U);
        indices = vector<int>(frame_average);

        total_sample_count = pixels_per_image*frame_average;

    }

    double frameStream::getFrequency(){ return frequency; }
    // ===================================================================================
    // =============================== LOADING DATA ======================================
    // ===================================================================================

    bool frameStream::loadDataFile(string filename, int frame_ignore){
        cout << filename << endl;
        //reading in file
        ifstream input_stream(filename.c_str(), ios::binary);

        input_stream.seekg(pixels_per_image*frame_ignore*2*2, std::ios::cur);

        short I_sample, Q_sample;
        int counter = 0;
        while(counter<pixels_per_image*(frame_average+1) &&  // + 1 is for the extra frame used in shifting
                input_stream.read((char *)&I_sample, sizeof(short)) &&
                input_stream.read((char *)&Q_sample, sizeof(short))){
            
            all_samples.at<short>(0,counter++) = sqrt(pow(I_sample,2)+pow(Q_sample,2));

            if(input_stream.peek() == EOF){
                cout << "Input file not long enough for current average frame amount/sample rate" << endl;
                cout << "Try decreasing your --average." << endl;
                cout << "This can happen because you need to record one more frame than you can use (for shifting purposes" << endl;
                return false;
            }

        }

        input_stream.close();

        return true;
    }

    bool frameStream::loadDataRx(uhd::usrp::multi_usrp::sptr usrp, double offset, size_t channel, int frame_ignore){
        if(verbose) cout << "Scanning frequency: " << frequency/1000000 << "MHz" << endl;
        uhd::tune_request_t tune_request(frequency, offset);
        usrp->set_rx_freq(tune_request, channel);

        long sample_size = pixels_per_image*(frame_average+frame_ignore+1); // total number of samples to take (include one extra)

        uhd::stream_args_t stream_arguments("sc16","sc16"); //setting both to complex shorts for efficiency

        // setting up channels
        vector<size_t> channels;
        channels.push_back(channel);
        stream_arguments.channels = channels;

        // get receiver stream obj
        uhd::rx_streamer::sptr receiver_stream = usrp->get_rx_stream(stream_arguments);

        uhd::rx_metadata_t meta_data;

        //create receiver buffer
        vector<complex<short>> buffer(sample_size);

        // setup the actual streaming
        uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
        stream_cmd.num_samps    = size_t(sample_size);
        stream_cmd.stream_now   = true;
        stream_cmd.time_spec    = uhd::time_spec_t();
        receiver_stream->issue_stream_cmd(stream_cmd);


        // run until buffer is filled
        long received_samps = 0;
        while (received_samps!=sample_size){ // streaming

            size_t num_rx_samps = receiver_stream->recv(&buffer.front(), buffer.size(), meta_data, 3.0, false);

            // receiver error handeling
            if(meta_data.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT){
                cout << "Time out while receiving!" << endl;
                return false;
            }else if(meta_data.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW){
                cout << "Receiver overflow!" << endl;
                return false;
            }else if(meta_data.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
                cerr << "Unknown receiver error: " << meta_data.strerror() << endl;
                return false;
            }

            received_samps += num_rx_samps;
        }

        // stop streaming
        stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
        receiver_stream->issue_stream_cmd(stream_cmd);


        // put all streamed data into IQ samples and save to all_samples
        if(verbose) cout << "Received samples: " << buffer.size() << endl;
        if(verbose) cout << "Saved samples: " << pixels_per_image*frame_average << endl;

        for(unsigned int i=0; i<pixels_per_image*(frame_average+1); i++){ // +1 for extra frame
            short sample = abs(buffer[pixels_per_image*frame_ignore + i]);
            all_samples.at<short>(0,i) = sample;
        }

        return true;

    }

    // ===================================================================================
    // ============================== SAMPLE PROCESSORS ==================================
    // ===================================================================================
    
    /**
     * Main processing of images done here.
     * All of the samples, captured either by receiver of input file are processed into one image.
     * This single image is the average of the average_amount of images given by the user.
     * The images are aligned then processed
     *
     * The function returns the amount their frames were shifted by.
     */
    pair<int, double> frameStream::processSamples(int shift_max){

        for(int i=0; i<frame_average; i++){
            indices[i] = pixels_per_image*i;

            // Save all frames
            Mat one_frame = makeMatrix(indices[i], all_samples);
            Mat stretch = Mat(1,width*height, CV_16U);
            resize(one_frame, stretch, Size(width*height,1));
            final_image = Mat::zeros(height, width, CV_8U);
            final_image = stretch.reshape(0,height);
            normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

            saveImage(to_string(i)+"-before_shift");
            final_image.release();
            


        }


        //corrolate frames
        return modeWithPercent(corrolateFrames(shift_max));

    }

    pair<int, double> frameStream::modeWithPercent(unordered_map<int, unsigned int> map){
        pair<int, double> results; // first is the shift, second the amount

        int max_index = 0;
        unsigned int max_value = 0;

        auto begin = map.begin(), end = map.end();

        while(begin!=end){
            if(begin->second > max_value){
                max_index = begin->first;
                max_value = begin->second;
            }

            begin++;
        }

        results.first = max_index;
        results.second = double(max_value)/(frame_average-1);

        cout << "========" << results.second << "========" << endl;
        return results;
    }

    /**
     * Uses all_samples and pixels_per_image, make use the indexes are valid
     */
    Mat frameStream::makeMatrix(int start_index, Mat & samples){
        if(start_index<0 || start_index+pixels_per_image>=samples.cols){
            cerr << "Index's are out of range" << endl;
            cerr << "Your range goes from:" << endl;
            cerr << start_index << "->" << start_index+pixels_per_image << endl;
            cerr << "All samples range goes from:" << endl;
            cerr << 0 << "->" << samples.cols << endl;

            Mat empty;
            return empty;
        }
        else {
            return samples.colRange(start_index, start_index+pixels_per_image);
        }

    }

    int frameStream::shiftIndex(int index, int amount){
        amount = amount%pixels_per_image;
        if(index+amount<0){
            cerr << "Cannot shift the first to the left" << endl;
        }
        return index+amount;

    }

    /**
     * corrolates the frames to that they line up (miss align due to error in refresh rate)
     */
    unordered_map<int, unsigned int> frameStream::corrolateFrames(int shift_max){
        // Average all the samples to get rid of noise
        int window = 10;
        Mat average_filter = Mat::ones(1,window, CV_32F)/window;
        average_filter = average_filter.mul(average_filter);

        
        Mat filtered_samples = Mat::ones(1, all_samples.cols, CV_32F);
        filter2D(all_samples, filtered_samples, -1, average_filter, Point(0,0), 5.0, BORDER_REFLECT);

        cout << "sizes of samples: " << all_samples.cols << ", " << filtered_samples.cols << endl;

        // Start corrolation process
        unordered_map<int, unsigned int> shift_amount_map; // cant be ordered because constantly changing

        //calculate the best shifts (first frame does not shift)
        if(verbose) cout << endl << "Frame:\tShifted" << endl;

        //this parallelism may not be worth it
#pragma omp parallel for
        for(int i=frame_average-1; i>=1; i--){

            double highest_corr = 0;
            int best_shift = 0;

            for(int j=-shift_max; j<=shift_max; j++){
                Mat shifting_frame = makeMatrix(shiftIndex(indices[i],j), filtered_samples);
                double corr = corrolation(shifting_frame, makeMatrix(indices[i-1], filtered_samples));
                //if(abs(corr) > highest_corr){
                if(corr > highest_corr){
                    highest_corr = corr;
                    best_shift = j;
                }
            }

#pragma omp critical
            {
            shift_amount_map[best_shift]++;


            Mat one_frame = makeMatrix(shiftIndex(indices[i],best_shift), filtered_samples);
            Mat stretch = Mat(1,width*height, CV_16U);
            resize(one_frame, stretch, Size(width*height,1));
            final_image = Mat::zeros(height, width, CV_8U);
            final_image = stretch.reshape(0,height);
            normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

            saveImage(to_string(i)+"-after_shift");
            final_image.release();
            }

            if(verbose) cout << i << "\t" << best_shift << endl;
        }

        return shift_amount_map;

    }

    /**
     * Shifts frames then makes them into into one frame.
     * averageFrames is used for this.
     * !!NOTE: pixels_per_image CHANGES AFTER THIS IS RUN (straigtens image out)
     */
    void frameStream::createFinalFrame(int shift_amount){
        //shift frame
        cout << "cff will shift by: " << shift_amount << endl;
        int total_shift = 0;
        for(int i=1; i<frame_average; i++){
            total_shift += shift_amount;

            indices[i] = shiftIndex(indices[i], total_shift);
        }

        //align frame
        if(verbose) cout << endl << "ppi before: " << pixels_per_image << endl;
        pixels_per_image = indices[1] - indices[0]; // SO BIG BRAIN
        if(verbose) cout << "ppi after: " << pixels_per_image << endl;

        // average frames
        Mat one_frame = averageFrames(indices);

        float multiplier = writeMiniFrame(one_frame);

        //clear memory of all samples
        all_samples.release();

        // stretch image to fit into final matrix resolution
        Mat stretch = Mat(1,width*height, CV_16U);
        resize(one_frame, stretch, Size(width*height,1));

        final_image = Mat::zeros(height, width, CV_8U);

        final_image = stretch.reshape(0,height);
        //final_image = imageCopy;

        // normalize frame to usigned char
        normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

        // Center image
        pair<int, int> amount = centerImage(final_mini_image); // centers the smaller 

        shiftImage(final_image.clone(), final_image, -amount.first*multiplier, -amount.second*multiplier);

    }

    /**
     * Averages the frames from all_samples together to make the final array
     * The average array is made using vector of starting indices of each frame
     */
    Mat frameStream::averageFrames(std::vector<int> & indices){
        Mat sum_frames = Mat::zeros(1, pixels_per_image, CV_32F); //larger size to handle summantion

        auto begin = indices.begin(), end = indices.end();
        while(begin!=end){
            Mat frame = makeMatrix(*begin++, all_samples);
            frame.convertTo(frame,CV_32F);
            sum_frames += frame/frame_average;
        }
        sum_frames = sum_frames/frame_average;

        return sum_frames;
    }


    // ===================================================================================
    // ==================================== EXTRA  =======================================
    // ===================================================================================

    bool frameStream::saveImage(string filename){
        imwrite("data/"+filename+".jpg", final_image);
        return true;
    }

    float frameStream::writeMiniFrame(Mat & samples){
        //reduce the amount of pixels for miniframe
        pair<int, int> reduced(width, height);

        // reduction lambda
        //auto reduce = [this](pair<int, int> frac)->pair<int,int>{
        function<pair<int,int>(pair<int,int>)> reduce = [this, &reduce](pair<int, int> frac)->pair<int,int>{
            for(int i=2; i<frac.first/2; i++){
                if(frac.first%i==0 && frac.second%i==0){
                    if(frac.first*frac.second/(i*i)<pixels_per_image)
                        return frac;

                    frac.first /= i;
                    frac.second/= i;

                    return reduce(frac);
                }
            }
            return frac;

        };

        reduced = reduce(reduced);

        if(verbose) cout << endl << "Smaller resolution is: " << reduced.first << "x" << reduced.second << endl;

        // stretch image to fit into final matrix resolution
        Mat stretch = Mat(1,reduced.first*reduced.second, CV_16U);
        resize(samples, stretch, Size(reduced.first*reduced.second,1));

        final_mini_image = Mat::zeros(reduced.second, reduced.first, CV_8U);

        final_mini_image = stretch.reshape(0,reduced.second);

        // normalize frame to usigned char
        normalize(final_mini_image, final_mini_image, 0, 255, NORM_MINMAX, CV_8UC1);

        //return the size ratio
        return float(width)/float(reduced.first);
    }

    pair<int, int> frameStream::centerImage(Mat & image){
        pair<int,int> minimums;
        
        // define filters
        int xwindow = 100;
        Mat xfilter = Mat::ones(1,xwindow, CV_8U)/xwindow;
        xfilter = xfilter.mul(xfilter);
        
        int ywindow = 10;
        Mat yfilter = Mat::ones(1,ywindow, CV_8U)/ywindow;
        yfilter = yfilter.mul(yfilter);

        // compute averages
        Mat x_average, y_average; 
        reduce(image, x_average, 0, CV_REDUCE_AVG);
        reduce(image, y_average, 1, CV_REDUCE_AVG);
        
        // apply filters
        filter2D(x_average, x_average.clone(), -1, xfilter, Point(-1,-1), 0.0, BORDER_REPLICATE);
        filter2D(y_average, y_average.clone(), -1, yfilter, Point(-1,-1), 0.0, BORDER_REPLICATE);

        double min, max;
        Point min_location, max_location;

        minMaxLoc(x_average, &min, &max, &min_location, &max_location);
        minimums.first = min_location.x;

        minMaxLoc(y_average, &min, &max, &min_location, &max_location);
        minimums.second = min_location.y;

        return minimums;
    }

}

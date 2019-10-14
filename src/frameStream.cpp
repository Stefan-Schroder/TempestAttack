#include "frameStream.h"
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <fstream>
#include <iterator>
#include <climits>
#include <vector>
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
                        bool inverted,
                        bool interlaced,
                        bool verbose,
                        string dir_name):

                        width(width), height(height), refresh(refresh),
                        frequency(frequency),
                        frame_average(frame_average),
                        sample_rate(sample_rate),
                        verbose(verbose),
                        inverted(inverted),
                        interlaced(interlaced),
                        output_directory(dir_name){

        pixels_per_image = round(sample_rate/refresh);

        int sample_size = pixels_per_image*(frame_average+1); // +1 for extra frame
        if(verbose) cout << "Samples to read in: " << sample_size << endl;

        all_samples = Mat::zeros(1,sample_size, CV_16U);
        indices = vector<int>(frame_average);

        total_sample_count = pixels_per_image*frame_average;

    }

    double frameStream::getFrequency(){ return frequency; }
    Mat frameStream::getFinalImage(){ return final_image; }
    // ===================================================================================
    // =============================== LOADING DATA ======================================
    // ===================================================================================

    bool frameStream::loadDataFile(string filename, int frame_ignore){
        if(verbose) cout << filename << endl;
        //reading in file
        ifstream input_stream(filename.c_str(), ios::binary);

        input_stream.seekg(pixels_per_image*frame_ignore*2*2, std::ios::cur);

        short I_sample, Q_sample;
        int counter = 0;
        while(counter<pixels_per_image*(frame_average+1) &&  // + 1 is for the extra frame used in shifting
                input_stream.read((char *)&I_sample, sizeof(short)) &&
                input_stream.read((char *)&Q_sample, sizeof(short))){
            
            all_samples.at<short>(0,counter++) = sqrt(pow(I_sample,2)+pow(Q_sample,2));
            //all_samples.at<short>(0,counter++) = I_sample+Q_sample;//sqrt(pow(I_sample,2)+pow(Q_sample,2));

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
    pair<int, unsigned int> frameStream::processSamples(int shift_max){

        for(int i=0; i<frame_average; i++){
            indices[i] = pixels_per_image*i;

            // Save all frames
            if(verbose){
                Mat one_frame = makeMatrix(indices[i], all_samples);
                Mat stretch = Mat(1,width*height, CV_16U);
                resize(one_frame, stretch, Size(width*height,1));
                final_image = Mat::zeros(height, width, CV_8U);
                final_image = stretch.reshape(0,height);
                normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

                saveImage(to_string(i)+"-before_shift");
                final_image.release();
            }
        }

        //corrolate frames
        if(frame_average==1)
            return make_pair(0,1);
        else
            return mapMode(corrolateFrames(shift_max));
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
        int window = 1; // can filter before corrolating 
        Mat average_filter = Mat::ones(1,window, CV_32F)/window;
        average_filter = average_filter.mul(average_filter);

        
        Mat filtered_samples = Mat::ones(1, all_samples.cols, CV_32F);
        filter2D(all_samples, filtered_samples, -1, average_filter, Point(0,0), 5.0, BORDER_REFLECT);

        if(verbose) cout << "sizes of samples: " << all_samples.cols << ", " << filtered_samples.cols << endl;

        // ========Start correlation process==========
        unordered_map<int, unsigned int> shift_amount_map; // cant be ordered because constantly changing

        //calculate the best shifts (first frame does not shift)

        //uncompent both #pragma to enable omp threads for this
//#pragma omp parallel for
        for(int i=frame_average-1; i>=1; i--){

            // if the polarization of the reconstruction inverst sample the correlation should be inverted
            int inverstion_mult = (inverted) ? -1 : 1; 

            double highest_corr = -numeric_limits<double>::max();
            int best_shift = 0;

            for(int j=-shift_max; j<=shift_max; j++){
                Mat shifting_frame = makeMatrix(shiftIndex(indices[i],j), filtered_samples);
                double corr = inverstion_mult*correlation(shifting_frame, makeMatrix(indices[i-1], filtered_samples));
                if(corr > highest_corr){
                    highest_corr = corr;
                    best_shift = j;
                }
            }

//#pragma omp critical
            {

            shift_amount_map[best_shift]++;

            if(verbose){ // save frames after shifts
                Mat one_frame = makeMatrix(shiftIndex(indices[i],best_shift*i), all_samples);
                Mat stretch = Mat(1,width*height, CV_16U);
                resize(one_frame, stretch, Size(width*height,1));
                final_image = Mat::zeros(height, width, CV_8U);
                final_image = stretch.reshape(0,height);
                normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

                saveImage(to_string(i)+"-after_shift");
                final_image.release();
            }

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
        if(frame_average!=1){

            int total_shift = 0;
            for(int i=1; i<frame_average; i++){
                total_shift += shift_amount;

                indices[i] = shiftIndex(indices[i], total_shift);
            }

            //align frame
            if(verbose) cout << endl << "ppi before: " << pixels_per_image << endl;
            pixels_per_image = indices[1] - indices[0]; // SO BIG BRAIN
            if(verbose) cout << "ppi after: " << pixels_per_image << endl;
        }

        // average frames
        Mat one_frame = averageFrames(indices);

        float multiplier = writeMiniFrame(one_frame);

        //clear memory of all samples as it isnt needed
        all_samples.release();

        // stretch image to fit into final matrix resolution
        Mat stretch = Mat(1,width*height, CV_16U);
        resize(one_frame, stretch, Size(width*height,1)); // interpolates samples

        final_image = Mat::zeros(height, width, CV_8U);

        final_image = stretch.reshape(0,height); // reshapes array into 2D image

        // normalize frame to usigned char
        normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);

        if(interlaced){
            // compensate for interlaced display
            final_image = reconInterlace(final_image.clone()); 
            final_mini_image = reconInterlace(final_mini_image.clone()); 
        }

        if(verbose) saveImage("uncenterd_image-"+to_string(getFrequency()));

        // Center image
        pair<int, int> amount = centerImage(final_mini_image); // finds shifts to center mini frame

        shiftImage(final_image.clone(), final_image, -amount.first*multiplier, -amount.second*multiplier);

    }

    /**
     * Averages the frames from all_samples together to make the final array
     * The average array is made using vector of starting indices of each frame
     * 
     * returns a one dimentional stream of samples of display
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

    /**
     * Saves final_image, used for public access.
     */
    bool frameStream::saveImage(string filename){
        imwrite(output_directory+filename+".jpg", final_image);
        return true;
    }

    /**
     * Write a smaller version of the main frame, that contains the same information received,
     * but is not interpolated to as many pixels as the final_image.
     * Made to improve efficiency on further filtering and templating, as well as centering frame.
     *
     * returns factor to multiply with when converting cordinates from mini_image to final_image
     */
    float frameStream::writeMiniFrame(Mat & samples){
        //reduce the amount of pixels for miniframe
        pair<int, int> reduced(width, height);

        /**
         * Reduction lambda
         * Recursive method for finding the lowest command multiple of width and height
         * that is larger than current pixels per fram.
         */
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

        reduced = reduce(reduced); // wow, what a line

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
        //int xwindow = 50;
        int xwindow = 100;
        Mat xfilter = Mat::ones(1,xwindow, CV_8U)/xwindow;
        xfilter = xfilter.mul(xfilter);
        
        //int ywindow = 10;
        int ywindow = 20;
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
        minimums.first = (inverted) ? max_location.x : min_location.x;

        minMaxLoc(y_average, &min, &max, &min_location, &max_location);
        minimums.second = (inverted) ? max_location.y : min_location.y;

        return minimums;
    }

    /**
     * Combines the two half frames, now present in final_frame (due to half the refresh), into one.
     */
    Mat frameStream::reconInterlace(Mat interlaced){

        Mat reconstructed = Mat::zeros(height, width, CV_8U);

        int i_height = interlaced.rows;

        for(int n=0; n<i_height; n++){
            interlaced.row(floor(float(n)/2) + ceil(float(i_height)/2) * (n % 2)).copyTo(reconstructed.row(n));
        }

        return reconstructed;
    }

    // =================================================================================== 
    // ============================== FFT PROCESSORS =====================================
    // =================================================================================== 

    /*
     * Matlab code never worked well enough so didnt finish cpp implementation
    void frameStream::shiftFrequency(double amount){

        int full_pixel_size = width*height;

        //unwrap image
        Mat unwrap_int = final_image.reshape(0,1);
        Mat unwrap;
        unwrap_int.convertTo(unwrap, CV_32F); 
        unwrap_int.release();

        //dft array
        Mat real = Mat::zeros(full_pixel_size,0, CV_32F);
        //Mat imag = Mat::zeros(height*width,0, CV_32F);
        dft(unwrap,real);
        //dft(unwrap,imag,DFT_COMPLEX_OUTPUT,0);
        //dft(unwrap,real,,0);


        //half the array
        //shift array
        int freq_shift_amount = int(round(amount/refresh));
        cout << "shift amount" << endl;

        Mat half_shifted = Mat::zeros(full_pixel_size/2,0,CV_32F);
        real.rowRange(0,full_pixel_size/2-freq_shift_amount).copyTo(half_shifted.rowRange(freq_shift_amount, full_pixel_size/2));
        
        //image_in.colRange(0,x).copyTo(image_out.colRange(image_out.cols-x, image_out.cols));

        //recombine array
        //Mat recombine = 
        //idf array
        //reshape image


        //final_image = stretch.reshape(0,height);

    }
    */

}

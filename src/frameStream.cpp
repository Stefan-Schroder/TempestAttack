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

        pixels_per_image = round(sample_rate/refresh);

        int sample_size = pixels_per_image*(frame_average+1); // +1 for extra frame
        if(verbose) cout << "Samples to read in: " << sample_size << endl;

        all_samples = Mat::zeros(1,sample_size, CV_16U);
        final_image = Mat::zeros(height, width, CV_8U);

        total_sample_count = pixels_per_image*frame_average;
    }

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

    /**
     * Main processing of images done here.
     * All of the samples, captured either by receiver of input file are processed into one image.
     * This single image is the average of the average_amount of images given by the user.
     * The images are aligned then processed
     */
    void frameStream::processSamples(int shift_max){

        vector<int> frame_starts(frame_average);
        for(int i=0; i<frame_average; i++){
            frame_starts[i] = pixels_per_image*i;
        }

        //corrolate frames
        corrolateFrames(frame_starts, shift_max);

        /* plot all frames
        for(int i=0; i<frame_average; i++){
            Mat one_frame = makeMatrix(frame_starts[i]);
            if(one_frame.empty()) break;

            Mat stretch = Mat(1,width*height, CV_16U);
            resize(one_frame, stretch, Size(width*height,1));

            final_image = stretch.reshape(0,height);

            // normalize frame to usigned char
            normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);
            saveImage(to_string(i)+"-frame");
        }
        */



        Mat one_frame = averageFrames(frame_starts);
        // stretch image to fit into final matrix resolution
        Mat stretch = Mat(1,width*height, CV_16U);
        resize(one_frame, stretch, Size(width*height,1));

        final_image = stretch.reshape(0,height);

        shiftImage(final_image.clone(), final_image, 20, -30);

        // normalize frame to usigned char
        normalize(final_image, final_image, 0, 255, NORM_MINMAX, CV_8UC1);
    }

    /**
     * Uses all_samples and pixels_per_image, make use the indexes are valid
     */
    Mat frameStream::makeMatrix(int start_index){
        if(start_index<0 || start_index+pixels_per_image>=all_samples.cols){
            cerr << "Index's are out of range" << endl;
            cerr << "Your range goes from:" << endl;
            cerr << start_index << "->" << start_index+pixels_per_image << endl;
            cerr << "All samples range goes from:" << endl;
            cerr << 0 << "->" << all_samples.cols << endl;

            Mat empty;
            return empty;
        }
        else {
            return all_samples.colRange(start_index, start_index+pixels_per_image);
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
     * !!NOTE: pixels_per_image CHANGES AFTER THIS IS RUN (straigtens image out)
     */
    void frameStream::corrolateFrames(vector<int> & indices, int shift_max){
        vector<int> shift_amount(frame_average);
        shift_amount[0] = 0;

        //calculate the best shifts (first frame does not shift)
        if(verbose) cout << endl << "Frame:\tShifted" << endl;
        for(int i=frame_average-1; i>=1; i--){

            double highest_corr = 0;
            int best_shift = 0;

            for(int j=-shift_max; j<=shift_max; j++){
                Mat shifting_frame = makeMatrix(shiftIndex(indices[i],j));
                double corr = corrolation(shifting_frame, makeMatrix(indices[i-1]));
                if(corr > highest_corr){
                    highest_corr = corr;
                    best_shift = j;
                }
            }

            shift_amount[i] = best_shift;
            if(verbose) cout << i << "\t" << best_shift << endl;
        }

        int total_shift = 0;
        for(int i=1; i<frame_average; i++){
            total_shift += shift_amount[i];

            indices[i] += total_shift;
        }

        if(verbose) cout << endl << "ppi before: " << pixels_per_image << endl;
        pixels_per_image = indices[1] - indices[0]; // SO BIG BRAIN
        if(verbose) cout << "ppi after: " << pixels_per_image << endl;
    }

    Mat frameStream::averageFrames(std::vector<int> & indices){
        Mat sum_frames = Mat::zeros(1, pixels_per_image, CV_32F); //larger size to handle summantion

        auto begin = indices.begin(), end = indices.end();
        while(begin!=end){
            Mat frame = makeMatrix(*begin++);
            frame.convertTo(frame,CV_32F);
            sum_frames += frame/frame_average;
        }
        sum_frames = sum_frames/frame_average;

        return sum_frames;
    }

    bool frameStream::saveImage(string filename){
        imwrite("data/"+filename+"-"+to_string(frequency)+"-image.jpg", final_image);
        return true;
    }


}

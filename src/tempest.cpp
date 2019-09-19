#include "tempest.h"
#include <uhd/usrp/multi_usrp.hpp>
#include "frameStream.h"
#include <thread>

using namespace std;

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

    void tempest::process(){
        bands = vector<tmpst::frameStream>(bandwidth_multiples);

        if(verbose) cout << endl << "Adding new recordings with base band: " << endl;
        for(int i=0; i<bandwidth_multiples; i++){
            double band_center = base_center_freq+i*sample_rate*bandwidth_overlap;
            if(verbose) cout << "\t" << band_center << endl;

            tmpst::frameStream newFrame(width, height, refresh,
                                        band_center,
                                        frame_av_num, sample_rate, verbose);

            bands[i] = newFrame;
        }

    }

    void tempest::loadData(){
        auto begin = bands.begin(), end = bands.end();

        while(begin!=end){
            if(from_file){
                begin->loadDataFile(input_file, frame_ignore);
                begin->processSamples(max_shift);
                begin->saveImage(name);

            } else {
                begin->loadDataRx(usrp, offset, channel, frame_ignore);
                begin->processSamples(max_shift);
                begin->saveImage(name);

            }


            begin++;
        }
    }


}

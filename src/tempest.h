#ifndef _TEMPEST_H_
#define _TEMPEST_H_
#include <uhd/usrp/multi_usrp.hpp>
#include <vector>
#include "frameStream.h"

namespace tmpst{
    class tempest{
    private:
        //user dependant
        std::string name;                       //name of everything to be outputted
        uhd::usrp::multi_usrp::sptr usrp;       //receiver device interface
        int width, height;                      //resolution of the monitor
        double refresh;                         //refresh rate of the monitor
        int bandwidth_multiples;                //multiple of bandwidth bins to be added together
        double bandwidth_overlap;               //percentage overlap of the bandwidths
        int frame_av_num;                       //number of frames to be averaged together
        double base_center_freq, sample_rate;   //base center frequ
        double offset;
        size_t channel;
        int frame_ignore;
        int max_shift;                          // maximum amount the frames can shift to align

        bool verbose;

        //from file
        std::string input_file;

        //calculated
        bool from_file = false;
        double full_spectrum_size;

        std::vector<tmpst::frameStream> bands;


    public:
        tempest(uhd::usrp::multi_usrp::sptr usrp,
                std::string name,
                int width, int height, double refresh,
                int bndwidth_mult, int frame_av, double overlap,
                double center_frequ, double sample_rate,
                double offset,
                size_t channel,
                int frame_ignore,
                int max_shift,
                bool verbose);

        tempest(std::string input_file,
                std::string name,
                int width, int height, double refresh,
                int frame_av,
                double sample_rate,
                int frame_ignore,
                int max_shift,
                bool verbose);

        void process();

        void loadData();


    };
}
#endif

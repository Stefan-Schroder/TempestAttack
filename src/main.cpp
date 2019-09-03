#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>

namespace ops = boost::program_options;

int UHD_SAFE_MAIN(int argc, char * argv[]){
    uhd::set_thread_priority_safe();

    // Inputs
    std::string addr, file, type, ant, subdev, ref;
    size_t channel;
    double rate, freq, gain, bw, total_time, lo_offset;

    // Handeling commandline ops
    ops::options_description desc("Available Options");
    desc.add_options()
        ("help",                                                                                    "help message")
        ("addr",        ops::value<std::string>(&addr)->        default_value("192.168.10.2"),      "address for the receiver")
        ("file",        ops::value<std::string>(&file)->        default_value("usrp_samples.dat"),  "name of the file to write binary samples to")
        ("type",        ops::value<std::string>(&type)->        default_value("short"),             "sample type: double, float, or short")
        ("duration",    ops::value<double>(&total_time)->       default_value(0),                   "total number of seconds to receive")
        ("rate",        ops::value<double>(&rate)->             default_value(25e6),                "rate of incoming samples")
        ("freq",        ops::value<double>(&freq)->             default_value(1288000000),          "RF center frequency in Hz")
        ("lo-offset",   ops::value<double>(&lo_offset)->        default_value(0.0),                 "Offset for frontend LO in Hz (optional)")
        ("gain",        ops::value<double>(&gain)->             default_value(10),                  "gain for the RF chain")
        ("ant",         ops::value<std::string>(&ant)->         default_value("TX/RX"),             "antenna selection")
        ("subdev",      ops::value<std::string>(&subdev),                                           "subdevice specification")
        ("channel",     ops::value<size_t>(&channel)->          default_value(0),                   "which channel to use")
        ("bw",          ops::value<double>(&bw),                                                    "analog frontend filter bandwidth in Hz")
        ("ref",         ops::value<std::string>(&ref)->         default_value("internal"),          "reference source (internal, external, mimo)")
    ;

        // clang-format on
    ops::variables_map varMap;
    ops::store(ops::parse_command_line(argc, argv, desc), varMap);
    ops::notify(varMap);

    // print the help message
    if (varMap.count("help")) {
        std::cout << boost::format("UHD RX samples to file %s") % desc << std::endl;
        std::cout << std::endl
                  << "This application streams data from a single channel of a USRP "
                     "device to a file.\n"
                  << std::endl;
        return ~0;
    }

    //create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % addr << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(addr); // setting the IP address

    // Lock mboard clocks
    std::cout << boost::format("Lock mboard clocks: %f") % ref << std::endl;
    usrp->set_clock_source(ref); // receivers reference clock

    //always select the subdevice first, the channel mapping affects the other settings
    std::cout << boost::format("subdev set to: %f") % subdev << std::endl;
    usrp->set_rx_subdev_spec(subdev); // setting sub device
    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    //set the sample rate
    if (rate <= 0.0) {
        std::cerr << "Please specify a valid sample rate" << std::endl;
        return ~0;
    }

    // set sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate / 1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;

    // set freq
    std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
    uhd::tune_request_t tune_request(freq);
    usrp->set_rx_freq(tune_request);
    std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq() / 1e6) << std::endl << std::endl;

    // set the rf gain
    std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
    usrp->set_rx_gain(gain);
    std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

    // set the IF filter bandwidth
    std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw / 1e6) << std::endl;
    usrp->set_rx_bandwidth(bw);
    std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;

    // set the antenna
    std::cout << boost::format("Setting RX Antenna: %s") % ant << std::endl;
    usrp->set_rx_antenna(ant);
    std::cout << boost::format("Actual RX Antenna: %s") % usrp->get_rx_antenna() << std::endl << std::endl;


    return 0;
}

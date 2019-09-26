//UHD
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
//Program options
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
// Streams
#include <iostream>
#include <fstream>
//create a new file
#include <bits/stdc++.h>
#include <sys/stat.h> 
#include <sys/types.h>
// internal
#include "tempest.h"
#include "resconvert.h"
// misc
#include <thread>
#include <string>

namespace ops = boost::program_options;
using namespace std;

// global variables
bool verbose = false;

/**
 * checks if the sensor has been locked. Code taken from Ettus example script
 */
typedef std::function<uhd::sensor_value_t(const std::string&)> get_sensor_fn_t;
bool check_locked_sensor(   std::vector<std::string> sensor_names,
                            const char*              sensor_name,
                            get_sensor_fn_t          get_sensor_fn,
                            double                   setup_time) {

    if (std::find(sensor_names.begin(), sensor_names.end(), sensor_name)
        == sensor_names.end())
        return false;

    auto setup_timeout = std::chrono::steady_clock::now()
                         + std::chrono::milliseconds(int64_t(setup_time * 1000));
    bool lock_detected = false;

    if(verbose) std::cout << boost::format("Waiting for \"%s\": ") % sensor_name;
    if(verbose) std::cout.flush();

    while (true) {
        if (lock_detected and (std::chrono::steady_clock::now() > setup_timeout)) {
            if(verbose) std::cout << " locked." << std::endl;
            break;
        }
        if (get_sensor_fn(sensor_name).to_bool()) {
            if(verbose) std::cout << "+";
            if(verbose) std::cout.flush();
            lock_detected = true;
        } else {
            if (std::chrono::steady_clock::now() > setup_timeout) {
                if(verbose) std::cout << std::endl;
                throw std::runtime_error(
                    str(boost::format(
                            "timed out waiting for consecutive locks on sensor \"%s\"")
                        % sensor_name));
            }
            if(verbose) std::cout << "_";
            if(verbose) std::cout.flush();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(verbose) std::cout << std::endl;
    return true;
}

/**
 * Main interface code.
 * This is where the program starts, and all command line arguments are handled.
 */
int UHD_SAFE_MAIN(int argc, char * argv[]){
    tmpst::tempest *main_tempest;

    uhd::set_thread_priority_safe();

    // Inputs
    string addr, folder, ant, subdev, ref, res_string, input_file, config_file;
    size_t channel;
    double rate, freq, gain, bw, lo_offset, refresh, setup_time, overlap;
    int multi, average_amount, width, height, frame_ignore, shift_max;
    bool exact_resolution = false;

    // Handeling commandline ops
    ops::options_description desc("Available Options");
    desc.add_options()
        ("help",                                                                                    "help message")
        ("conf_file",   ops::value<std::string>(&config_file)-> default_value("config.ini"),        "Alternative config file")
        ("addr",        ops::value<std::string>(&addr)->        default_value("192.168.10.2"),      "address for the receiver")
        ("folder",      ops::value<std::string>(&folder)->      default_value("data/"),             "name of the folder where all information is dumped to")
        ("res",         ops::value<std::string>(&res_string)->  default_value("1024x768"),          "resolution of the device you want to attack")
        ("refresh",     ops::value<double>(&refresh)->          default_value(75.024),              "refresh rate of the monitor you wish to attack")
        ("average",     ops::value<int>(&average_amount)->      default_value(2),                   "amount of frames to average together")
        ("rate",        ops::value<double>(&rate)->             default_value(25e6),                "rate of incoming samples")
        ("freq",        ops::value<double>(&freq)->             default_value(1288000000),          "RF center frequency in Hz of the lowest multiple freq")
        ("lo-offset",   ops::value<double>(&lo_offset)->        default_value(0.0),                 "offset for frontend LO in Hz (optional)")
        ("gain",        ops::value<double>(&gain)->             default_value(30),                  "gain for the RF chain")
        ("ant",         ops::value<std::string>(&ant)->         default_value("TX/RX"),             "antenna selection")
        ("subdev",      ops::value<std::string>(&subdev),                                           "subdevice specification")
        ("channel",     ops::value<size_t>(&channel)->          default_value(0),                   "which channel to use")
        ("bw",          ops::value<double>(&bw),                                                    "analog frontend filter bandwidth in Hz")
        ("ref",         ops::value<std::string>(&ref)->         default_value("internal"),          "reference source (internal, external, mimo)")
        ("setup",       ops::value<double>(&setup_time)->       default_value(1.0),                 "seconds of setup time")
        ("multi",       ops::value<int>(&multi)->               default_value(1),                   "multiple of the amount of bandwidths you want to combine (0 for auto calculation)")
        ("overlap",     ops::value<double>(&overlap)->          default_value(0.5),                 "overlap between the sub-bands as a percentage (0.9 mean 90% of band A and B are the same)")
        ("input",       ops::value<std::string>(&input_file),                                       "filename of raw short IQ samples, used instead of receiver")
        ("ignore",      ops::value<int>(&frame_ignore)->        default_value(0),                   "specify how many frames to ignore from the received data (can help in certain cases)")
        ("max_shift",   ops::value<int>(&shift_max)->           default_value(200),                 "maximum amount each frame can shift to align each other (higher amount make it slower)")
        ("v",                                                                                       "print all information")
        ("x",                                                                                       "Use the unconverted resolution entered")
    ;

    ops::variables_map var_map;
    // pass command line arguments
    ops::store(ops::parse_command_line(argc, argv, desc), var_map);
    // pass config file options
    ifstream conf_file("config.ini");
    ops::store(ops::parse_config_file(conf_file, desc), var_map);
    ops::notify(var_map);

    // print the help message
    if (var_map.count("help")) {
        std::cout << boost::format("Command line Tempest Attack %s") % desc << std::endl;
        std::cout << std::endl
                  << "Application description."
                  << std::endl;
        return ~0;
    }
    

    // ============================  Set variables ================================

    verbose = var_map.count("v") > 0;
    exact_resolution = var_map.count("x");


    // string resolution to int
    if(!exact_resolution){
        height = tmpst::getHeight(res_string,refresh);
        width = tmpst::getWidth(res_string,refresh);
        if(height == 0 || width == 0){
            cerr << "Resolution " << res_string << " does not exist. If you are sure this is the resolution enable --x (exact resolution)" << endl;
            return -1;
        }
    }else {
        try{
            width = stoi(res_string.substr(0,res_string.find('x')));
            height = stoi(res_string.substr(res_string.find('x')+1));
        }catch(exception& e){
            cerr << "The width and height need to be written as --res=1234x321" << endl;
            return -1;
        }
    }
    if(verbose) cout << "width: " << width << " height: " << height << endl;

    // ============ no input file ===================
    if(input_file.empty()){
        //create a usrp device
        if(verbose) std::cout << boost::format("Creating the usrp device with: %s...") % addr << std::endl;
        uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make("--addr=\""+addr+"\""); // setting the IP address


        // Lock mboard clocks
        if(verbose) std::cout << boost::format("Lock mboard clocks: %f") % ref << std::endl;
        usrp->set_clock_source(ref); // receivers reference clock

        //always select the subdevice first, the channel mapping affects the other settings
        if(var_map.count("subdev")){
            if(verbose) std::cout << boost::format("subdev set to: %f") % subdev << std::endl;
            usrp->set_rx_subdev_spec(subdev); // setting sub device
            if(verbose) std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;
        }

        //set the sample rate
        if (rate <= 0.0) {
            std::cerr << "Please specify a valid sample rate" << std::endl;
            return ~0;
        }

        // set sample rate
        if(verbose) std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate / 1e6) << std::endl;
        usrp->set_rx_rate(rate);
        if(verbose) std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;

        // set freq
        if (var_map.count("freq")) { // with default of 0.0 this will always be true
            if(verbose) std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
            if(verbose) std::cout << boost::format("Setting RX LO Offset: %f MHz...") % (lo_offset / 1e6) << std::endl;

            uhd::tune_request_t tune_request(freq, lo_offset);

            usrp->set_rx_freq(tune_request, channel);
            if(verbose) std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq(channel) / 1e6) << std::endl;
        }

        // set the rf gain
        if(verbose) std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
        usrp->set_rx_gain(gain);
        if(verbose) std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

        // set the IF filter bandwidth
        if(var_map.count("bw")){
            if(verbose) std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw / 1e6) << std::endl;
            usrp->set_rx_bandwidth(bw);
            if(verbose) std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;
        }

        // set the antenna
        if(verbose) std::cout << boost::format("Setting RX Antenna: %s") % ant << std::endl;
        usrp->set_rx_antenna(ant);
        if(verbose) std::cout << boost::format("Actual RX Antenna: %s") % usrp->get_rx_antenna() << std::endl << std::endl;

        // start setup
        std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(1000 * setup_time)));

        // check Ref and LO Lock detect
        check_locked_sensor(usrp->get_rx_sensor_names(channel),
            "lo_locked",
            [usrp, channel](const std::string& sensor_name) {
                return usrp->get_rx_sensor(sensor_name, channel);
            },
            setup_time);
        if (ref == "mimo") {
            check_locked_sensor(usrp->get_mboard_sensor_names(0),
                "mimo_locked",
                [usrp](const std::string& sensor_name) {
                    return usrp->get_mboard_sensor(sensor_name);
                },
                setup_time);
        }
        if (ref == "external") {
            check_locked_sensor(usrp->get_mboard_sensor_names(0),
                "ref_locked",
                [usrp](const std::string& sensor_name) {
                    return usrp->get_mboard_sensor(sensor_name);
                },
                setup_time);
        }

        // Transmit data to be processed
        main_tempest = new tmpst::tempest(usrp, folder, width, height, refresh, multi, average_amount, overlap, freq, rate, lo_offset, channel ,frame_ignore, shift_max, verbose); 


    }else{
        // Reading in a file
        main_tempest = new tmpst::tempest(input_file, folder, width, height, refresh, average_amount, rate, frame_ignore, shift_max, verbose);

    }

    main_tempest->initializeBands();
    main_tempest->processBands();
    main_tempest->combineBands();

    delete main_tempest;


    return 0;
}

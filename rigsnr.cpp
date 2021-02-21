// Copyright 2021 Jeff Aigner
// MIT License

#include <chrono>
#include <iostream>
#include <math.h>
#include <thread>
#include <mutex>
#include <conio.h>
#include <vector>
#include <algorithm>

#include "hamlib/rig.h"

std::string SERIAL_PORT = "COM3";
auto BAUD_RATE = 4800;
auto MODEL = RIG_MODEL_IC7300;

std::mutex snr_lock;
double snr = 0, dnr = 0;
double h = 1, l = 200;

bool still_alive = true;

// fuction pointer for the rig_list_foreach function
std::vector<std::pair<int, std::string>> models_vector;
int hash_model_list(const rig_caps* rig, void* data) {
    char buf[256];
    snprintf(buf, 256, "%6d   %-23s%-24s", rig->rig_model, rig->mfg_name, rig->model_name);
    models_vector.push_back(std::pair<int, std::string>(int(rig->rig_model), std::string(buf)));
    return 1;
}

// comparitor function for sorting models
bool cmp(std::pair<int, std::string> a, std::pair<int, std::string> b) {
    return a.first < b.first;
}

// print all models available in hamlib
void print_all_models() {
    rig_load_all_backends();
    rig_list_foreach(hash_model_list, NULL);
    std::sort(models_vector.begin(), models_vector.end(), cmp);
    for (auto& it : models_vector) {
        std::cout << it.second << std::endl;
    }
}

// Main input loop
void input_handler() {
    int key;
    while (still_alive) {
        key = _getch();
        snr_lock.lock();
        h = 1, l = 200;

        // User pressed enter, save input
        if (key == 13) {
            std::cout << std::endl;
        }

        // User pressed ctrl+C, Exit
        if (key == 3) {
            // we must die now
            std::cout << std::endl;
            still_alive = false;
        }
        snr_lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void print_help() {
    std::cout << "rigsnr help                                                        " << std::endl;
    std::cout << "-m, --model=ID           Radio model (see -l for radio model IDS)  " << std::endl;
    std::cout << "-l, --list               List all models                           " << std::endl;
    std::cout << "-r, --rig-file=DEVICE    Set device of the radio to operate on     " << std::endl;
    std::cout << "-s, --serial-speed       Set the BAUD rate of the serial connection" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char *argv[])
{
    int retcode, strength;
    
    // disable debug messaging
    rig_set_debug(RIG_DEBUG_NONE);

    if (argc < 2) {
        print_help();
        return 0;
    }

    // parse arguments
    for (int i = 0; i < argc; ++i) {
        std::string flag(argv[i]);
        if (flag == "-l") {
            print_all_models();
            return 0;
        }
        else if (flag == "-h") {
            print_help();
            return 0;
        }
        else if (flag == "-s" || flag == "--serial-speed") {
            BAUD_RATE = atoi(argv[++i]);
        }
        else if (flag == "-m" || flag == "--model") {
            MODEL = atoi(argv[++i]);
        }
        else if (flag == "-r" || flag == "--rig-file") {
            SERIAL_PORT = std::string(argv[++i]);
        }
    }

    // start keyboard capture thread
    std::thread t(input_handler);

    auto my_rig = rig_init(MODEL);
    if (!my_rig)
    {
        fprintf(stderr, "rig_init: could not init rig (wrong rig selection?)\n");
        exit(1);
    }

    my_rig->state.rigport.type.rig = RIG_PORT_SERIAL;
    my_rig->state.rigport.parm.serial.rate = BAUD_RATE;

    strncpy_s(my_rig->state.rigport.pathname, SERIAL_PORT.c_str(), FILPATHLEN - 1);
    retcode = rig_open(my_rig);
    if (retcode != RIG_OK)
    {
        std::cout << "rig_open: could not open rig (" << retcode << ")" << std::endl;
        exit(2);
    }

    while (still_alive) {
        // get a reading from the S-meter
        // if it fails, just wait 20ms and continue
        retcode = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);
        if (retcode != RIG_OK)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        // normalize so that s0 = 1
        // this way we don't get an SNR of infinity
        // or negative SNRs
        // (hamlib normalizes s9 to 0)
        strength = strength + 55;

        snr_lock.lock();
        // set our high and low value
        h = strength > h ? strength : h;
        l = strength < l ? strength : l;

        // do our calculations
        snr = 10.0 * log(abs(h/l));
        dnr = h - l;
        snr_lock.unlock();

        // output and delete
        char output[256];
        snprintf(output, 256, "SNR: %10f DNR: %f", snr, dnr);
        std::string o(output);
        std::cout << o << std::string(o.length(), '\b');
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // join thread
    t.join();
    return 0;
}


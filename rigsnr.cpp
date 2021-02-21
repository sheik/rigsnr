// Copyright 2021 Jeff Aigner
// MIT License

#include <chrono>
#include <iostream>
#include <math.h>
#include <thread>
#include <mutex>
#include <conio.h>

#include "hamlib/rig.h"

constexpr auto SERIAL_PORT = "COM3";
constexpr auto BAUD_RATE = 4800;
constexpr auto MODEL = RIG_MODEL_IC7300;

std::mutex snr_lock;
double snr = 0, dnr = 0;
double h = 1, l = 200;

bool still_alive = true;

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

int main()
{
    int retcode, strength;

    // start keyboard capture thread
    std::thread t(input_handler);

    // disable debug messaging
    rig_set_debug(RIG_DEBUG_NONE);

    auto my_rig = rig_init(MODEL);
    if (!my_rig)
    {
        fprintf(stderr, "Unable to init rig (wrong rig selection?)\n");
        exit(1);
    }

    my_rig->state.rigport.type.rig = RIG_PORT_SERIAL;
    my_rig->state.rigport.parm.serial.rate = BAUD_RATE;

    strncpy_s(my_rig->state.rigport.pathname, SERIAL_PORT, FILPATHLEN - 1);
    retcode = rig_open(my_rig);
    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
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

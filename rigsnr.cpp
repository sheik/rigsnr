// Copyright 2021 Jeff Aigner
// MIT License

#include <chrono>
#include <iostream>
#include <math.h>
#include <thread>

#include "hamlib/rig.h"

constexpr auto SERIAL_PORT = "COM3";
constexpr auto BAUD_RATE = 4800;
constexpr auto ALPHA = 0.1;
constexpr auto MODEL = RIG_MODEL_IC7300;

int main()
{
    RIG* my_rig;
    hamlib_port_t myport;
    rig_model_t myrig_model;
    int retcode, strength;
    double snr, dnr, h = 1, l = 100;

    // disable debug messaging
    rig_set_debug(RIG_DEBUG_NONE);

    my_rig = rig_init(MODEL);
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

    while (1) {
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

        // set our high and low value
        h = strength > h ? strength : h;
        l = strength < l ? strength : l;

        // do our calculations
        snr = 10.0 * log(abs(h/l));
        dnr = h - l;

        // output
        std::cout << "SNR: " << snr << "dB" << std::endl;
        std::cout << "DNR: " << dnr << "dB" << std::endl;
    }
}

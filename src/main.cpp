/*
 * Copyright (c) 2024 by Bert Laverman. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include <raspberry-pi.hpp>

#include <interfaces/pigpiod-spi.hpp>
#include <interfaces/spidev-spi.hpp>
#include <devices/local-max7219.hpp>

#include <util/ini-state.hpp>
#include <util/max7219-state.hpp>


#if !defined(HAVE_SPI)
#error "This example needs SPI enabled"
#endif

using namespace nl::rakis::raspberrypi;

static util::IniState config;


static const std::string optVerbose("-v");

static const std::string cmdReset("reset");
static const std::string cmdSet("set");
static const std::string cmdBrightness("brightness");
static const std::string cmdSync("sync");
static const std::string cmdClear("clear");
static const std::string cmdOn("on");
static const std::string cmdOff("off");

inline static std::string moduleName(int num) {
    return "display:" + std::to_string(num+1);
}

int main(int argc, char *argv[])
{
    std::string prog{ argv[0] };
    bool verbose{ false };
    if ((argc > 1) && (optVerbose == argv[1])) {
        verbose = true;
        --argc;
        ++argv;
    }

    config.verbose(verbose);
    config.load();

    if (verbose) { std::cerr << "Starting up.\n"; }
    RaspberryPi& berry{ RaspberryPi::instance() };
    auto spi = berry.addSPI<interfaces::PigpiodSPI>("spi0_0");
    // auto spi = berry.addSPI<interfaces::SPIDevSPI>("spi0_0", "/dev/spidev0.0");
    spi->verbose(verbose);
    spi->baudRate(500000);

    auto max = std::make_shared<devices::LocalMAX7219>();
    spi->device(max);

    config.log("Checking for spi-0 number of modules.");
    if (config.has("interface:spi-0", "modules")) {
        auto count = atoi(config["interface:spi-0"]["modules"].c_str());
        if (verbose) { std::cerr << "Setting number of modules to " << count << "\n"; }
        max->numDevices(count);
    }

    config.log("Telling MAX not to write immediately.");
    max->writeImmediately(false);

    config.log("Loading device-specific state.");
    for (uint8_t i = 0; i < max->numDevices(); ++i) {
        std::string name{ moduleName(i) };
        if (config.has(name)) {
            config.log(std::format("Loading state for '{}'", name));
            loadState(config[name], *max, i);
        }
    }

    config.log("Cleaning displays.");
    max->setClean();

    config.log("Checking what we need to do.");
    if (argc > 1) {
        if (cmdReset == argv[1]) {
            max->reset();
        } else if (cmdSync == argv[1]) {
            max->setDirty();
        } else if (cmdSet == argv[1]) {
            if (argc == 4) {
                try {
                    auto num{ atoi(argv[2])-1 };
                    auto name{ moduleName(num) };
                    auto value{ atoi(argv[3]) };

                    if ((num < 0) || (static_cast<unsigned>(num) >= max->numDevices())) {
                        std::cerr << "Invalid module number: " << num << "\n";
                        return -1;
                    }
                    if (verbose) { std::cerr << "Setting value for '" << name << "' (module " << num << ") to " << value << "\n"; }

                    max->setNumber(num, value);

                    if (verbose) { std::cerr << "Saving state for '" << name << "'\n"; }
                    devices::saveState(config[name], *max, num);
                    config.markDirty();
                    config.save();
                } catch(const std::exception& e) {
                    std::cerr << "Error: " << e.what() << "\n";
                    return -1;
                }
            } else {
                std::cerr << "Usage: " << prog << " set <module> <number>\n";
            }
        } else if (cmdClear == argv[1]) {
            if (argc == 3) {
                try {
                    auto num{ atoi(argv[2])-1 };
                    auto name{ moduleName(num) };

                    if ((num < 0) || (static_cast<unsigned>(num) >= max->numDevices())) {
                        std::cerr << "Invalid module number: " << num << "\n";
                        return -1;
                    }
                    if (verbose) { std::cerr << "Clearing '" << name << "' (module " << num << ")\n"; }

                    max->clear(num);
                    if (verbose) { std::cerr << "Value is now " << max->getValue(num) << ", with haveValue set to " << max->hasValue(num) << "\n"; }

                    if (verbose) { std::cerr << "Saving state for '" << name << "'\n"; }
                    devices::saveState(config[name], *max, num);
                    config.markDirty();
                    config.save();
                } catch(const std::exception& e) {
                    std::cerr << "Error: " << e.what() << "\n";
                    return -1;
                }
            } else {
                std::cerr << "Usage: " << prog << " clear <module>\n";
            }
        } else if (cmdBrightness == argv[1]) {
            if (argc == 4) {
                try {
                    auto num{ atoi(argv[2])-1 };
                    auto name{ moduleName(num) };
                    auto value{ atoi(argv[3]) };

                    if ((num < 0) || (static_cast<unsigned>(num) >= max->numDevices())) {
                        std::cerr << "Invalid module number: " << num << "\n";
                        return -1;
                    }
                    if ((value < 0) || (value > 15)) {
                        std::cerr << "Brightness value out of range: " << value << "\n";
                        return -1;
                    }
                    if (verbose) { std::cerr << "Setting brightness for '" << name << "' (module " << num << ") to " << value << "\n"; }

                    max->setBrightness(num, value);

                    if (verbose) { std::cerr << "Saving state for '" << name << "'\n"; }
                    devices::saveState(config[name], *max, num);
                    config.markDirty();
                    config.save();
                } catch(const std::exception& e) {
                    std::cerr << "Error: " << e.what() << "\n";
                    return -1;
                }
            } else {
                std::cerr << "Usage: " << prog << " brightness <module> <number>\n";
            }
        } else {
            std::cerr << "Unknown command '" << argv[1] << "'\n";
        }
    }
    config.log("Sending data to device.");
    max->sendData();

    config.log("Done.");
}
# Cup-Thor

### Team - Ariton Cosmin, Vîlceanu Sonia, Mărilă Mircea, Plămădeală Cristian, Bujor Marian, Vasu Ionuț

## The application has the following functionalities:

- temperature adjustment
- ambient light
- ventilation
- fire-alarm with build-in water jet
- option to schedule an hour for the oven to start automatically
- option to keep food warm for long after the cooking process has finished
- presets for different receipes
- smoke sensor with alarm
- set timer + alarm
- silent mode
- build-in camera
- media player

## Prerequisites for installation
Build tested on Ubuntu Server. Pistache doesn't support Windows, but you can use something like WSL or a virtual machine with Linux.

You will need to have a C++ compiler. I used g++ that came preinstalled. Check using g++ -v

You will need to install the Pistache library. On Ubuntu, you can install a pre-built binary as described here.

## Building
Using Make
You can build the cupThor executable by running make.

# Manually
A step by step series of examples that tell you how to get a development env running

You should open the terminal, navigate into the root folder of this repository, and run
g++ cupThor.cpp -o cupThor -lpistache -lcrypto -lssl -lpthread

This will compile the project using g++, into an executable called cupThor using the libraries pistache, crypto, ssl, pthread. You only really want pistache, but the last three are dependencies of the former. Note that in this compilation process, the order of the libraries is important.

# Running
To start the server run
./microwave

Your server should display the number of cores being used and no errors.

To test, open up another terminal, and type
curl http://localhost:9080/ready

Number 1 should display.

Now you have the server running

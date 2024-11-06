# Heat Illness Prevention System for Construction Sites

**WE ARE SYSTEM PROGRAMMING GROUP 4 _우당탕탕!**


## Overview

This project aims to create a comprehensive system for preventing heat-related illnesses at construction sites. The system collects temperature and humidity data using a DHT11 sensor and light intensity data using a photoresistor, all connected to a Raspberry Pi. This data is then sent to a remote server for continuous monitoring and to alert construction workers about potential heat hazards. Additionally, the system includes a mechanism to stop alerts once workers enter a designated area, ensuring that the presence of workers is monitored and alerts are correctly managed.

## Requirements

### Hardware

1. **Raspberry Pi**
    - Raspberry Pi *4
        - RPi 1: for managing environmental data (connect to DHT11)
        - RPi 2: for managing environmental data (Photoresistor)
        - RPi 3: for detecting the workers (connect to PIR, Servo Motor)
        - RPi 4: for processing alerts (connect to LED, Piezo Buzzer)

2. **Sensors**
    - DHT11 Temperature and Humidity Sensor
    - Photoresistor (Light Sensor)
    - PIR Sensor

3. **Actuators**
    - LED
    - Piezo Buzzer 
    - Servo Motor

4. **Others**
    - Breadboard and jumper wires

### Software

- Raspbian OS
- wiringPi library
- GCC compiler

## Workflow

![Workflow](https://git.ajou.ac.kr/luc010302/systemprogramming-4/-/raw/main/image/workflow.png)



## Installation

### 1. Install wiringPi

First, install the wiringPi library on each Raspberry Pi:
```bash
sudo apt-get update
sudo apt-get install wiringpi
```

### 2. Download the Project Code

Clone the project repository from Gitlab and navigate to the project directory:
```bash 
git clone https://git.ajou.ac.kr/luc010302/systemprogramming-4.git
cd systemprogramming-4
```

### 3. Compile the Code

Use GCC to compile the source code on each Raspberry Pi:
1. server.c
```bash 
gcc -o server server.c -lwiringPi -lpthread -lm
```

2. client1 (DHT1.c)
```bash
gcc -o client1 DHT1.c -lwiringPi -lpthread -lm
```

3. client2 (light.c)
```bash
gcc -o client2 light.c -lpthread
```

4. client3 (pir.c)
```bash
gcc -o client3 pir.c -lpthread -lwiringPi
```

## Usage

1. Connect the Sensors and Actuators
    - Rpi 1
        - DHT11 Sensor:

        Connect the VCC pin of the DHT11 sensor to the 5V pin on the Raspberry Pi.

        Connect the GND pin of the DHT11 sensor to the GND pin on the Raspberry Pi.

        Connect the DATA pin of the DHT11 sensor to the GPIO pin(2) on the Raspberry Pi.

    - RPi2 
        - Photoresistor:

        Connect one leg of the photoresistor to a 3.3V pin on the Raspberry Pi.

        Connect the other leg of the photoresistor to a GPIO pin(18) through a pull-down resistor.

    
    - RPi3 
        - PIR Sensor:

        Connect the VCC pin to the 5V pin on the Raspberry Pi.

        Connect the GND pin to the GND pin on the Raspberry Pi.

        Connect the OUTPUT pin to the PWM(0) on the Raspberry Pi.
    
        - LED:

        Connect one leg to the GPIO pin(18) and the other to the GND pin.
        - Servo Motor:

         Connect the power pins to the 5V and GND pins, and the control pin to a GPIO pin(18).

    
    - RPi4 
        - Piezo Buzzer:

        Connect the VCC pin to the 5V pin on the Raspberry Pi.
        
        Connect the GND pin to the GND pin on the Raspberry Pi.'
        
        Connect the OUTPUT pin to the GPIO pin(20) on the Raspberry Pi.

        - LED:
       
        Connect one leg to the GPIO pin(18) and the other to the GND pin.
    



2. Run the Program
  Execute the compiled programs: 
  First, you should execute the server.c to open the server.
  ```bash
  ./server
  ```

  And then, execute each client program.
   ```bash
    ./client1
    ./client2
    ./client3
   ```


   Each Raspberry Pi will perform its designated function, and the data will be sent to the remote server for monitoring.

## Contributors
- Kim Haechan: RPi 3, RPi4 Support, Model Design and Creation, Presentation, Socket Communication Environment Setting
- Lee Eunchae: RPi 1, RPi4, Proposal Writing, Final PPT, Gitlab Management, MoM Management
- Lee Eun: RPi 2, RPi4 Support, **Captain**, Proposal Writing, Final PPT
- Song InKyung: RPi 4 Support, Proposal Writing

## Question
If you have any question about this project, feel free to reach out us!

**leee331@ajou.ac.kr**


## Etc.
These below links provide detail explanation for the codes.

- DHT11.c

https://dandy-couch-5b8.notion.site/code-explanation-dht11-d2845e0bfda94d75b8165171570ee72d?pvs=4

- light.c

https://detailed-woodwind-16b.notion.site/Code-Explanation-light-c-e5d51f975dfd441bbf5550044a3e6050?pvs=4

- server.c

https://dandy-couch-5b8.notion.site/Code-Explanation-server-c-acb613f3acac4a9caec6752ad8e2ca59?pvs=4
# HSOMbehaviorSuite
## The Harvard School of Mouse Behavioral Control Suite

As used in Hamilos et al., _eLife_, 2021 (https://doi.org/10.7554/eLife.62583) and Hamilos and Assad, 2020 (https://doi.org/10.1101/2020.06.03.128272)

Contains behavioral control files for use with MATLAB 2016B and Arduino

The Harvard School of Mouse (HSOM) Behavioral Control Suite consists of an Arduino State System controller and MATLAB serial interface for setting parameters and collecting/viewing behavioral events on-line.

>Authors:
  >
  >Matlab Serial Interface:       Ofer Mazor
  >
  >Original Arduino State System Backbone:      Lingfeng Hou
  >
  >Matlab HSOM GUI Interface:       Lingfeng Hou and Allison Hamilos
  >
  >Harvard School of Mouse Arduino Controllers:       Allison Hamilos

Last updated by Allison Hamilos on May 19, 2020. Please contact ahamilos{at}g.harvard.edu if you have questions or encounter issues.

## Equipment Needed

  - MATLAB2016B+
  - Arduino 1.8.12+   NB: If using Teensy controllers, you will also need to install Teensy add ons to Arduino from PRJC: https://www.pjrc.com/teensy/
  - 2 Arduino controllers (We use Teensy 3.2 or 3.6)
    - One controller is for behavior, the other is for optogenetics
  - LEDs and speakers

## Instructions

0. Clone or download the repo. Add the folders to your MATLAB 2016B+ path.
1. You will need to configure each Arduino to match the pinouts annotated in the .ino files
2. Upload the Photometry_and_Optogenetics_Controller_2019.ino Arduino file to the Behavior Controller Arduino
3. Upload the Optogenetics_Controller4.ino Arduino file to the Optogenetics Controller Adruino
4. Open MATLAB2016B+ and run MouseBehaviorInterface
    - Select the COM port for the Behavior Controller Arduino
      - You can now edit parameters of the behavior controller, start and save files, and track behavior with the plotting window
5. For best results, open a new instance of Matlab to run the Optogenetics Controller, run: OptogeneticsController
    - Select the COM port of the Optogenetics Controller Arduino
      - You can now set stimulation parameters


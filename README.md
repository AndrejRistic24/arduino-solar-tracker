# SOLAR TRACKER - SUNFLOWER


## INTRODUCTION

This is an Arduino, embedded systems project with a goal to design, implement and test solar tracker function, which is supposed to regulate optimal position of the solar panel towards source of light (Sun) for random initial position of the panel. 

## LIST OF TASKS

This project had some inital tasks and functions and some were added while working on it. Some initial tasks were also modified.

### Initial tasks

- sketch block diagram of necessary component
- preparation of bill of material
- setup HW environment, assemble HW setup
- setup SW environment for target dev. board
- setup github repository

### Basic tasks

- differential light sensor
- servo engine for one axis (yaw angle)
  
### Advanced tasks

- basic + more advanced board
- GPS, RTC, Calendar
- 2nd servo engine for elevation angle according to the location, and time of the day and date

### Advanced functions:

- Start calibration of the device to avoid flip of the solar panel
- Power save, move either on some event or every 1 minute
- Compensate elevation of the ground using accelerometer
- PI regulation of the position
- Measurement of the voltage of the solar panel

### Added tasks and functions:

- Add states to the project
- Make a hysteresis with tolerances for entering and exiting states 
- Make a switch from a 180 to 360 degree motor
- Add a Hall sensor to serve as a limit to the rotation of the
- Add a "scan" state to find the optimal position after sleep
- Add "backoff" function if the system is trying to go past the limit
- Move from magnet when going to sleep 

  
## LIST OF CONTENT
This project has **4 codes available** and here is a short explanation for each one, while a more detailed explanation will be available latter in the text.

1. **stbasic.ino** - This is the simplest possible code for a solar tracker 
2. **statemachine01.ino** - Introduces states like **tracking** and **sleeping**
3. **180motor.ino** - Now a **3 state system** and code that has been upgraded
4. **360motor.ino** - Uses a **continuous 360 degree micro servo** with a **hall sensor** and a **magnet** to limit the rotation


## LIST OF COMPONENTS & SOFTWARE REQUIRED  

Here is a list of the software/programs and components that were used for this project. It shows the number of each component before their name, in brackets is the exact component i used and the numbers on the right show for which code was the component used for.

### Software/Programs

- Arduino IDE
- TinkerCad
- Microsoft Excel

### Electronic components

- 1x Arduino Board (Arduino UNO)  ----    /**1.**/**2.**/**3.**/**4.**/
- 1x Solar Panel (4V 260mA) ---- /**1.**/**2.**/**3.**/**4.**/
- 2x LDR Sensors ---- /**1.**/**2.**/**3.**/**4.**/
- 1x 180 Degree Servo Motor (180 SG90 Micro Servo) ---- /**1.**/**2.**/**3.**/
- 1x 360 Degree Servo Motor (SG90) ---- /**4.**/
- 1x Hall Sensor (3144E) ---- /**4.**/
- 1x Push Button ---- /**4**/
- 1x Real Time Clock (DS1302 RTC) ---- /**1.**/**2.**/**3.**/**4.**/
- 1x Battery (CR2032) ---- /**1.**/**2.**/**3.**/**4.**/
- 1x Breadboard  (830 Tie Points) ---- /**1.**/**2.**/**3.**/**4.**/
- 1x 2.0 USB Cable (Type A / Type B) ---- /**1.**/**2.**/**3.**/**4.**/
- 1x Power Adapter (5V 2A) ---- /**1.**/**2.**/**3.**/**4.**/
- Multiple Jumper Wires, Resistors, LE Diodes, Capacitors... ---- /**1.**/**2.**/**3.**/**4.**/

### Other components

- 1x Magnet ---- /**4.**/
- 2x Screws ---- /**1.**/**2.**/**3.**/**4.**/
- Carboard ---- /**1.**/**2.**/**3.**/**4.**/
- Popsicle Sticks / Small Wood Sticks ---- /**1.**/**2.**/**3.**/**4.**/
- Hot Glue Gun ---- /**1.**/**2.**/**3.**/**4.**/

**Note**: Not all components are necessary (eg. RTC and a battery, or most of the non-electronic components except for the magnet) and you dont need them, but take a look at the code to see if you need to change or delete something.

## VISUAL HELPER
### DIAGRAM
### VIDEO

## INSTRUCTIONS & EXPLANATIONS

Here are the instructions to setup

## UPDATES

## KNOWN ISSUES & FUTURE FIXES/CHANGES/UPGRADES

- Advanced task hasnt been done
- 3D print of the components carrying the panel and LDRs; Now just a demonstration model 
- Missing a slip ring for better and safer movement of cables
- Comments in codes are not in English
- Non consistent names of variables in different codes
- Tracking doesnt have a time limit in code number 4 (360motor)
- Code should be more readable, updates in "TRACKING" case and "moveServo" function needed
- 

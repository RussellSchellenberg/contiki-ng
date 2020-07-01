# Cooja To CSV

Convert the Cooja/Energest simulation log files to a .csv file

## Getting Started

These instructions will get the program up and running.

### Prerequisites

The prerequisites are python3 and python3-pandas

### Installing

Check to see if python3 is installed:

```
python3 --version
```

If it is not installed enter:

```
sudo apt-get update
sudo apt-get install python3
```

Use the following command to install the pandas module.

```
sudo apt-get install python3-pandas
```


## Running the program

To run the program, a simulation log must have already been written to a file.

### Simulation Log Requirements

The log needs to include lines with the specific format:

```
<Time> ID:<ID> Simulation_Data: <Mode[Light/Deep]> <Active Duration> <LPM1 Duration> <LPM4 Duration> <Total Duration> <Total Energy>
```

For example:

```
00:52.550	ID:5	Simulation_Data: Light     0    50 0    51      11250
```
Note that the Time and ID fields are automatically filled by Cooja when a line is printed to the log.
The fields are delimited by white space, so the spacing can be uneven and the program will still work.

### Usage

The command format is:

```
python3 cooja_to_csv.py --inputpath=<Path to simulation log file> --outputpath=<Path to destination file>
```

For example:

```
python3 cooja_to_csv.py --inputpath=ellipse-10-int20.txt --outputpath=ellipse-10-int20.csv
```

## Output

The program will produce 3 .csv files. The file names are based on the outputpath argument.

For example:

```
ellipse-10-int20.csv
ellipse-10-int20_deep_sleep.csv
ellipse-10-int20_light_sleep.csv
```

The first file is the tabulated data from the simulation file.

The deep sleep and light sleep files are pivot tables with linearly interpolated Total Energy values.
They are separated based on the Mode field into deep and light.
import datetime
import pandas
import argparse

parser = argparse.ArgumentParser(description='Convert Cooja logs to .csv format')
parser.add_argument('--inputpath', action='store',
                   help='The name of the file to convert.')

parser.add_argument('--outputpath', action='store',
                   help='The name of the file to convert.')

args = parser.parse_args()

rows = []
rows.append("Time,ID,Battery Value,Temperature Value\n")

start_time = 0

with open (args.inputpath, 'rt') as simfile:
    for line in simfile:
        if line.find("Received DATA") != -1:
            data = line.split()
            time = datetime.datetime.strptime(data[0]+" "+data[1], "[%Y-%m-%d %H:%M:%S]")
            if(start_time == 0):
                start_time = time
            time = (time - start_time).total_seconds()
            id = data[12]
            battery = data[8][0:-1]
            temperature = data[10][0:-1]
            rows.append(str(time) + "," + id + "," + battery + "," + temperature + "\n")
with open (args.outputpath, 'w') as csvfile:
    for line in rows:
        csvfile.write(line)

df = pandas.read_csv(args.outputpath)

df_bat = pandas.pivot_table(df, values='Battery Value', index=['Time'], columns=['ID'])
df_bat = df_bat.interpolate(method ='linear', limit_direction ='backward')
df_bat["Diff"] = (df_bat.iloc[:,0].sub(df_bat.iloc[:,1], axis=0))
df_bat.to_csv(args.outputpath[0:-4]  + '_battery.csv')

df_temp = pandas.pivot_table(df, values='Temperature Value', index=['Time'], columns=['ID'])
df_temp = df_temp.interpolate(method ='linear', limit_direction ='backward')
df_temp["Diff"] = (df_temp.iloc[:,0].sub(df_temp.iloc[:,1], axis=0))
df_temp.to_csv(args.outputpath[0:-4]  + '_temperature.csv')
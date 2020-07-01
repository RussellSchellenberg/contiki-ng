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
rows.append("Time,ID,Mode,Active Duration,LPM1 Duration,LPM4 Duration,Total Duration,Total Energy\n")

with open (args.inputpath, 'rt') as simfile:
    for line in simfile:
        if line.find("Simulation_Data: ") != -1:
            data = line.split()
            try:
                time = (datetime.datetime.strptime(data[0], "%M:%S.%f") - datetime.datetime(1900, 1, 1)).total_seconds()
            except:
                time = (datetime.datetime.strptime(data[0], "%H:%M:%S.%f") - datetime.datetime(1900, 1, 1)).total_seconds()
            id = data[1][3:]
            mode = data[3]
            active = data[4]
            lpm1 = data[5]
            lpm4 = data[6]
            total = data[7]
            energy = data[8]
            rows.append(str(time) + "," + id + "," + mode + "," + active + "," + lpm1 + "," + lpm4 + "," + total + "," + energy+"\n")
with open (args.outputpath, 'w') as csvfile:
    for line in rows:
        csvfile.write(line)

df = pandas.read_csv(args.outputpath)

for i in df["ID"].unique():
    df = df.append({'Time': 0, 'ID': i, 'Mode': 'Light', 'Active Duration': 0, 'LPM1 Duration': 0, 'LPM4 Duration': 0, 'Total Duration': 0, 'Total Energy': 0}, ignore_index=True)
    df = df.append({'Time': 0, 'ID': i, 'Mode': 'Deep', 'Active Duration': 0, 'LPM1 Duration': 0, 'LPM4 Duration': 0, 'Total Duration': 0, 'Total Energy': 0}, ignore_index=True)

df = df.sort_values(by=['Time'])

light_df = df.loc[df['Mode'] == 'Light']
light_df = pandas.pivot_table(light_df, values='Total Energy', index=['Time'], columns=['ID'])
light_df = light_df.interpolate(method ='linear', limit_direction ='backward')
light_df.to_csv(args.outputpath[0:-4] + '_light_sleep.csv')

deep_df = df.loc[df['Mode'] == 'Deep']
deep_df = pandas.pivot_table(deep_df, values='Total Energy', index=['Time'], columns=['ID'])
deep_df = deep_df.interpolate(method ='linear', limit_direction ='backward')
deep_df.to_csv(args.outputpath[0:-4]  + '_deep_sleep.csv')
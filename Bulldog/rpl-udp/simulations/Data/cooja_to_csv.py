import datetime
import pandas
import argparse

def custom_pivot(df):
    df = pandas.pivot_table(df, values='Total Energy', index=['Time'], columns=['ID'])
    df = df.interpolate(method ='linear', limit_direction ='backward')
    df['Average'] = df.mean(axis=1)
    return df

parser = argparse.ArgumentParser(description='Convert Cooja logs to .csv format')
parser.add_argument('--inputpath', action='store',
                   help='The name of the file to convert.')

parser.add_argument('--outputpath', action='store',
                   help='The name of the file to convert.')

args = parser.parse_args()

rows = []
rows.append("Time,ID,Active Duration,LPM1 Duration,LPM4 Duration,RX Duration,TX Duration,Total Energy\n")

with open (args.inputpath, 'rt') as simfile:
    for line in simfile:
        if line.find("Simulation_Data: ") != -1:
            data = line.split()
            try:
                time = (datetime.datetime.strptime(data[0], "%M:%S.%f") - datetime.datetime(1900, 1, 1)).total_seconds()
            except:
                time = (datetime.datetime.strptime(data[0], "%H:%M:%S.%f") - datetime.datetime(1900, 1, 1)).total_seconds()
            id = data[1][3:]
            active = data[3]
            lpm1 = data[4]
            lpm4 = data[5]
            rx = data[6]
            tx = data[7]
            energy = data[8]
            rows.append(str(time) + "," + id + "," + active + "," + lpm1 + "," + lpm4 + "," + rx + "," + tx + "," + energy+"\n")
with open (args.outputpath, 'w') as csvfile:
    for line in rows:
        csvfile.write(line)

df = pandas.read_csv(args.outputpath)

for i in df["ID"].unique():
    df = df.append({'Time': 0, 'ID': i, 'Active Duration': 0, 'LPM1 Duration': 0, 'LPM4 Duration': 0, 'RX Duration': 0, 'TX Duration': 0, 'Total Energy': 0}, ignore_index=True)

df = df.sort_values(by=['Time'])

df = custom_pivot(df)
df.to_csv(args.outputpath[0:-4] + '_energy.csv')

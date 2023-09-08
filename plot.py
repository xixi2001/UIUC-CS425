import numpy as np
import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator

#define dataset
data = [[0.084054, 0.082063, 0.083191, 0.081871, 0.081458],
        [1.8984, 1.87932, 1.88892, 1.88004, 1.88754],
        [11.285, 11.1959,  11.2503, 11.1947, 11.2288],
        [18.7288, 18.654, 18.7789, 18.7539, 18.6447]]

std_errors = []
for i in range(4):
    std_errors += [np.std(data[i], ddof=1) / np.sqrt(len(data[i]))]
print(std_errors)
avgs = []
for i in range(4):
    avgs += [sum(data[i])/len(data[i])]
print(avgs)

#define chart 
fig, ax = plt.subplots()

#create chart
ax.bar(x=["^107", "POST", "GET", "HTTP"], #x-coordinates of bars
       height=avgs, #height of bars
       yerr=std_errors, #error bar width
       capsize=4) #length of error bar caps


plt.title("grep result")
plt.xlabel("commands")
plt.ylabel("seconds")

plt.show()
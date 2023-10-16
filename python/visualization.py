import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

# import matplotlib
# matplotlib.use('TkAgg')

import matplotlib.pyplot as plt
import time
from PyQt5.Qt import QT_VERSION_STR

print(QT_VERSION_STR)

start_time = time.time()

cred = credentials.Certificate(
    "secrets/iot-esp32-62cd3-firebase-adminsdk-hokuz-e3597231bb.json")

firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://iot-esp32-62cd3-default-rtdb.europe-west1.firebasedatabase.app'
})

root = db.reference("/")

temperatures = []
humidities = []
times = []
child_list = list(root.get().keys())
reversed_child_list = child_list[::-1]
counterInChilds = 0

def zeitanpassung(x):
    result = (x*60)/2
    return result


# Definiert Zeitinterval, hier eine 12 ergibt einen Plot der letzten 12h
zeitInStunden = 18
zeit = zeitanpassung(zeitInStunden)

print(len(reversed_child_list))

for child_key in reversed_child_list:

    if counterInChilds >= zeit:
        break

    child_ref = db.reference(f"/{child_key}")
    child_data = list(child_ref.get())
    reversed_child_data = child_data[::-1]
    print("fetched child ref")

    for data in reversed_child_data:
        
        # Check if the current item is not None
        if data is not None and len(humidities) < zeit and len(temperatures) < zeit and len(times) < zeit:

            if counterInChilds >= zeit:
                break

            humidity = data['Humidity']
            humidities.append(humidity)

            temperature = data['Temperature']
            temperatures.append(temperature)

            hour = data['Day Hour']
            minute = data['Day Minutes']

            current_time = hour + minute / 60

            # Calculate the time in hours and append it to the list
            times.append(current_time)

            counterInChilds += 1


max_value_humidities = max(humidities)
max_value_temperatures = max(temperatures)

min_value_humidities = min(humidities)
min_value_temperatures = min(temperatures)

fig, ax1 = plt.subplots()
ax2 = ax1.twinx()

ax1.plot(times, humidities, 'b-')
ax2.plot(times, temperatures, 'r-')

# Füge Beschriftungen und Titel hinzu
ax1.set_xlabel('time')
ax1.set_ylabel('humidity', color='b')
ax2.set_ylabel('temperature', color='r')
plt.title('Humidity and Tempreature')

# Add a legend
legend_label0 = "Measured values:"
legend_label1 = "Show for last {} hours".format(zeitInStunden)
legend_label2 = "Max Value Humidity: {:.2f}%".format(max_value_humidities)
legend_label3 = "Max Value Tempreature: {:.2f}°C".format(
    max_value_temperatures)
legend_label4 = "Min Value Humidity: {:.2f}%".format(min_value_humidities)
legend_label5 = "Min Value Tempreature: {:.2f}°C".format(
    min_value_temperatures)

legend_labels = []

# Set the y position of the text
text_y_position = 0.9

legend_labels.append(legend_label0)
legend_labels.append(legend_label1)
legend_labels.append(legend_label2)
legend_labels.append(legend_label3)
legend_labels.append(legend_label4)
legend_labels.append(legend_label5)

for label in legend_labels:
    plt.figtext(0.01, text_y_position, label, wrap=True,
                horizontalalignment='left', fontsize=10)
    text_y_position -= 0.05

# Show grid lines on the chart
plt.grid()

end_time = time.time()
elapsed_time = end_time - start_time
print(f"Laufzeit des Skripts: {elapsed_time} Sekunden")

# Code to create the plot
plt.savefig('figure.png', dpi=300)

# plt.show()

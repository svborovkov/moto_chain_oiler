Chain oiler for motorbikes.

Code is written for arduino IDE. I used wemos mini, its ESP8266 baised module.
Features:
1. It uses GPS to oil chain based on speed. It oils chain every XX meters, but only if speed is grater then YY km/h.
2. If GPS position is not available it will start to oil chain periodically. 
3. It creates wifi hotspot + cative portal. So you can connect to oiler, phone will open web page and you can look/change settings.
4. It supports one place (GPS position) to avoid oiling chain near that point. It can be useful to not oil floor of garage.
5. It supports multiple settings. By default there are 5 modes: Usual, Sand, Rain, stop oiling, and continuous pumping for filling tubes. Its adjustable.

I used diesel pump from heater, you can use any oil pumps from scooters, they are much smaller.
You can use any GPS reciever compatible with tinyGPS library. I think most GPS modules from aliexpress are compatible with tinyGPS.

I had issue with GPS signal when ESP8266 was near GPS reciever, so i putted GPS module on ~30 cm wire.

About nozzles: I used brass tube 4 mm, soldered it as "Y" letter and putted cutted syringe needles to ends. Needles have small hole in it so oil doesn't drain to the floor when bike is parked. The needles are easy to replace if they get dirty.
# Setting Up ezDV

There are several steps required to get ezDV up and running with your radio.

## Initial Assembly

If ezDV did not already come with a battery, you will need to obtain a 3.7V lithium polymer (LiPo) battery.
One recommended battery can be purchased [from Amazon](https://www.amazon.com/gp/product/B08214DJLJ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1).

*WARNING: Do not puncture or otherwise damage lithium batteries as injury and/or property damage could result!
Neither the project nor its creator take responsibility for any damage or injury due to misuse of batteries. A 
battery with a protection module (such as the one linked above) is highly recommended.*

The battery should be plugged into the BT1 connector on the bottom of the ezDV circuit board, which is
the circled connector below:

![The back of the ezDV circuit board with BT1 circled](images/2-setup-battery-location.jpg)

Once a battery is attached, you should flip the switch to the right of the BT1 connector upwards (towards the back of the ezDV board)
to connect the battery to the rest of the board. This switch is designed to allow for long-term storage or shipping of ezDV without 
discharging the battery. Note that ezDV may power on immediately after flipping this switch; you can power it down by pushing
and holding the Mode button (second button from the top when looking downward) until red lights turn on on the right-hand side.

*Note: it is not recommended to use this switch for normal operation of ezDV. Some features require an orderly shutdown of ezDV
(which is accomplished by using the Mode button) and may not function properly if power is suddenly lost.*

Next, plug ezDV into a USB-C charger (similar to what would be done to charge a cell phone or portable power bank). A red LED
may light on the bottom of ezDV indicating that the battery is charging as well as some or all of the LEDs on the right hand side
(indicating its current state of charge in 25% increments). When ezDV is done charging, a green LED will turn on
and the red LED will turn off. This may take several hours or more depending on the state of the battery.

Finally, plug in a wired headset on the TRRS jack located on the right side of the ezDV board (when looking
from above). This headset should be wired for compatibility with Android phones, which is as follows:

* Tip: RX audio from ezDV
* Ring 1: RX audio from ezDV
* Ring 2: GND
* Shield: TX microphone audio to ezDV

Once attached, ezDV will appear as follows:

![ezDV with headset attached](images/2-setup-headset-location.jpg)

## Radio Wiring

### Radios without Wi-Fi support

Radios without Wi-Fi support will need an interface cable. One end of this cable should 
be a male 3.5mm four conductor (TRRS) with the following pinout:

* Tip: TX audio from ezDV to your radio
* Ring 1: PTT to radio (this is connected to GND when you push the PTT button)
* Ring 2: GND
* Shield: RX audio from your radio to ezDV

There is also silkscreen labeling on the back of the board corresponding to the location of 
the radio jack that indicates the pinout. This jack is on the left hand side of ezDV when looking
downward at the front of the device.

#### Example Wiring Configurations

Some example configurations are below.

##### Elecraft KX3

If you have an Elecraft KX3, you can use off the shelf parts for your interface cable
(no soldering required). These parts consist of a [3.5mm splitter](https://www.amazon.com/Headphone-Splitter-KOOPAO-Microphone-Earphones/dp/B084V3TRTV/ref=sr_1_3?crid=2V0WV9A8JJMW9&keywords=headset%2Bsplitter&qid=1671701520&sprefix=headset%2Bsplitte%2Caps%2C136&sr=8-3&th=1)
and a [3.5mm TRRS cable](https://www.amazon.com/gp/product/B07PJW6RQ7/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1). The microphone
connector on the splitter should be plugged into PHONES on the KX3 while the speaker connector should be plugged into MIC.

This setup will look like the following when plugged in:

![ezDV plugged into an Elecraft KX3](images/2-setup-radio-kx3.jpg)

### Radios with Wi-Fi support

If you have a radio that is capable of remote network access, you can configure ezDV to connect to the radio
over Wi-Fi. Currently this is supported only for the [FlexRadio](https://www.flexradio.com/) 6000 series radios 
and the Icom IC-705.

#### Flex 6000 series radios

Ensure that you can access your radio using the SmartSDR software from a PC on the same network (i.e. *not* using SmartLink).
Once this is confirmed, skip to [Initial Configuration](#initial-configuration).

#### Icom IC-705

Several settings will need to be adjusted in the IC-705 before it can be used with ezDV. First, you will need to decide
whether you want to join your IC-705 to an existing Wi-Fi network or whether you want to configure ezDV to use the IC-705's
built-in access point.

*Note: it is recommended to join your IC-705 to an existing Wi-Fi network in order to be able to access ezDV's Web interface
(and to take advantage of certain features if an Internet connection is available).*

Next, you'll need to turn on your radio and push the Menu button. This will bring up the following screen:

![Icom IC-705 on menu screen](images/2-setup-radio-ic705-mainmenu.jpg)

Tap the Set button on the lower-right corner of the display and then select "WLAN Set" as shown below:

![Icom IC-705 with WLAN Set selected](images/2-setup-radio-ic705-setmode-3.jpg)

Next, tap the WLAN option on the first page and turn it OFF. Once switched OFF, tap the Connection Type setting and tap either
Station (for connecting to an existing Wi-Fi network) or Access Point (for connecting ezDV to the IC-705's access point):

![Icom IC-705 displaying WLAN Set options](images/2-setup-radio-ic705-wlan-set-1.jpg)

![Icom IC-705 displaying Connection Type menu](images/2-setup-radio-ic705-connection-type.jpg)

##### Configuring Wi-Fi on the IC-705

###### Using the IC-705's built-in access point

If you're using the IC-705's Access Point mode, it is a good idea to double-check your radio's SSID (aka Wi-Fi network name) and 
Wi-Fi password by tapping "Connection Settings (Access Point)" and then tapping each option as shown below, updating them as needed:

![Icom IC-705 displaying Access Point settings](images/2-setup-radio-ic705-connection-settings-ap.jpg)

Also, note the IP address that your IC-705 is using on this page as you will need it for later configuration of ezDV. Once the settings
here are satisfactory, tap the Back button on the lower right hand corner of the screen, tap the WLAN option and then change it to ON.

###### Connecting the IC-705 to an existing network

First, tap the WLAN option on the WLAN Set page and change it to ON. This is needed in order to turn on the Wi-Fi radio in the IC-705
and allow it to scan for networks.

Next, tap "Connection Settings (Station)". You will see the following page on the IC-705:

![Icom IC-705 displaying the Connection Settings (Station) page](images/2-setup-radio-ic705-connection-settings-sta.jpg)

From here, you can either tap "Access Point" to select your network from a list or "Manual Connect" if it's not broadcasting
its name. Tapping "Access Point" will bring up a page similar to the following:

![Icom IC-705 displaying a list of Wi-Fi networks](images/2-setup-radio-ic705-access-point.jpg)

Tap your preferred network from the list that appears. If it's not already saved in the IC-705, you may see a page similar
to the following:

![Icom IC-705 displaying the Connect page](images/2-setup-radio-ic705-connect.jpg)

Tapping Password will display the following page that will allow you to enter your Wi-Fi network's password:

![Icom IC-705 displaying the Password page](images/2-setup-radio-ic705-password.jpg)

Once entered, tap the ENT button to save it, then tap the Connect option to connect to the network. If successful,
the Access Point page will show "Connected" below your network's name. Tap the Back button until you return to the
"Connection Settings (Station)" page and make sure you see an IP address under "DHCP (Valid After Restart)":

![Icom IC-705 displaying an IP address](images/2-setup-radio-ic705-connection-settings-sta-with-ip.jpg)

Note this address as it will be needed for later configuration of ezDV.

##### Configuring the Radio User

The IC-705 requires a username and password to connect to the radio. If this is not already set up, you will need
to do so now before continuing with ezDV setup. First, ensure that you're on the "WLAN Set" page and then go to the
second page of options. You will see a "Remote Settings" option:

![Icom IC-705 displaying the Remote Settings option](images/2-setup-radio-ic705-wlan-set-2.jpg)

Tap on "Remote Settings" and then tap on "Network User1":

![Icom IC-705 displaying the "Network User1" option.](images/2-setup-radio-ic705-remote-settings.jpg)

The following page should appear. Tap on "Network User1 ID" and "Network User1 Password" and enter a username and password
(if not already saved). Make a note of these as you'll need them later. Additionally, make sure "Network User1 Administrator"
is set to "YES".

![Icom IC-705 displaying the "Network User1" settings.](images/2-setup-radio-ic705-network-user1.jpg)

Tap the Back button until you reach the Set menu.

##### Configuring Audio

The IC-705 must be configured to accept transmit audio from the Wi-Fi interface in order for transmit with
ezDV to work properly. From the Set menu, select "Connectors" as shown below:

![Icom IC-705 displaying the "Connectors" option.](images/2-setup-radio-ic705-setmode-2.jpg)

Next, scroll until you find the "MOD Input" option and then tap it to continue:

![Icom IC-705 displaying the "MOD Input" option.](images/2-setup-radio-ic705-connectors-2.jpg)

Finally, tap the "DATA MOD" option and ensure that it is set to "WLAN":

![Icom IC-705 displaying the "MOD Input" menu.](images/2-setup-radio-ic705-mod-input.jpg)

![List of valid "DATA MOD" options.](images/2-setup-radio-ic705-data-mod.jpg)

*Note: if you share the IC-705 with other data mode applications over USB (for example, WSJT-X for FT8), make sure
that "DATA MOD" is set to one of the "USB" options above when done using ezDV.*

## Initial Configuration

To perform initial configuration of ezDV, push and hold both the Mode and Volume Down buttons at the same
time until all four of the LEDs on the right hand side of ezDV turn on.

![ezDV showing the buttons to push and hold (red circles) and the location of the LEDs (pink circle)](images/2-setup-power-on.jpg)

Once the LEDs turn off, go to your PC or cell phone's Wi-Fi configuration and join a network named "ezDV xxxx",
where "xxxx" is the last few alphanumeric digits of its MAC address (a unique identifier for each network device). 
Once joined, open your Web browser and navigate to http://192.168.4.1/. You should see a page similar to the following:

![Screenshot showing ezDV's main Web page](images/2-setup-webpage-general.png)

### Transmit Audio Levels (except Flex 6000 series)

*Note: for the Icom IC-705, this can only be done after configuring Wi-Fi and the radio connection.*

ezDV's transmit audio levels will need to be adjusted to ensure that your radio does not indicate too much ALC, similar
to other HF digital modes. Push the Mode button once; you should hear "700D" in Morse Code in your headset within a second
after releasing the button. After doing so, you can adjust the transmit audio by pushing the Volume Up or Volume Down buttons
while holding the PTT button as indicated below:

![ezDV showing the PTT button (red circle) and the Volume Up/Down buttons (pink circle)](images/2-setup-tx-audio-levels.jpg)

For most radios, ezDV's transmit level should be such that little to no ALC is indicated on the radio display (or alternatively,
until power output just starts dropping from the level set in the radio). The specific ALC level required depends on the radio, so
your radio's user manual is the best source for determining this.

### Wi-Fi Configuration

*Note: this is optional unless you are using a radio with Wi-Fi support or wish to use other Internet connected features
(such as reporting to [FreeDV Reporter](https://qso.freedv.org/)).*

Click or tap on the "Wi-Fi" tab and check the "Enable Wi-Fi" checkbox. You'll see something similar to the following:

![ezDV showing the Wi-Fi tab](images/2-setup-webpage-wifi.png)

To connect ezDV to the network that your radio is on, select "Client" for "Wireless Mode". Select your Wi-Fi network from
the list that appears (which may take a few seconds while ezDV scans for your network) and enter its password (if required).
If desired, you can also change ezDV's name by modifying the "Hostname" field; this may allow you to access ezDV later by entering
this name instead of trying to find its IP address. Click or tap Save to save your Wi-Fi settings.

### Radio Connection

*Note: this is optional unless you are using a radio with Wi-Fi support.*

Click on the "Radio" tab and check the "Use Wi-Fi Network for Radio RX/TX" checkbox. Select your radio model in the "Radio Type"
drop-down list, which will bring up different fields depending on the radio being configured.

#### Flex 6000 series

Select the name of your radio from the "Radio" drop-down list as shown below. Your radio's IP address should automatically
be filled in the "IP Address" field:

![ezDV showing a FlexRadio configuration](images/2-setup-webpage-radio-flex.png)

Click or tap Save to save the radio configuration.

#### Icom IC-705

Enter your radio's IP address, username and password that you saved when you configured Wi-Fi on the IC-705:

![ezDV showing a Icom IC-705 configuration](images/2-setup-webpage-radio-icom.png)

Click or tap Save to save the radio configuration.

### Reporting Configuration

It is highly recommended to configure your callsign and grid square. This has several benefits:

1. If ezDV is lost or stolen, someone can access its Web page and use callsign databases like [QRZ](https://www.qrz.com) to find a way to get it back to you.
2. Other users will be able to decode your callsign and report spots on [FreeDV Reporter](https://qso.freedv.org).
3. If ezDV has Internet access, it will also be able to decode callsigns from received signals and report them itself to FreeDV Reporter.

To do this, click or tap on the "Reporting" tab and enter your callsign and grid square where prompted:

![ezDV showing a sample reporting configuration](images/2-setup-webpage-reporting.png)

If desired, you can also enter a short message that will appear on FreeDV Reporter. This message is transmitted solely over the Internet, not over RF.

Once done, click or tap Save to save the reporting configuration.

### Final Setup

After your radio, Wi-Fi and reporting configuration are complete, simply click or tap on the General tab and then click or tap on the Reboot button.
ezDV will first show two red LEDs on the right hand side for a few seconds, then show all four LEDs for a few seconds, then turn them all off. If you're
using a wired connection to ezDV, it is now ready for use.

If you configured a Wi-Fi network and radio for ezDV to connect to, ezDV should then begin blinking a blue LED (indicating that it is connected to the Wi-Fi
network) followed by showing a solid blue LED if it's able to connect to the radio. On the IC-705, a WLAN indicator will appear if ezDV is connected to the radio
as shown below:

![Icom IC-705 properly connected to ezDV (WLAN indicator at the top of the screen)](images/2-setup-radio-ic705-connected.jpg)

For Flex 6000 series radios, opening the SmartSDR software will show a "FDVU" (for upper sideband) and "FDVL" (for lower sideband) if ezDV is properly connected
to the radio:

![Flex 6300 properly connected to ezDV (existence of FDVU/FDVL modes)](images/2-setup-radio-flex-connected.png)

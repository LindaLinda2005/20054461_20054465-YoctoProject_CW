# **Project Overview**

This README file contains an overview of our project to create an image for the Raspberry Pi 5 from a host platform. We ultimately chose Yocto to perform this task as despite its more challenging learning curve, we believed it would provide more benefits compared to Buildroot when it came to debugging and iterating changes down the line.

Our project involved creating a c program which we would then include in our Yocto build to cross-compile it for the aarch64 architecture of the RPI5. Additionally, we wanted to install/modify files via the Yocto build process, with the goal being to create an image that required little to no set-up once flashed to the RPI SD.

# **Student Information**

- **Cameron Wilkinson** (20054461)
- **Lewis Ball** (20054465)

# **System Information**

- **Host System/s:** Ubuntu 20.04.6 LTS, x86_64
- **Target System:** RaspberryPi 5, aarch64

# **Contents of Repository Files**

- `ProgramFiles/CMakeLists.txt`, `ProgramFiles/main.c`, `ProgramFiles/libmod_demo`
  These are the program files generated by CLion (the IDE used to write code for this project), including the compiled executable for x86_64 architecture.

- `YoctoFiles/meta-custom-image-mods/conf/layer.conf`
  This file contains the configuration details for the custom Yocto layer and was put together following the format layed out within the Yocto documentation (https://docs.yoctoproject.org/dev/dev-manual/layers.html#creating-your-own-layer).

- `YoctoFiles/meta-custom-image-mods/network-configuration-add`
  This recipe directory contains the .bb recipe to build a package that installs the following to the built image:
  - `YoctoFiles/meta-custom-image-mods/network-configuration-add/files/10-eth0.network`
    This enables a default IP address of 192.168.137.100/24 to the eth0 port on the RPI5.
  - `YoctoFiles/meta-custom-image-mods/network-configuration-add/files/wpa_supplicant.conf`
    This installs a Wi-Fi configuration file. The wpa_supplicant service still requires manually starting via command line upon start up, however, requires no manual set-up due to this file.

- `YoctoFiles/meta-custom-image-mods/temp-monitor-program`
  This recipe directory contains the program files and .bb recipe which unzips, compiles, and installs the compiled binary to the final image build.
  - `YoctoFiles/meta-custom-image-mods/temp-monitor-program/custom-temp-monitor-10.tar.gz`
    This compressed directory contains the .c & CMakeLists.txt files for the program.

- `YoctoFiles/meta-custom-image-mods/u-boot-fix`
  This directory contains a .bbappend file which patches an exiting file generated by another Yocto layer, with the purpose of fixing a bug that prevented the image from booting WITHOUT a valid serial connection (this will be described in more detail below).
  - `YoctoFiles/meta-custom-image-mods/u-boot-fix/files`
    This file contains the details of which lines to remove/add & what text should be modified.

- `YoctoFiles/custom-image-mods.yml`
  This file contains the configuration details for the custom layer. This includes the path to the custom layer directory and text to add to the generated local.conf file for the image build process. The text added to the local.conf file includes bug fixes that will be discussed further below.

- `YoctoFiles/raspberrypi5.yml`
  This file is a pre existing kas file, which is run to generate the build environment using the kas tool/script. It has been modified to include the custom-image-mods.yml as an additional header file.

# **How to Replicate Our Build**

For this project, I spent a long time searching for a guide that built a working Yocto image for the Raspberry Pi 5. As the RPI5 is relatively new, there hasn’t been as much development for the board compared to previous versions, and the documentation surrounding Yocto is still minimal. Eventually, we found a guide that created a working RPI Yocto image with an IoT tool called Mender installed (https://hub.mender.io/t/raspberry-pi-5/6689). We then modified the process in this guide to include our custom Yocto layer alongside the Mender install. Below is the process we followed to create our Yocto image:

Step 1: Prerequisites
Follow the prerequisites described in the linked guide.

Step 2: Set Up Yocto Environment
- Create a directory for the Yocto environment and navigate into it:
  `mkdir mender-raspberrypi5 && cd mender-raspberrypi5`
- Clone the Mender community repository:
  `git clone https://github.com/theyoctojester/meta-mender-community -b scarthgap`

Step 3: Add Custom Layer
Copy the custom layer directory to the following location:
`cp -r <path-to-custom-layer-dir> mender-raspberrypi5/meta-mender-community`

Step 4: Replace Configuration File
Replace the raspberrypi5.yml file with the modified version:
`cp <path-to-modified-raspberrypi5.yml> mender-raspberrypi5/meta-mender-community/kas/raspberrypi5.yml`

Step 5: Add Custom Image Modifications
Copy custom-image-mods.yml to the include folder:
`cp <path-to-custom-image-mods.yml> mender-raspberrypi5/meta-mender-community/kas/include/custom-image-mods.yml`

Step 6: Set Up the Build Environment
Follow these steps to set up the build environment:
- Navigate into the Mender community directory:
  `cd meta-mender-community`
- Create a directory for the Raspberry Pi 5 build:
  `mkdir my-raspberrypi5 && cd my-raspberrypi5`

Step 7: Add the Custom Layer
Add the custom layer to the Yocto build:
`bitbake-layers add-layer ../../meta-custom-image-mods`

Step 8: Build the Image
Run the following command to build the image:
`bitbake core-image-base`

# **Running and Testing Instructions**

Step 1: Network Configuration
Ensure both Raspberry Pis are connected to the same local network or via a direct Ethernet connection.

Step 2: Setting IP Address
On the Client RPI, configure a new IP address using the following commands:
- Delete the existing IP address
  `ip addr del 192.168.137.100/24 dev eth0`
- Add a new unique IP address with the same subnet as the server RPI
  `ip addr add 192.168.137.2/24 dev eth0`

Step 3: Running the program
- Start the program as server (on the RPI configured as the server)
  `libmod_demo server Y #replace Y with N for no debugging messages`
- Start the program as client (on the RPI configured as the client)
  `libmod_demo client 5 Y #5 is the logging rate in seconds`
  - Next enter the maximum allowable temperature in degrees C

Step 4: Verifying Communication
On the client, confirm that the CPU temperature is read from the server and displayed.
Check if the "start/stop" register updates successfully when the temperature exceedes the maximum set value.

For further verification if required, the below is our list of requirements we wrote our program to.

Requirements:
- the client must read temp data at a set time (logging rate)
- the logging rate should be set by the user
- the max temp should be set by the user
- the client must write to the server if the avg temp goes out of bounds for more than 3 reads
- the server must store cpu temp into modbus register (addr[0]) when it recieves a read request
- the server must stop/start another program depending on a modbus register
  
# **Issues We Faced**

During this project we faced several issues & bugs which we had to resolve, this section will cover the main issues & how we resolved them.

The first & most frustrating issue of the project occurred at the start. Following online guides no one in our class was able to create a bootable image using either Yocto or Buildroot. We spent a lot of time investigating this issue, I (Cameron) even created a forum post discussing with someone who didn't have our issue. Eventually we discovered the RPI5 2GB variant used a different chip and required an overlay file which wasn't included in the image (for further details see the forum discussion: (https://hub.mender.io/t/asynchronous-serror-interrupt-rpi5-boot-issue/7275). To resolve this issue, the below line was added to the .yml file for inclusion in the local.conf.
- `RPI_KERNEL_DEVICETREE_OVERLAYS:append = " overlays/bcm2712d0.dtbo"`

The second most frustrating issue, and perhaps the most bizarre, was only discovered late in the testing phase. Throughout all our initial debugging & testing, we had been using a host laptop to communicate with the pi & act as a pseudo pi, while also being able to monitor the terminal of the actual pi via serial (this allowed us to run 2 consoles side by side on one screen). However, when we came to test the pi's over HDMI, they wouldn't get past the U-boot logo. After investigation and further testing, we ultimately found that when the serial was not connected, the u-boot was seeing/reading a ghost key press (we're still not sure on the reason behind this). This was causing the u-boot to exit automatic startup because of the 'press any key to pause' prompt. As a solution to this, we added a patch to the u-boot configuration via our custom layer which added a special condition that the 'press any key to pause' now required a press of the specific 'u' key instead.

Finally, we also saw a small issue when trying to set a custom & default IP address for the eth0 port via the Yocto image. I was following a guide which for the most part was correct, but didn't mention how the networking config file is not prioritised over file location, but instead file naming convention. Eventually I found a forum post (https://superuser.com/questions/1694538/systemd-networkd-what-is-the-configuration-file-precedence) which explained the eth0.network file is prioritised on the name in ASCII order (i.e. 10-eth0.network would be picked before 80-eth0.network, which would be picked before eth0.network).


# **End of Document**

# Instruction to install ccnSim-Parallel

In this section we will cover all the steps needed for the installation of ccnSim-Parallel. We suggest the reader to check and execute them by following the presentation order.

## Portability

ccnSim-Parallel has been tested on the following platforms, and with the following softwares:
* Ubuntu Linux 14.04 (64-bit)
* Ubuntu Linux 16.04 Server (64-bit)
* Omnetpp-4.6
* Omnetpp-5.0 • Akaroa-2.7.13

## Prerequisites
* Boost libraries ≥ 1.54: they can be installed either by using the standard packet manager of your system (e.g., apt-get install, yum install, port install, etc.), or by downloading them from http://www.boost.org/users/download/, and following instructions therein.
* gcc > 4.8.1: on Ubuntu platforms, gcc can be updated by adding the ubuntu-toolchain-r/test PPA. Sample commands for the latest 5.x version are:
        sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-5 g++-5
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60
--slave /usr/bin/g++ g++ /usr/bin/g++-5
• parallel-ssh: in order to ease the simulation workflow, a passwordless SSH login needs to be present on all the available servers. parallel-ssh can than be used in order to send commands to slaves from the master node.
sudo apt-get install pssh
Akaroa2©
As mentioned above, Akaroa2© requires a specific license in order to be used. For instructions about software download and licenses, please visit the website https://akaroa.canterbury. ac.nz/akaroa/. The design of ccnSim-Parallel, concerning the Akaroa component, has started from the Akaroa-2.7.13 version. The whole set of modifications that have been made in order to implement the parallel ModelGraft strategy are grouped inside the Akaroa 2.7.13 for PMG.patch patch, which is freely available at http://perso.telecom-paristech.fr/~drossi/ccnSim.
Once obtained the original licensed Akaroa-2.7.13, place it in the same directory with the aforementioned patch, and type:
￼￼￼2
￼patch -s -p0 < Akaroa 2.7.13 for PMG.patch
Notice that the original Akaroa directory will keep the same name even after the application of the patch.
Once the patch is applied, type the following commands from the root directory of the patched Akaroa in order to install it:
Akaroa binaries, libraries, and include files will be installed in /usr/local/akaroa/bin, /usr/local/akaroa/lib, and /usr/local/akaroa/include, respectively. If you do not have sudo privileges, and you need to
install Akaroa into another location, e.g., <install path>, please add this location to your PATH
variable, and use the following commands:
Furthermore, always in the case of lacking sudo privileges, users might need to modify LIB and PYTHON paths in the several “Makefile.config *” files and inside the “pyconfig.py” file according to the customized locations of related libraries, before compiling and installing Akaroa.
Omnet++
ccnSim-Parallel has been tested with both Omnetpp-4.6 and Omnetpp-5.0, which are available at https://omnetpp.org/. Before the installation, Omnetpp-x requires prerequisite packages to be installed. For the complete list, please read the official installation manual. Furthermore, since Omnetpp-x bin/ directory needs to be added to the PATH variable, the following line should be put at the end of the 􏰅/.bashrc file:
export PATH=$PATH:$HOME/omnetpp-x/bin
After having closed and saved the file, please close and re-open the terminal.
Regardless of the Omnetpp version, a couple of patch files need to be applied to it before com- pilation and installation. These patch files are provided with ccnSim-Parallel, which we suppose being correctly downloaded (see next section) and decompressed in the $CCNSIM DIR. Provided that Omnetpp is present in the $OMNET DIR, specific instructions to patch, compile, and install it without the graphical interface (not needed for parallel ModelGraft simulations) are provided in the following, according to the selected version:
• Omnetpp-4.6
pint:􏰅$ cd $CCNSIM DIR
pint:CCNSIM DIR$ cp ./patch/omnet4x/ctopology.h $OMNET DIR/include/ pint:CCNSIM DIR$ cp ./patch/omnet4x/ctopology.cc $OMNET DIR/src/sim/ pint:CCNSIM DIR$ cd $OMNET DIR/
pint:OMNET DIR$ NO TCL=1 ./configure
￼￼￼￼./configure
￼make
￼sudo make install
￼￼./configure --prefix=<install path>
￼￼make
￼make install
￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼￼pint:OMNET DIR$ make
￼3
• Omnetpp-5.0
pint:􏰅$ cd $CCNSIM DIR
pint:CCNSIM DIR$cp ./patch/omnet5x/ctopology.h $OMNET DIR/include/omnetpp pint:CCNSIM DIR$cp ./patch/omnet5x/ctopology.cc $OMNET DIR/src/sim pint:CCNSIM DIR$ cd $OMNET DIR/
pint:OMNET DIR$ ./configure WITH TKENV=no
pint:OMNET DIR$ make
Compilation flags can also be set through the file configure.user (e.g., to disable Qtenv).
ccnS im-Parallel
As anticipated above, the .tgz file containing the ccnSim-Parallel code can be obtained from http://perso.telecom-paristech.fr/~drossi/ccnSim. After having decompressed it in the $CCNSIM DIR folder, installation requires the following steps:
If all the steps described in the previous sections have been successfully executed, simulations of general cache networks using the parallel ModelGraft technique can now be launched. The several steps needed to simulate sample scenarios will be described in the following.
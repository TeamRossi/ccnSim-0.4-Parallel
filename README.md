# Instruction to install ccnSim-Parallel

In this section we will cover all the steps needed for the installation of ccnSim-Parallel. We suggest the reader to check and execute them by following the presentation order.

## Portability

ccnSim-Parallel has been tested on the following platforms, and with the following softwares:
* Ubuntu Linux 14.04 (64-bit)
* Ubuntu Linux 16.04 Server (64-bit)
* Omnetpp-4.6
* Omnetpp-5.0 • Akaroa-2.7.13

## Prerequisites
* Boost libraries ≥ 1.54

They can be installed either by using the standard packet manager of your system (e.g., apt-get install, yum install, port install, etc.), or by downloading them from http://www.boost.org/users/download/, and following instructions therein.

* gcc > 4.8.1

On Ubuntu platforms, gcc can be updated by adding the ubuntu-toolchain-r/test PPA. Sample commands for the latest 5.x version are:
    
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt update
    sudo apt install gcc-5 g++-5
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5

• parallel-ssh

In order to ease the simulation workflow, a passwordless SSH login needs to be present on all the available servers. parallel-ssh can than be used in order to send commands to slaves from the master node.

    sudo apt-get install pssh

## Akaroa2©

As mentioned above, Akaroa2© requires a specific license in order to be used. For instructions about software download and licenses, please visit the website https://akaroa.canterbury. ac.nz/akaroa/. ccnSim-Parallel has been developed starting from Akaroa-2.7.13. The whole set of modifications that have been made in order to implement the parallel ModelGraft strategy are grouped inside the Akaroa_2.7.13_for_PMG.patch.
Once obtained the original licensed Akaroa-2.7.13, place it in the same directory with the aforementioned patch, and type:

    patch -s -p0 < Akaroa_2.7.13_for_PMG.patch

Notice that the original Akaroa directory will keep the same name even after the application of the patch.
Once the patch is applied, type the following commands from the root directory of the patched Akaroa in order to install it:

    ./configure
    make
    sudo make install
    
Akaroa binaries, libraries, and include files will be installed in /usr/local/akaroa/bin, /usr/local/akaroa/lib, and /usr/local/akaroa/include, respectively. If you do not have sudo privileges, and you need to install Akaroa into another location, e.g., <install_path>, please add this location to your PATH variable, and use the following commands:

    ./configure --prefix=<install_path>
    make
    make install
    
Furthermore, always in the case of lacking sudo privileges, users might need to modify LIB and PYTHON paths in the several “Makefile.config_*” files and inside the “pyconfig.py” file, according to the customized locations of related libraries, before compiling and installing Akaroa.

## Omnet++

ccnSim-Parallel has been tested with both Omnetpp-4.6 and Omnetpp-5.0, which are available at https://omnetpp.org/. Before the installation, Omnetpp-x requires prerequisite packages to be installed. For the complete list, please read the official installation manual. Furthermore, since Omnetpp-x bin/ directory needs to be added to the PATH variable, the following line should be put at the end of the 􏰅/.bashrc file:

    export PATH=$PATH:$HOME/omnetpp-x/bin
    
After having closed and saved the file, please close and re-open the terminal.
Regardless of the Omnetpp version, a couple of patch files need to be applied to it before compilation and installation. These patch files are provided with ccnSim-Parallel, which we suppose being located in $CCNSIM_DIR. Provided that Omnetpp is located in $OMNET_DIR, specific instructions to patch, compile, and install it without the graphical interface (not needed for parallel ModelGraft simulations) are provided in the following, according to the selected version:

* Omnetpp-4.6

      pint:􏰅$ cd $CCNSIM_DIR
      pint:CCNSIM_DIR$ cp ./patch/omnet4x/ctopology.h $OMNET_DIR/include/ 
      pint:CCNSIM_DIR$ cp ./patch/omnet4x/ctopology.cc $OMNET_DIR/src/sim/ 
      pint:CCNSIM_DIR$ cd $OMNET_DIR/
      pint:OMNET_DIR$ NO_TCL=1 ./configure
      pint:OMNET_DIR$ make
￼
* Omnetpp-5.0

      pint:􏰅$ cd $CCNSIM_DIR
      pint:CCNSIM_DIR$ cp ./patch/omnet5x/ctopology.h $OMNET_DIR/include/omnetpp 
      pint:CCNSIM_DIR$ cp ./patch/omnet5x/ctopology.cc $OMNET_DIR/src/sim 
      pint:CCNSIM_DIR$ cd $OMNET_DIR/
      pint:OMNET_DIR$ ./configure WITH_TKENV=no
      pint:OMNET_DIR$ make

Compilation flags can also be set through the file configure.user (e.g., to disable Qtenv).

## ccnSim-Parallel

Installation requires the following commands from the $CCNSIM_DIR:

    ./scripts/makemake.sh
    make
    
If all the steps described in the previous sections have been successfully executed, simulations of general cache networks using the parallel ModelGraft technique can now be launched. 
Please refer to the Manual for details about the execution of sample scenarios.
